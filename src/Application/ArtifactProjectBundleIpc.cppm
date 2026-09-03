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
#include <memory>
#include <algorithm>

export module Artifact.Application.ProjectBundleIpc;

import Artifact.Application.Manager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.Clone;
import Artifact.Service.Project;
import Undo.UndoManager;
import Utils.Id;
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
    QStringList beforeSelectionIds;
    if (selectionManager) {
        for (const auto& selectedLayer : selectionManager->selectedLayers()) {
            if (selectedLayer) beforeSelectionIds.append(selectedLayer->id().toString());
        }
    }
    const QString beforeCurrentSelection = selectionManager && selectionManager->currentLayer()
        ? selectionManager->currentLayer()->id().toString()
        : QString();
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

    if (auto* undo = UndoManager::instance()) {
        const auto pastedId = [](const ArtifactAbstractLayerPtr& layer) {
            return layer ? layer->id().toString() : QString();
        };
        const auto liveLayers = comp->allLayer();
        QVector<ArtifactAbstractLayerPtr> orderedPastedLayers;
        QHash<QString, int> desiredIndices;
        for (int index = 0; index < liveLayers.size(); ++index) {
            const auto& layer = liveLayers[index];
            if (layer && pastedLayers.contains(layer)) {
                orderedPastedLayers.push_back(layer);
                desiredIndices.insert(pastedId(layer), index);
            }
        }
        if (orderedPastedLayers.size() != pastedLayers.size()) {
            for (const auto& layer : pastedLayers) {
                if (layer) comp->removeLayer(layer->id());
            }
            if (selectionManager) {
                selectionManager->clearSelection();
                for (const auto& layerId : beforeSelectionIds) {
                    auto layer = comp->layerById(LayerID(layerId));
                    if (layer) selectionManager->addToSelection(layer);
                }
            }
            comp->changed();
            return false;
        }
        for (const auto& layer : orderedPastedLayers) {
            comp->removeLayer(layer->id());
            if (comp->containsLayerById(layer->id())) {
                for (const auto& pastedLayer : orderedPastedLayers) {
                    if (pastedLayer && !comp->containsLayerById(pastedLayer->id())) {
                        comp->appendLayerBottom(pastedLayer);
                    }
                }
                if (selectionManager) {
                    selectionManager->clearSelection();
                    for (const auto& layerId : beforeSelectionIds) {
                        auto layer = comp->layerById(LayerID(layerId));
                        if (layer) selectionManager->addToSelection(layer);
                    }
                }
                comp->changed();
                return false;
            }
        }
        const auto restorePastedLayers = [&]() {
            for (const auto& layer : orderedPastedLayers) {
                if (layer && !comp->containsLayerById(layer->id())) {
                    comp->appendLayerBottom(layer);
                }
            }
            for (const auto& layer : orderedPastedLayers) {
                if (!layer) continue;
                const int targetIndex = desiredIndices.value(pastedId(layer), -1);
                const auto currentLayers = comp->allLayer();
                int currentIndex = -1;
                for (int index = 0; index < currentLayers.size(); ++index) {
                    if (currentLayers[index] && currentLayers[index]->id() == layer->id()) {
                        currentIndex = index;
                        break;
                    }
                }
                if (currentIndex >= 0 && targetIndex >= 0 && currentIndex != targetIndex) {
                    comp->moveLayerToIndex(layer->id(), targetIndex);
                }
            }
        };
        const auto restoreSelection = [&]() {
            if (!selectionManager) return;
            selectionManager->clearSelection();
            for (const auto& layer : orderedPastedLayers) {
                if (layer && comp->containsLayerById(layer->id())) {
                    selectionManager->addToSelection(layer);
                }
            }
        };
        auto macro = std::make_unique<MacroUndoCommand>(QStringLiteral("Paste Layers"));
        for (const auto& layer : orderedPastedLayers) {
            macro->addChild(std::make_unique<AddLayerCommand>(comp, layer, false));
        }
        const auto baseLayers = comp->allLayer();
        std::vector<QString> simulatedIds;
        simulatedIds.reserve(baseLayers.size() + orderedPastedLayers.size());
        for (const auto& layer : baseLayers) {
            if (layer) simulatedIds.push_back(layer->id().toString());
        }
        for (const auto& layer : orderedPastedLayers) {
            if (layer) simulatedIds.push_back(layer->id().toString());
        }
        for (const auto& layer : orderedPastedLayers) {
            if (!layer) continue;
            const QString id = pastedId(layer);
            const int targetIndex = desiredIndices.value(id, -1);
            const auto currentIt = std::find(simulatedIds.begin(), simulatedIds.end(), id);
            if (targetIndex < 0 || currentIt == simulatedIds.end()) continue;
            const int oldIndex = static_cast<int>(std::distance(simulatedIds.begin(), currentIt));
            const int boundedTarget = std::clamp(targetIndex, 0,
                static_cast<int>(simulatedIds.size()) - 1);
            if (oldIndex != boundedTarget) {
                macro->addChild(std::make_unique<MoveLayerIndexCommand>(
                    comp, layer, oldIndex, boundedTarget));
                const QString movedId = simulatedIds[oldIndex];
                simulatedIds.erase(simulatedIds.begin() + oldIndex);
                simulatedIds.insert(simulatedIds.begin() + boundedTarget, movedId);
            }
        }
        QStringList afterSelectionIds;
        for (const auto& layer : orderedPastedLayers) {
            if (layer) afterSelectionIds.append(layer->id().toString());
        }
        macro->addChild(std::make_unique<LayerSelectionSnapshotCommand>(
            comp, beforeSelectionIds, beforeCurrentSelection, afterSelectionIds,
            afterSelectionIds.isEmpty() ? QString() : afterSelectionIds.back()));
        const size_t undoCountBefore = undo->undoCount();
        if (!undo->push(std::move(macro))) {
            restorePastedLayers();
            restoreSelection();
            return false;
        }
        bool applied = true;
        const auto afterLayers = comp->allLayer();
        for (const auto& layer : orderedPastedLayers) {
            int actualIndex = -1;
            for (int index = 0; index < afterLayers.size(); ++index) {
                if (afterLayers[index] && afterLayers[index]->id() == layer->id()) {
                    actualIndex = index;
                    break;
                }
            }
            applied = applied && actualIndex == desiredIndices.value(pastedId(layer), -1);
        }
        if (!applied) {
            if (undo->undoCount() == undoCountBefore + 1) undo->undo();
            restorePastedLayers();
            restoreSelection();
            return false;
        }
    } else {
        comp->changed();
    }
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
    bool ok = false;
    if (auto* undo = UndoManager::instance()) {
        ok = undo->push(std::make_unique<AddProjectItemsCommand>(items));
    } else {
        ok = project->addProjectItemsFromJson(items, nullptr);
    }
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
    item[QStringLiteral("id")] = Id().toString();
    item[QStringLiteral("compositionJson")] = compositionJson;
    if (compositionJson.contains(QStringLiteral("id"))) {
        item[QStringLiteral("compositionId")] =
            compositionJson.value(QStringLiteral("id")).toString();
    }

    const QJsonArray items{item};
    bool ok = false;
    if (auto* undo = UndoManager::instance()) {
        ok = undo->push(std::make_unique<AddProjectItemsCommand>(
            items, nullptr,
            compositionJson.value(QStringLiteral("id")).toString()));
    } else {
        ok = project->addProjectItemsFromJson(items, nullptr);
    }
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

    QJsonObject item;
    item[QStringLiteral("type")] = QStringLiteral("composition");
    item[QStringLiteral("id")] = Id().toString();
    item[QStringLiteral("name")] = finalName;
    item[QStringLiteral("compositionId")] = composition->id().toString();
    item[QStringLiteral("compositionJson")] = compositionJson;
    const QJsonArray items{item};
    bool ok = false;
    if (auto* undo = UndoManager::instance()) {
        ok = undo->push(std::make_unique<AddProjectItemsCommand>(items));
    } else {
        ok = project->addProjectItemsFromJson(items, nullptr);
    }
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
