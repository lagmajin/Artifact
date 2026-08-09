module;
#include <QByteArray>
#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QDebug>
#include <QTimer>
#include <QWidget>
#include <algorithm>

export module Artifact.Application.ProjectBundleIpc;

import Artifact.Application.Manager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.Clone;
import Artifact.Service.Project;
import Artifact.Layers.Selection.Manager;
import Clipboard.ClipboardManager;
import Composition.ParametricComposition;

namespace Artifact {

namespace {
constexpr qsizetype kMaxBundleIpcResponseBytes = 32 * 1024 * 1024;
}

namespace {
constexpr auto kProjectBundleServerName =
    "ArtifactStudio.ProjectBundleBridge";

QLocalServer& projectBundleServer() {
    static QLocalServer server;
    return server;
}

QPointer<QWidget> g_mainWindow;
bool g_isHost = false;

void bringMainWindowToFront() {
    QWidget* mainWindow = g_mainWindow.data();
    if (!mainWindow) {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            if (!widget) {
                continue;
            }
            if (widget->objectName() == QStringLiteral("ArtifactMainWindow")) {
                mainWindow = widget;
                break;
            }
        }
    }
    if (!mainWindow) {
        return;
    }
    mainWindow->showNormal();
    mainWindow->raise();
    mainWindow->activateWindow();
}

bool pasteLayerBundle(const QJsonObject& bundle) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return false;
    }

    auto comp = svc->currentComposition().lock();
    if (!comp) {
        return false;
    }

    const QJsonArray layersArray = bundle.value(QStringLiteral("layers")).toArray();
    if (layersArray.isEmpty()) {
        return false;
    }

    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    ArtifactAbstractLayerPtr anchorLayer;
    int anchorIndex = -1;
    if (selectionManager) {
        anchorLayer = selectionManager->currentLayer();
        if (anchorLayer) {
            const auto layers = comp->allLayer();
            for (int i = 0; i < layers.size(); ++i) {
                if (layers[i] && layers[i]->id() == anchorLayer->id()) {
                    anchorIndex = i;
                    break;
                }
            }
        }
    }

    if (selectionManager) {
        selectionManager->clearSelection();
    }

    int pasted = 0;
    QHash<QString, LayerID> pastedLayerIdMap;
    QVector<ArtifactAbstractLayerPtr> pastedLayers;
    for (const auto& val : layersArray) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject sourceLayerObject = val.toObject();
        const QString sourceLayerId =
            sourceLayerObject.value(QStringLiteral("id")).toString().trimmed();
        QJsonObject layerObject = sourceLayerObject;
        layerObject.remove(QStringLiteral("id"));
        auto layer = ArtifactLayerFactory::createFromJson(layerObject);
        if (!layer) {
            continue;
        }

        layer->setLayerName(layer->layerName() + QStringLiteral(" (Copy)"));
        auto result = comp->appendLayerTop(layer);
        if (!result.success) {
            continue;
        }

        if (anchorIndex >= 0) {
            const auto layers = comp->allLayer();
            int pastedIndex = -1;
            for (int i = 0; i < layers.size(); ++i) {
                if (layers[i] && layers[i]->id() == layer->id()) {
                    pastedIndex = i;
                    break;
                }
            }
            const int targetIndex = std::clamp(
                anchorIndex + pasted, 0,
                std::max(0, static_cast<int>(layers.size()) - 1));
            if (pastedIndex >= 0 && pastedIndex != targetIndex) {
                comp->moveLayerToIndex(layer->id(), targetIndex);
            }
        }

        if (selectionManager) {
            selectionManager->addToSelection(layer);
        }
        pastedLayers.push_back(layer);
        if (!sourceLayerId.isEmpty() && !LayerID(sourceLayerId).isNil()) {
            pastedLayerIdMap.insert(sourceLayerId, layer->id());
        }
        ++pasted;
    }

    if (pasted == 0) {
        return false;
    }

    for (const auto &pastedLayer : pastedLayers) {
        if (!pastedLayer) {
            continue;
        }
        const auto parentId = pastedLayer->parentLayerId();
        const auto parentIt = pastedLayerIdMap.constFind(parentId.toString());
        if (parentIt != pastedLayerIdMap.constEnd()) {
            pastedLayer->setParentById(parentIt.value());
        }
        if (auto *cloneLayer = dynamic_cast<ArtifactCloneLayer *>(
                pastedLayer.get())) {
            auto cloneSettings = cloneLayer->cloneSettings();
            const auto sourceIt = pastedLayerIdMap.constFind(
                cloneSettings.sourceLayerId.toString());
            if (sourceIt != pastedLayerIdMap.constEnd()) {
                cloneSettings.sourceLayerId = sourceIt.value();
                cloneLayer->setCloneSettings(cloneSettings);
            }
        }
        auto matteReferences = pastedLayer->matteReferences();
        bool matteReferencesChanged = false;
        for (auto &matteReference : matteReferences) {
            const auto sourceIt = pastedLayerIdMap.constFind(
                matteReference.sourceLayerId.toString());
            if (sourceIt != pastedLayerIdMap.constEnd()) {
                matteReference.sourceLayerId = sourceIt.value();
                matteReferencesChanged = true;
            }
        }
        if (matteReferencesChanged) {
            pastedLayer->setMatteReferences(matteReferences);
        }
    }

    comp->changed();
    if (auto project = svc->getCurrentProjectSharedPtr()) {
        project->projectChanged();
    }
    return true;
}

bool pasteProjectItemsBundle(const QJsonObject& bundle) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return false;
    }
    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }
    const QJsonArray items = bundle.value(QStringLiteral("items")).toArray();
    if (items.isEmpty()) {
        return false;
    }
    const bool ok = project->addProjectItemsFromJson(items, nullptr);
    if (ok) {
        project->projectChanged();
    }
    return ok;
}

bool pasteParametricCompositionBundle(const QJsonObject& bundle) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return false;
    }
    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }

    const auto parsed = ArtifactCore::ParametricCompositionBundle::fromJson(bundle);
    const QJsonObject compositionJson =
        ArtifactCore::parametricCompositionBundleToCompositionJson(parsed);
    if (compositionJson.isEmpty()) {
        return false;
    }

    QJsonObject item;
    item[QStringLiteral("type")] = QStringLiteral("composition");
    item[QStringLiteral("name")] = parsed.bundleTitle.isEmpty()
        ? QStringLiteral("Parametric Composition")
        : parsed.bundleTitle;
    item[QStringLiteral("compositionJson")] = compositionJson;
    if (compositionJson.contains(QStringLiteral("id"))) {
        item[QStringLiteral("compositionId")] =
            compositionJson.value(QStringLiteral("id")).toString();
    }

    const QJsonArray items{item};
    const bool ok = project->addProjectItemsFromJson(items, nullptr);
    if (ok) {
        project->projectChanged();
    }
    return ok;
}

bool pasteCompositionBundle(const QJsonObject& bundle) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return false;
    }
    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        return false;
    }

    const QJsonObject compositionJson = bundle.value(QStringLiteral("composition")).toObject();
    if (compositionJson.isEmpty()) {
        return false;
    }

    auto composition = ArtifactAbstractComposition::fromJson(QJsonDocument(compositionJson));
    if (!composition) {
        return false;
    }

    const QString sourceName = compositionJson.value(QStringLiteral("name")).toString();
    const QString finalName = bundle.value(QStringLiteral("bundleTitle")).toString().trimmed().isEmpty()
        ? (sourceName.trimmed().isEmpty() ? QStringLiteral("Composition") : sourceName)
        : bundle.value(QStringLiteral("bundleTitle")).toString().trimmed();

    if (project->hasComposition(composition->id())) {
        return false;
    }

    const bool ok = project->addImportedComposition(composition, finalName);
    if (ok) {
        project->setCurrentCompositionId(composition->id(), false);
        project->projectChanged();
    }
    return ok;
}

bool applyProjectBundleLocally(const QJsonObject& bundle) {
    const QString bundleKind = bundle.value(QStringLiteral("bundleKind")).toString();
    if (bundleKind == QStringLiteral("parametric-composition")) {
        return pasteParametricCompositionBundle(bundle);
    }
    if (bundleKind == QStringLiteral("composition")) {
        return pasteCompositionBundle(bundle);
    }
    if (bundleKind == QStringLiteral("layer")) {
        return pasteLayerBundle(bundle);
    }
    if (bundleKind == QStringLiteral("project-items")) {
        return pasteProjectItemsBundle(bundle);
    }
    if (bundle.contains(QStringLiteral("layers"))) {
        return pasteLayerBundle(bundle);
    }
    if (bundle.contains(QStringLiteral("items"))) {
        return pasteProjectItemsBundle(bundle);
    }
    if (bundle.contains(QStringLiteral("definition")) ||
        bundle.contains(QStringLiteral("instance"))) {
        return pasteParametricCompositionBundle(bundle);
    }
    return false;
}

QByteArray makeWirePayload(const QJsonObject& request) {
    const QByteArray json = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << json;
    return payload;
}

bool sendWirePayload(const QByteArray& payload, QString* error) {
    QLocalSocket socket;
    socket.connectToServer(QString::fromLatin1(kProjectBundleServerName));
    if (!socket.waitForConnected(1000)) {
        if (error) {
            *error = socket.errorString();
        }
        return false;
    }

    if (socket.write(payload) != payload.size()) {
        if (error) {
            *error = socket.errorString();
        }
        socket.abort();
        return false;
    }

    if (!socket.waitForBytesWritten(1000)) {
        if (error) {
            *error = socket.errorString();
        }
        socket.abort();
        return false;
    }

    if (!socket.waitForReadyRead(1500)) {
        if (error) {
            *error = socket.errorString().isEmpty()
                        ? QStringLiteral("IPC response timed out")
                        : socket.errorString();
        }
        socket.abort();
        return false;
    }

    QByteArray responseBytes = socket.readAll();
    if (responseBytes.size() <= 0 ||
        responseBytes.size() > kMaxBundleIpcResponseBytes) {
        if (error) {
            *error = QStringLiteral("IPC response is too large");
        }
        socket.disconnectFromServer();
        return false;
    }
    QBuffer responseBuffer(&responseBytes);
    responseBuffer.open(QIODevice::ReadOnly);
    QDataStream in(&responseBuffer);
    in.setVersion(QDataStream::Qt_6_5);
    QByteArray responseJson;
    in >> responseJson;
    if (responseJson.size() <= 0 ||
        responseJson.size() > kMaxBundleIpcResponseBytes) {
        if (error) {
            *error = QStringLiteral("IPC response JSON is too large");
        }
        socket.disconnectFromServer();
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid IPC response");
        }
        socket.disconnectFromServer();
        return false;
    }
    const QJsonObject response = responseDoc.object();
    const bool ok = response.value(QStringLiteral("ok")).toBool(false);
    if (!ok && error) {
        *error = response.value(QStringLiteral("error")).toString();
    }
    socket.disconnectFromServer();
    return ok;
}

} // namespace

export void initializeProjectBundleIpc(QObject* mainWindow) {
    g_mainWindow = qobject_cast<QWidget*>(mainWindow);
    auto& server = projectBundleServer();
    if (server.isListening()) {
        g_isHost = true;
        return;
    }

    g_isHost = server.listen(QString::fromLatin1(kProjectBundleServerName));
    if (!g_isHost) {
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(kProjectBundleServerName));
        if (!probe.waitForConnected(100)) {
            QLocalServer::removeServer(QString::fromLatin1(kProjectBundleServerName));
            g_isHost = server.listen(QString::fromLatin1(kProjectBundleServerName));
        }
        if (!g_isHost) {
            qWarning() << "[ProjectBundleIpc] Failed to listen on"
                       << kProjectBundleServerName << server.errorString();
        }
        return;
    }

    QObject::connect(&server, &QLocalServer::newConnection, &server, [&server]() {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            if (!socket) {
                continue;
            }
            socket->setParent(&server);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket]() {
                QDataStream in(socket);
                in.setVersion(QDataStream::Qt_6_5);
                QByteArray requestBytes;
                in >> requestBytes;
                if (requestBytes.isEmpty()) {
                    return;
                }

                QJsonParseError parseError{};
                const QJsonDocument requestDoc =
                    QJsonDocument::fromJson(requestBytes, &parseError);
                QJsonObject response;
                if (parseError.error != QJsonParseError::NoError || !requestDoc.isObject()) {
                    response[QStringLiteral("ok")] = false;
                    response[QStringLiteral("error")] = QStringLiteral("Invalid request");
                } else {
                    const QJsonObject request = requestDoc.object();
                    const QJsonObject bundle = request.value(QStringLiteral("bundle")).toObject();
                    const bool ok = applyProjectBundleLocally(bundle);
                    response[QStringLiteral("ok")] = ok;
                    if (!ok) {
                        response[QStringLiteral("error")] = QStringLiteral("Bundle could not be applied");
                    } else {
                        bringMainWindowToFront();
                    }
                }

                const QByteArray responseBytes =
                    QJsonDocument(response).toJson(QJsonDocument::Compact);
                QByteArray wireResponse;
                QDataStream out(&wireResponse, QIODevice::WriteOnly);
                out.setVersion(QDataStream::Qt_6_5);
                out << responseBytes;
                socket->write(wireResponse);
                socket->flush();
                socket->disconnectFromServer();
                socket->deleteLater();
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket,
                             &QObject::deleteLater);
        }
    });
}

export bool isProjectBundleIpcHost() {
    return g_isHost;
}

export bool sendProjectBundleToMainProject(const QJsonObject& bundle, QString* error) {
    if (bundle.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty bundle");
        }
        return false;
    }

    if (g_isHost) {
        const bool ok = applyProjectBundleLocally(bundle);
        if (ok) {
            bringMainWindowToFront();
        } else if (error) {
            *error = QStringLiteral("Bundle could not be applied locally");
        }
        if (!ok) {
            ClipboardManager::instance().copyProjectBundle(
                bundle, bundle.value(QStringLiteral("bundleTitle")).toString());
        }
        return ok;
    }

    QJsonObject request;
    request[QStringLiteral("action")] = QStringLiteral("send-project-bundle");
    request[QStringLiteral("bundle")] = bundle;
    const QByteArray payload = makeWirePayload(request);
    const bool ok = sendWirePayload(payload, error);
    if (!ok) {
        ClipboardManager::instance().copyProjectBundle(
            bundle, bundle.value(QStringLiteral("bundleTitle")).toString());
    }
    return ok;
}

export bool applyProjectBundleToCurrentProject(const QJsonObject& bundle, QString* error) {
    const bool ok = applyProjectBundleLocally(bundle);
    if (!ok && error) {
        *error = QStringLiteral("Bundle could not be applied");
    }
    return ok;
}

} // namespace Artifact
