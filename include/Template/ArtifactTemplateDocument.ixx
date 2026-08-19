module;
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

export module Artifact.Template.Document;

import Composition.TemplateLock;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;

export namespace Artifact {

class ArtifactTemplateDocument {
public:
    QString id;
    QString name;
    QString description;
    QStringList sourceLayerIds;
    QJsonArray exposedParameters;
    QJsonArray layerSnapshots;
    QJsonObject metadata;
    ArtifactCore::TemplateLockSchema templateLock;

    QJsonObject toJson() const;
    static ArtifactTemplateDocument fromJson(const QJsonObject& object);
    static ArtifactTemplateDocument fromLayers(
        const QVector<ArtifactAbstractLayerPtr>& layers,
        const QString& templateName = {});
    QVector<ArtifactAbstractLayerPtr> instantiateLayers() const;
    int appendToComposition(ArtifactAbstractComposition& composition) const;

    void addLayerSnapshot(const QString& layerId, const QJsonObject& snapshot);
    void addExposedParameter(const QJsonObject& parameter);

    bool saveToFile(const QString& filePath, QString* errorMessage = nullptr) const;
    static ArtifactTemplateDocument loadFromFile(const QString& filePath,
                                                 QString* errorMessage = nullptr);
};

class ArtifactTemplateLibrary {
public:
    explicit ArtifactTemplateLibrary(const QString& directoryPath = {});

    QString directoryPath() const;
    QStringList templateFiles() const;
    bool save(const ArtifactTemplateDocument& document,
              QString* errorMessage = nullptr) const;
    ArtifactTemplateDocument load(const QString& fileName,
                                  QString* errorMessage = nullptr) const;
    bool remove(const QString& fileName) const;

private:
    QString directoryPath_;
};

} // namespace Artifact
