module;
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
module Artifact.Project.Importer;

import std;
import Utils.String.UniString;
import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;

namespace Artifact
{

 class ArtifactProjectImporter::Impl {
 private:
  QString inputPath_;

  QJsonDocument loadJsonDocument(const QString& path, UniString& errorMsg);

 public:
  Impl();
  ~Impl();
  void setInputPath(const QString& path);
  ArtifactProjectImporterResult importProject();
  bool validateFile(const QString& path);
  UniString getFileFormatVersion(const QString& path);
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

 QJsonDocument ArtifactProjectImporter::Impl::loadJsonDocument(const QString& path, UniString& errorMsg)
 {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
   errorMsg = UniString("Failed to open file: " + file.errorString().toStdString());
   return QJsonDocument();
  }

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
   errorMsg = UniString("JSON parse error: " + parseError.errorString().toStdString());
   return QJsonDocument();
  }

  if (!doc.isObject()) {
   errorMsg = UniString("Invalid JSON: root is not an object");
   return QJsonDocument();
  }

  return doc;
 }

 bool ArtifactProjectImporter::Impl::validateFile(const QString& path)
 {
  UniString errorMsg;
  QJsonDocument doc = loadJsonDocument(path, errorMsg);
  if (doc.isNull()) {
   qDebug() << "[Importer] Validation failed:" << errorMsg.toQString();
   return false;
  }

  QJsonObject root = doc.object();
  if (!root.contains("name")) {
   qDebug() << "[Importer] Validation failed: missing 'name' field";
   return false;
  }

  qDebug() << "[Importer] File validated successfully";
  return true;
 }

 UniString ArtifactProjectImporter::Impl::getFileFormatVersion(const QString& path)
 {
  UniString errorMsg;
  QJsonDocument doc = loadJsonDocument(path, errorMsg);
  if (doc.isNull()) {
   return UniString("unknown");
  }

  QJsonObject root = doc.object();
  if (root.contains("version") && root["version"].isString()) {
   return UniString(root["version"].toString().toStdString());
  }

  return UniString("1.0");
 }

 ArtifactProjectImporterResult ArtifactProjectImporter::Impl::importProject()
 {
  ArtifactProjectImporterResult result;
  result.success = false;
  result.project = nullptr;
  result.compositionsLoaded = 0;
  result.layersLoaded = 0;

  if (inputPath_.isEmpty()) {
   result.errorMessage = UniString("Input path is empty");
   qDebug() << "[Importer] Error: Input path is empty";
   return result;
  }

  // JSONドキュメントを読み込む
  UniString errorMsg;
  QJsonDocument doc = loadJsonDocument(inputPath_, errorMsg);
  if (doc.isNull()) {
   result.errorMessage = errorMsg;
   qDebug() << "[Importer] Error loading document:" << errorMsg.toQString();
   return result;
  }

  QJsonObject root = doc.object();
  auto projectPtr = std::make_shared<ArtifactProject>();

  // プロジェクト基本情報の読み込み
  if (root.contains("name") && root["name"].isString()) {
   projectPtr->setProjectName(root["name"].toString());
   qDebug() << "[Importer] Project name:" << root["name"].toString();
  }

  if (root.contains("author") && root["author"].isString()) {
   projectPtr->setAuthor(root["author"].toString());
   qDebug() << "[Importer] Author:" << root["author"].toString();
  }

  // AI向けメタデータの読み込み
  if (root.contains("ai_description") && root["ai_description"].isString()) {
   projectPtr->setAIDescription(root["ai_description"].toString());
   qDebug() << "[Importer] AI Description:" << root["ai_description"].toString();
  }

  if (root.contains("ai_tags") && root["ai_tags"].isArray()) {
   QStringList tags;
   QJsonArray tagsArray = root["ai_tags"].toArray();
   for (const auto& tagVal : tagsArray) {
    if (tagVal.isString()) {
     tags.append(tagVal.toString());
    }
   }
   projectPtr->setAITags(tags);
   qDebug() << "[Importer] AI Tags:" << tags;
  }

  if (root.contains("ai_notes") && root["ai_notes"].isString()) {
   projectPtr->setAINotes(root["ai_notes"].toString());
   qDebug() << "[Importer] AI Notes:" << root["ai_notes"].toString();
  }

  // エクスポーターが追加したAIメタデータセクション
  if (root.contains("_ai_metadata") && root["_ai_metadata"].isObject()) {
   QJsonObject aiMeta = root["_ai_metadata"].toObject();
   if (aiMeta.contains("_ai_export_timestamp")) {
    qDebug() << "[Importer] Export Timestamp:" << aiMeta["_ai_export_timestamp"].toString();
   }
   if (aiMeta.contains("project_summary")) {
    QJsonObject summary = aiMeta["project_summary"].toObject();
    if (summary.contains("composition_count")) {
     qDebug() << "[Importer] AI Metadata - Composition Count:" << summary["composition_count"].toInt();
    }
   }
  }

  // コンポジションの読み込み
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
    int layersInComp = 0;

    // レイヤーの読み込み
    if (compObj.contains("layers") && compObj["layers"].isArray()) {
     QJsonArray layersArray = compObj["layers"].toArray();
     for (const auto& layerVal : layersArray) {
      if (layerVal.isObject()) {
       auto layer = ArtifactAbstractLayer::fromJson(layerVal.toObject());
       if (layer) {
        compPtr->appendLayerTop(layer);
        layersInComp++;
        result.layersLoaded++;
       }
      }
     }
    }

    projectPtr->addImportedComposition(compPtr, compName);
    result.compositionsLoaded++;
    qDebug() << "[Importer] Loaded composition:" << compName << "with" << layersInComp << "layers";
   }
  }

  result.project = projectPtr;
  result.success = true;
  qDebug() << "[Importer] Project imported successfully - Compositions:" 
           << result.compositionsLoaded << "Layers:" << result.layersLoaded;

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

 bool ArtifactProjectImporter::validateFile(const QString& path)
 {
  return impl_->validateFile(path);
 }

 UniString ArtifactProjectImporter::getFileFormatVersion(const QString& path)
 {
  return impl_->getFileFormatVersion(path);
 }

};
