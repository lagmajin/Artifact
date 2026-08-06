module;
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
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
import Artifact.Project.CreationDefaults;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Asset.Manager;
import Memory.SharedPtr;
import Artifact.Color.OCIOManager;
import Serialization.ProjectSerializer;

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
  inputPath_ = path.trimmed();
 }

 QJsonDocument ArtifactProjectImporter::Impl::loadJsonDocument(const QString& path, UniString& errorMsg)
 {
  const QString normalizedPath = path.trimmed();
  if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
   errorMsg = UniString("Project file path is invalid");
   return QJsonDocument();
  }
  const QFileInfo pathInfo(normalizedPath);
  if (pathInfo.isDir()) {
   QJsonObject manifest;
   QMap<QString, QJsonObject> documents;
   if (!ArtifactCore::Serialization::ProjectSerializer::loadSplit(
           normalizedPath, manifest, documents)) {
    errorMsg = UniString("Failed to read split project documents");
    return QJsonDocument();
   }
   QString rootDocument = manifest.value(QStringLiteral("rootDocument")).toString();
   if (rootDocument.isEmpty()) {
    rootDocument = QStringLiteral("project");
   }
   if (!documents.contains(rootDocument)) {
    errorMsg = UniString("Split project root document is missing");
    return QJsonDocument();
   }
   QJsonObject root = documents.value(rootDocument);
   QJsonArray compositions = root.value(QStringLiteral("compositions")).toArray();
   for (int index = 0; index < compositions.size(); ++index) {
    const QJsonObject reference = compositions.at(index).toObject();
    const QString documentName = reference.value(QStringLiteral("$document")).toString();
    if (documentName.isEmpty()) {
     continue;
    }
    if (!documents.contains(documentName)) {
     errorMsg = UniString("Split project composition document is missing");
     return QJsonDocument();
    }
    compositions[index] = documents.value(documentName);
   }
   if (!compositions.isEmpty()) {
    root.insert(QStringLiteral("compositions"), compositions);
   }
   return QJsonDocument(root);
  }
  if (!pathInfo.isFile()) {
   errorMsg = UniString("Project file path is invalid");
   return QJsonDocument();
  }
  QFile file(normalizedPath);
  if (!file.open(QIODevice::ReadOnly)) {
   errorMsg = UniString("Failed to open file: " + file.errorString().toStdString());
   return QJsonDocument();
  }

  constexpr qint64 kMaxProjectFileBytes = 256LL * 1024LL * 1024LL;
  const qint64 projectFileBytes = file.size();
  if (projectFileBytes < 0 || projectFileBytes > kMaxProjectFileBytes) {
   file.close();
   errorMsg = UniString("Project file is too large");
   return QJsonDocument();
  }

  file.close();

  QJsonObject object;
  QString serializationError;
  if (!ArtifactCore::Serialization::ProjectSerializer::load(
          normalizedPath, object, &serializationError)) {
   errorMsg = UniString("Failed to read project document: " +
                        serializationError.toStdString());
   return QJsonDocument();
  }

  return QJsonDocument(object);
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
  const QJsonValue colorPipelineValue = root.value(QStringLiteral("colorPipelineVersion"));
  const double rawColorPipelineVersion = colorPipelineValue.isUndefined()
      ? static_cast<double>(ArtifactProject::LegacyColorPipelineVersion)
      : colorPipelineValue.toDouble();
  if ((!colorPipelineValue.isUndefined() && !colorPipelineValue.isDouble()) ||
      std::floor(rawColorPipelineVersion) != rawColorPipelineVersion) {
   result.errorMessage = UniString("Project colorPipelineVersion must be an integer");
   qWarning() << "[Importer] Invalid colorPipelineVersion" << colorPipelineValue;
   return result;
  }
  if (rawColorPipelineVersion < ArtifactProject::LegacyColorPipelineVersion ||
      rawColorPipelineVersion > ArtifactProject::CanonicalColorPipelineVersion) {
   result.errorMessage = UniString(
       QStringLiteral("Project color pipeline version %1 is not supported (supported: %2-%3)")
           .arg(rawColorPipelineVersion)
           .arg(ArtifactProject::LegacyColorPipelineVersion)
           .arg(ArtifactProject::CanonicalColorPipelineVersion)
           .toStdString());
   qWarning() << "[Importer] Unsupported colorPipelineVersion" << rawColorPipelineVersion;
   return result;
  }
  const int colorPipelineVersion = static_cast<int>(rawColorPipelineVersion);
  auto projectPtr = ArtifactCore::makeShared<ArtifactProject>();
  projectPtr->setColorPipelineVersion(colorPipelineVersion, false);

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
   constexpr qsizetype kMaxImportedTags = 10000;
   qsizetype importedTags = 0;
   for (const auto& tagVal : tagsArray) {
    if (importedTags++ >= kMaxImportedTags) break;
    if (tagVal.isString()) {
     tags.append(tagVal.toString().trimmed().left(256));
    }
   }
   projectPtr->setAITags(tags);
   qDebug() << "[Importer] AI Tags:" << tags;
  }

  if (root.contains("ai_notes") && root["ai_notes"].isString()) {
   projectPtr->setAINotes(root["ai_notes"].toString());
   qDebug() << "[Importer] AI Notes:" << root["ai_notes"].toString();
  }

  if (root.contains("creationDefaults") && root["creationDefaults"].isObject()) {
   CreationDefaultsState state;
   state.fromJson(root["creationDefaults"].toObject());
   projectPtr->setCreationDefaultsState(state);
   qDebug() << "[Importer] Creation defaults restored";
  }

  // Restore source identities and versions before layer construction. Older
  // projects legitimately omit this section and rebuild it from source paths.
  // Reset only after the project metadata has passed the importer checks, so an
  // early parse failure does not discard the currently loaded project's leases.
  ArtifactCore::AssetManager::instance().resetSourceRegistry();
  if (root.contains("assets") && root["assets"].isObject()) {
   const QJsonObject assets = root["assets"].toObject();
   if (assets.contains("sourceRegistry") && assets["sourceRegistry"].isObject()) {
    if (!ArtifactCore::AssetManager::instance().restoreSourceRegistrySnapshot(
            assets["sourceRegistry"].toObject())) {
     qWarning() << "[Importer] Source registry snapshot contained invalid entries";
    }
   }
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
   constexpr qsizetype kMaxImportedCompositions = 10000;
   qsizetype importedCompositions = 0;
   for (const auto& compVal : compsArray) {
    if (importedCompositions++ >= kMaxImportedCompositions) break;
    if (!compVal.isObject()) continue;
    QJsonObject compObj = compVal.toObject();
    if (!compObj.contains("id")) continue;

    QString idStr = compObj["id"].toString();
    QString compName = compObj.contains("name") && compObj["name"].isString()
     ? compObj["name"].toString()
     : QStringLiteral("Composition");

    // Use the canonical composition deserializer so dimensions, frame rate,
    // frame/work ranges, layer factory, IDs, parenting, and masks are restored
    // consistently with IPC and copy/paste import paths.
    auto compPtr = ArtifactAbstractComposition::fromJson(QJsonDocument(compObj));
    if (!compPtr) {
      qWarning() << "[Importer] Failed to deserialize composition:" << idStr;
      continue;
    }
    projectPtr->addImportedComposition(compPtr, compName);
    result.compositionsLoaded++;
    result.layersLoaded += compPtr->allLayer().size();
    qDebug() << "[Importer] Loaded composition:" << compName
             << "with" << compPtr->allLayer().size() << "layers";
   }
  }

  // Project items (Footage, Folder, etc.) restoration
  if (root.contains("projectItems") && root["projectItems"].isArray()) {
   const QJsonArray sourceProjectItems = root["projectItems"].toArray();
   QJsonArray projectItemsArray;
   constexpr qsizetype kMaxImportedProjectItems = 100000;
   const qsizetype projectItemCount = std::min(
       sourceProjectItems.size(), kMaxImportedProjectItems);
   for (qsizetype i = 0; i < projectItemCount; ++i) {
    projectItemsArray.append(sourceProjectItems.at(i));
   }
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

  // 拡張データ(コマンドパレット MRU 等)を復元
  if (root.contains("_extension_data") && root["_extension_data"].isObject()) {
   projectPtr->setExtensionData(root["_extension_data"].toObject());
  }

  // Restore project-scoped OCIO selection after the project data is parsed.
  // Missing or invalid external config files are handled by the manager's
  // existing fallback behavior and must not reject the project itself.
  if (root.contains(QStringLiteral("ocio")) && root[QStringLiteral("ocio")].isObject()) {
   if (auto* ocio = ArtifactOCIOManager::instance()) {
    ocio->fromJson(root[QStringLiteral("ocio")].toObject());
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
