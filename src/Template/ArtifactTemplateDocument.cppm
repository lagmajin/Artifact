module;
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QRegularExpression>
#include <QFileInfo>

module Artifact.Template.Document;

import Artifact.Layer.Abstract;

namespace Artifact {

ArtifactTemplateLibrary::ArtifactTemplateLibrary(const QString& directoryPath)
    : directoryPath_(directoryPath.trimmed()) {
    if (directoryPath_.isEmpty()) {
        directoryPath_ = QDir::home().filePath(QStringLiteral("Artifact/Templates"));
    }
}

QString ArtifactTemplateLibrary::directoryPath() const {
    return directoryPath_;
}

QStringList ArtifactTemplateLibrary::templateFiles() const {
    QDir directory(directoryPath_);
    return directory.entryList({QStringLiteral("*.artemplate")},
                               QDir::Files | QDir::Readable, QDir::Name);
}

bool ArtifactTemplateLibrary::save(const ArtifactTemplateDocument& document,
                                   QString* errorMessage) const {
    QDir directory(directoryPath_);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (errorMessage) *errorMessage = QStringLiteral("Unable to create template directory");
        return false;
    }
    QString fileName = document.name.trimmed();
    if (fileName.isEmpty()) fileName = document.id.trimmed();
    if (fileName.isEmpty()) fileName = QStringLiteral("Untitled");
    fileName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                     QStringLiteral("_"));
    if (!fileName.endsWith(QStringLiteral(".artemplate"), Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".artemplate");
    }
    return document.saveToFile(directory.filePath(fileName), errorMessage);
}

ArtifactTemplateDocument ArtifactTemplateLibrary::load(
    const QString& fileName, QString* errorMessage) const {
    const QString safeName = QFileInfo(fileName).fileName();
    if (!safeName.endsWith(QStringLiteral(".artemplate"), Qt::CaseInsensitive)) {
        if (errorMessage) *errorMessage = QStringLiteral("Not an .artemplate file");
        return {};
    }
    return ArtifactTemplateDocument::loadFromFile(
        QDir(directoryPath_).filePath(safeName), errorMessage);
}

bool ArtifactTemplateLibrary::remove(const QString& fileName) const {
    const QString safeName = QFileInfo(fileName).fileName();
    return safeName.endsWith(QStringLiteral(".artemplate"), Qt::CaseInsensitive) &&
           QFile::remove(QDir(directoryPath_).filePath(safeName));
}

QJsonObject ArtifactTemplateDocument::toJson() const {
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), 1);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("description"), description);
    object.insert(QStringLiteral("sourceLayerIds"), QJsonArray::fromStringList(sourceLayerIds));
    object.insert(QStringLiteral("exposedParameters"), exposedParameters);
    object.insert(QStringLiteral("layerSnapshots"), layerSnapshots);
    object.insert(QStringLiteral("metadata"), metadata);
    object.insert(QStringLiteral("templateLock"), templateLock.toJson());
    return object;
}

int ArtifactTemplateDocument::appendToComposition(
    ArtifactAbstractComposition& composition) const {
    int appended = 0;
    const auto layers = instantiateLayers();
    for (auto it = layers.crbegin(); it != layers.crend(); ++it) {
        if (*it && composition.appendLayerTop(*it).success) ++appended;
    }
    return appended;
}

ArtifactTemplateDocument ArtifactTemplateDocument::fromJson(const QJsonObject& object) {
    ArtifactTemplateDocument document;
    document.id = object.value(QStringLiteral("id")).toString().trimmed();
    document.name = object.value(QStringLiteral("name")).toString().trimmed();
    document.description = object.value(QStringLiteral("description")).toString();
    for (const auto& value : object.value(QStringLiteral("sourceLayerIds")).toArray()) {
        const QString layerId = value.toString().trimmed();
        if (!layerId.isEmpty()) document.sourceLayerIds.push_back(layerId);
    }
    document.exposedParameters = object.value(QStringLiteral("exposedParameters")).toArray();
    document.layerSnapshots = object.value(QStringLiteral("layerSnapshots")).toArray();
    document.metadata = object.value(QStringLiteral("metadata")).toObject();
    document.templateLock = ArtifactCore::TemplateLockSchema::fromJson(
        object.value(QStringLiteral("templateLock")).toObject());
    return document;
}

ArtifactTemplateDocument ArtifactTemplateDocument::fromLayers(
    const QVector<ArtifactAbstractLayerPtr>& layers, const QString& templateName) {
    ArtifactTemplateDocument document;
    document.name = templateName.trimmed();
    for (const auto& layer : layers) {
        if (!layer) continue;
        const QString layerId = layer->id().toString();
        const QJsonObject snapshot = layer->toJson();
        document.addLayerSnapshot(layerId, snapshot);
        for (const auto& value : snapshot.value(QStringLiteral("masterProperties")).toArray()) {
            if (!value.isObject()) continue;
            QJsonObject parameter = value.toObject();
            parameter.insert(QStringLiteral("sourceLayerId"), layerId);
            document.addExposedParameter(parameter);
        }
    }
    return document;
}

QVector<ArtifactAbstractLayerPtr> ArtifactTemplateDocument::instantiateLayers() const {
    QVector<ArtifactAbstractLayerPtr> layers;
    layers.reserve(layerSnapshots.size());
    for (const auto& value : layerSnapshots) {
        if (!value.isObject()) continue;
        const auto layer = ArtifactAbstractLayer::fromJson(value.toObject());
        if (layer) layers.push_back(layer);
    }
    return layers;
}

void ArtifactTemplateDocument::addLayerSnapshot(const QString& layerId,
                                                const QJsonObject& snapshot) {
    const QString normalizedId = layerId.trimmed();
    if (normalizedId.isEmpty()) return;
    if (!sourceLayerIds.contains(normalizedId)) sourceLayerIds.push_back(normalizedId);

    QJsonObject entry = snapshot;
    entry.insert(QStringLiteral("sourceLayerId"), normalizedId);
    layerSnapshots.push_back(entry);
}

void ArtifactTemplateDocument::addExposedParameter(const QJsonObject& parameter) {
    const QString path = parameter.value(QStringLiteral("path")).toString().trimmed();
    const QString id = parameter.value(QStringLiteral("id")).toString().trimmed();
    const QString key = !path.isEmpty() ? path : id;
    if (key.isEmpty()) return;
    for (const auto& value : exposedParameters) {
        const auto object = value.toObject();
        const QString existing = object.value(QStringLiteral("path")).toString().trimmed();
        const QString existingId = object.value(QStringLiteral("id")).toString().trimmed();
        if ((!existing.isEmpty() ? existing : existingId) == key) return;
    }
    exposedParameters.push_back(parameter);
}

bool ArtifactTemplateDocument::saveToFile(const QString& filePath,
                                          QString* errorMessage) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    const QByteArray data = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

ArtifactTemplateDocument ArtifactTemplateDocument::loadFromFile(
    const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!json.isObject()) {
        if (errorMessage) *errorMessage = parseError.errorString();
        return {};
    }
    return fromJson(json.object());
}

} // namespace Artifact
