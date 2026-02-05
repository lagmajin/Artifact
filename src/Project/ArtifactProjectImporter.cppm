module;
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
module Artifact.Project.Importer;

import std;
import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;

namespace Artifact
{

 class ArtifactProjectImporter::Impl {
 private:
  QString inputPath_;
 public:
  Impl();
  ~Impl();
  void setInputPath(const QString& path);
  ArtifactProjectImporterResult importProject();
 };

 ArtifactProjectImporter::Impl::Impl()
 {
 }

 ArtifactProjectImporter::Impl::~Impl()
 {
 }

 void ArtifactProjectImporter::Impl::setInputPath(const QString& path)
 {
  inputPath_ = path;
 }

 ArtifactProjectImporterResult ArtifactProjectImporter::Impl::importProject()
 {
  ArtifactProjectImporterResult result;
  result.success = false;
  result.project = nullptr;

  if (inputPath_.isEmpty()) return result;

  QFile file(inputPath_);
  if (!file.open(QIODevice::ReadOnly)) return result;

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
   return result;
  }

  QJsonObject root = doc.object();
  auto projectPtr = std::make_shared<ArtifactProject>();

  if (root.contains("name") && root["name"].isString()) {
   projectPtr->setProjectName(root["name"].toString());
  }
  if (root.contains("author") && root["author"].isString()) {
   projectPtr->setAuthor(root["author"].toString());
  }

  if (root.contains("compositions") && root["compositions"].isArray()) {
   QJsonArray compsArray = root["compositions"].toArray();
   for (const auto& compVal : compsArray) {
    if (!compVal.isObject()) continue;
    QJsonObject compObj = compVal.toObject();
    if (!compObj.contains("id")) continue;

    QString idStr = compObj["id"].toString();
    CompositionID compId(idStr);
    ArtifactCompositionInitParams params;
    QString compName = compObj.contains("name") && compObj["name"].isString()
     ? compObj["name"].toString()
     : QStringLiteral("Composition");

    auto compPtr = std::make_shared<ArtifactAbstractComposition>(compId, params);

    if (compObj.contains("layers") && compObj["layers"].isArray()) {
     QJsonArray layersArray = compObj["layers"].toArray();
     for (const auto& layerVal : layersArray) {
      if (layerVal.isObject()) {
       auto layer = ArtifactAbstractLayer::fromJson(layerVal.toObject());
       if (layer) {
        compPtr->appendLayerTop(layer);
       }
      }
     }
    }

    projectPtr->addImportedComposition(compPtr, compName);
   }
  }

  result.project = projectPtr;
  result.success = true;
  return result;
 }

 ArtifactProjectImporter::ArtifactProjectImporter() : impl_(new Impl())
 {
 }

 ArtifactProjectImporter::~ArtifactProjectImporter()
 {
  delete impl_;
 }

 void ArtifactProjectImporter::setInputPath(const QString& path)
 {
  impl_->setInputPath(path);
 }

 ArtifactProjectImporterResult ArtifactProjectImporter::importProject()
 {
  return impl_->importProject();
 }

};
