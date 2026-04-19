module;
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Project.Importer;

import Utils.String.UniString;
import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Render.Queue.Service;

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

   // Version compatibility check
   QString fileVersion = root.contains("version") && root["version"].isString()
    ? root["version"].toString()
    : QStringLiteral("1.0");
   QString minVersion = root.contains("minVersion") && root["minVersion"].isString()
    ? root["minVersion"].toString()
    : fileVersion;

   // Current supported version
   static const QString kCurrentVersion = "1.1";
   static const QString kMinSupportedVersion = "1.0";

   // Check if file version is too new
   auto versionToFloat = [](const QString& v) -> float {
    bool ok = false;
    float val = v.toFloat(&ok);
    return ok ? val : 0.0f;
   };

   float fileVer = versionToFloat(fileVersion);
   float minSupportedVer = versionToFloat(kMinSupportedVersion);
   float currentVer = versionToFloat(kCurrentVersion);

   if (fileVer > currentVer) {
    qWarning() << "[Importer] File version" << fileVersion << "is newer than supported" << kCurrentVersion;
    result.errorMessage = UniString(QStringLiteral("Project file version %1 is newer than supported version %2. Please update the application.").arg(fileVersion, kCurrentVersion).toStdString());
    return result;
   }

   // Check if minVersion requirement is satisfied
   float minReqVer = versionToFloat(minVersion);
   if (minReqVer > currentVer) {
    qWarning() << "[Importer] File requires minimum version" << minVersion << "but current is" << kCurrentVersion;
    result.errorMessage = UniString(QStringLiteral("Project file requires minimum version %1 but current version is %2").arg(minVersion, kCurrentVersion).toStdString());
    return result;
   }

   qDebug() << "[Importer] Version check passed - file version:" << fileVersion << "min version:" << minVersion;
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

  // Render queue restoration
  if (root.contains("renderQueue") && root["renderQueue"].isArray()) {
   if (auto* rqService = ArtifactRenderQueueService::instance()) {
    rqService->fromJson(root["renderQueue"].toArray());
    qDebug() << "[Importer] Render queue restored:" << rqService->jobCount() << "jobs";
   }
  }

  // Project items (Footage, Folder, etc.) restoration
  if (root.contains("projectItems") && root["projectItems"].isArray()) {
   QJsonArray projectItemsArray = root["projectItems"].toArray();
   projectPtr->restoreProjectItems(projectItemsArray);
   qDebug() << "[Importer] Project items restored:" << projectItemsArray.size() << "top-level items";
  }

  // 現在選択中のコンポジションを復元
  if (root.contains("currentCompositionId") && root["currentCompositionId"].isString()) {
   const QString currentCompositionIdStr = root["currentCompositionId"].toString().trimmed();
   if (!currentCompositionIdStr.isEmpty()) {
    projectPtr->setCurrentCompositionId(CompositionID(currentCompositionIdStr), false);
    qDebug() << "[Importer] Current composition restored:" << currentCompositionIdStr;
   }
  }

  // 健康状態のチェックと自動修復の実行
  ArtifactProjectHealthChecker::checkAndRepair(projectPtr.get(), AutoRepairOptions{
      true, // repairFrameRanges
      false, // removeMissingAssets (アセットは後で再リンク可能にするため残すのが一般的)
      true, // normalizeCompositionRanges
      true  // removeBrokenReferences (存在しないコンポジションへの参照などは削除)
  });
  
  result.healthReport = ArtifactProjectHealthChecker::check(projectPtr.get());
  if (!result.healthReport.isHealthy) {
      qWarning() << "[Importer] Project health check failed with issues:";
      for (const auto& issue : result.healthReport.issues) {
          qWarning() << "  [" << issue.category << "]" << issue.targetName << ":" << issue.message;
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
