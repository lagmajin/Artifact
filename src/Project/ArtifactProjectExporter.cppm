module;
#include <utility>
#include <functional>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QStringList>
module Artifact.Project.Exporter;

import Asset.Manager;
import Artifact.Color.OCIOManager;
import Serialization.ProjectSerializer;

namespace Artifact
{
 class ArtifactProjectExporter::Impl {
 private:
  QString outputPath_;
 public:
  Impl();
  ~Impl();
  ArtifactProjectPtr projectPtr_;
  ArtifactProjectExporterResult exportProject();
  void setOutputPath(const QString& str);
  void exportProject2();
 };

 ArtifactProjectExporter::Impl::Impl()
 {
 }

 ArtifactProjectExporter::Impl::~Impl()
 {
 }

 void ArtifactProjectExporter::Impl::setOutputPath(const QString& str)
 {
  outputPath_ = str;
 }

 void ArtifactProjectExporter::Impl::exportProject2()
 {
  if (projectPtr_==nullptr || projectPtr_->isNull())
  {
   return;
  }
  const auto result = exportProject();
  if (!result.success) {
   qWarning() << "[Exporter] exportProject2() failed:" << result.errorStage << result.errorMessage;
  }
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::Impl::exportProject()
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  if (!projectPtr_ || projectPtr_->isNull()) {
   result.errorStage = "precondition";
   result.errorMessage = "Project is null";
   return result;
  }
  if (outputPath_.isEmpty()) {
   result.errorStage = "precondition";
   result.errorMessage = "Output path is empty";
   return result;
  }

  // Validate project metadata before serializing.  The tree validator below
  // cannot catch invalid project names or metadata because those belong to
  // ArtifactProjectSettings.
  const auto projectIssues = projectPtr_->validate();
  QStringList blockingIssues;
  for (const auto& issue : projectIssues) {
   const QString message = QStringLiteral("%1: %2")
                               .arg(issue.field, issue.message);
   if (issue.severity == ProjectValidationIssue::Severity::Error) {
    blockingIssues.append(message);
   } else {
    qWarning() << "[Exporter] Project validation notice:" << message;
   }
  }
  if (!blockingIssues.isEmpty()) {
   result.errorStage = "settings_validation";
   result.errorMessage = blockingIssues.join(QStringLiteral("\n"));
   qWarning() << "[Exporter] Project settings validation failed:"
              << result.errorMessage;
   return result;
  }

  QString treeError;
  if (!projectPtr_->validateProjectTree(&treeError)) {
   result.errorStage = "validation";
   result.errorMessage = treeError;
   qWarning() << "[Exporter] Project tree validation failed:" << treeError;
   return result;
  }

  QJsonObject obj = projectPtr_->toJson();

  // Keep the existing absolute path as a fallback while recording a project-
  // relative candidate for relocation. Older importers ignore the extra keys.
  const QString projectDirectory = QFileInfo(outputPath_).absolutePath();
  const auto relativePathFor = [&projectDirectory](const QString& path) {
   return ArtifactCore::projectRelativeSourceCandidate(projectDirectory, path);
  };
  QJsonObject assets = obj.value(QStringLiteral("assets")).toObject();
  QJsonObject sourceRegistry = assets.value(QStringLiteral("sourceRegistry")).toObject();
  QJsonArray sources = sourceRegistry.value(QStringLiteral("sources")).toArray();
  for (int i = 0; i < sources.size(); ++i) {
   if (!sources.at(i).isObject()) {
    continue;
   }
   QJsonObject source = sources.at(i).toObject();
   const QString relative = relativePathFor(
       source.value(QStringLiteral("path")).toString());
   if (!relative.isEmpty()) {
    source.insert(QStringLiteral("pathRelative"), relative);
   }
   sources[i] = source;
  }
  if (!sources.isEmpty()) {
   sourceRegistry.insert(QStringLiteral("sources"), sources);
   assets.insert(QStringLiteral("sourceRegistry"), sourceRegistry);
   obj.insert(QStringLiteral("assets"), assets);
  }
  std::function<void(QJsonObject&)> annotateProjectItem =
      [&](QJsonObject& item) {
       if (item.value(QStringLiteral("type")).toString() ==
           QStringLiteral("footage")) {
        const QString filePath = item.value(QStringLiteral("filePath")).toString();
        const QString relativePath = relativePathFor(filePath);
        if (!relativePath.isEmpty()) {
         item.insert(QStringLiteral("filePathRelative"), relativePath);
        }
        const QJsonArray sequencePaths =
            item.value(QStringLiteral("sequencePaths")).toArray();
        if (!sequencePaths.isEmpty()) {
         QJsonArray relativeSequencePaths;
         for (const auto& value : sequencePaths) {
          const QString relative = relativePathFor(value.toString());
           relativeSequencePaths.append(
               relative.isEmpty() ? value : QJsonValue(relative));
         }
         item.insert(QStringLiteral("sequencePathsRelative"),
                     relativeSequencePaths);
        }
       }
       QJsonArray children = item.value(QStringLiteral("children")).toArray();
       for (int i = 0; i < children.size(); ++i) {
        if (!children.at(i).isObject()) {
         continue;
        }
        QJsonObject child = children.at(i).toObject();
        annotateProjectItem(child);
        children[i] = child;
       }
       if (!children.isEmpty()) {
        item.insert(QStringLiteral("children"), children);
       }
      };
  QJsonArray projectItems = obj.value(QStringLiteral("projectItems")).toArray();
  for (int i = 0; i < projectItems.size(); ++i) {
   if (!projectItems.at(i).isObject()) {
    continue;
   }
   QJsonObject item = projectItems.at(i).toObject();
   annotateProjectItem(item);
   projectItems[i] = item;
  }
  obj.insert(QStringLiteral("projectItems"), projectItems);

  std::function<void(QJsonObject&)> annotateLayerSourcePaths =
      [&](QJsonObject& object) {
       const QStringList keys = object.keys();
       for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        const bool isSourcePath =
            key == QStringLiteral("sourcePath") ||
            key.endsWith(QStringLiteral(".sourcePath"));
        const bool isSequencePaths =
            key == QStringLiteral("sequencePaths") ||
            key.endsWith(QStringLiteral(".sequencePaths"));
        if (isSourcePath && value.isString()) {
         const QString relative = relativePathFor(value.toString());
         if (!relative.isEmpty()) {
          object.insert(key + QStringLiteral("Relative"), relative);
         }
        } else if (isSequencePaths && value.isArray()) {
         QJsonArray relativePaths;
         for (const auto& entry : value.toArray()) {
          const QString relative = relativePathFor(entry.toString());
           relativePaths.append(
               relative.isEmpty() ? entry : QJsonValue(relative));
         }
         if (!relativePaths.isEmpty()) {
          object.insert(key + QStringLiteral("Relative"), relativePaths);
         }
        }

        if (value.isObject()) {
         QJsonObject child = value.toObject();
         annotateLayerSourcePaths(child);
         object.insert(key, child);
        } else if (value.isArray()) {
         QJsonArray children = value.toArray();
         for (int i = 0; i < children.size(); ++i) {
          if (!children.at(i).isObject()) {
           continue;
          }
          QJsonObject child = children.at(i).toObject();
          annotateLayerSourcePaths(child);
          children[i] = child;
         }
         object.insert(key, children);
        }
       }
      };
  QJsonArray compositions = obj.value(QStringLiteral("compositions")).toArray();
  for (int i = 0; i < compositions.size(); ++i) {
   if (!compositions.at(i).isObject()) {
    continue;
   }
   QJsonObject composition = compositions.at(i).toObject();
   annotateLayerSourcePaths(composition);
   compositions[i] = composition;
  }
  obj.insert(QStringLiteral("compositions"), compositions);

  // Project-scoped OCIO selection. Keep this at the project root so older
  // importers can ignore it without affecting composition/layer data.
  if (auto* ocio = ArtifactOCIOManager::instance()) {
   obj[QStringLiteral("ocio")] = ocio->toJson();
  }

  // AI向けメタデータセクションを追加
  QJsonObject aiMetadata;
  aiMetadata["_ai_export_version"] = "1.0";
  aiMetadata["_ai_export_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  aiMetadata["_ai_schema_description"] = "Artifact Project Format - AI-friendly export";

  // プロジェクト構造のサマリー
  QJsonObject projectSummary;
  if (obj.contains("compositions")) {
   QJsonArray comps = obj["compositions"].toArray();
   projectSummary["composition_count"] = comps.size();

   // 各コンポジションの概要
   QJsonArray compsSummary;
   for (const auto& compVal : comps) {
    QJsonObject comp = compVal.toObject();
    QJsonObject compInfo;
    compInfo["name"] = comp.value("name");
    compInfo["id"] = comp.value("id");
    compsSummary.append(compInfo);
   }
   projectSummary["compositions_summary"] = compsSummary;
  }
  aiMetadata["project_summary"] = projectSummary;

  // AI向けの説明テキスト
  QJsonObject aiInstructions;
  aiInstructions["format"] = "This is an Artifact project file containing compositions and layers";
  aiInstructions["structure"] = "Top-level contains project metadata, compositions array, and assets";
  aiInstructions["composition_structure"] = "Each composition contains layers, settings, and frame range";
  aiInstructions["layer_structure"] = "Layers have id, name, type, transform, and effects";
  aiMetadata["ai_instructions"] = aiInstructions;

  obj["_ai_metadata"] = aiMetadata;

  // 拡張データ(コマンドパレット MRU 等)を保存
  {
   const QJsonObject ext = projectPtr_->extensionData();
   if (!ext.isEmpty()) {
    obj["_extension_data"] = ext;
   }
  }

  const bool artifactOutput = outputPath_.endsWith(
      QStringLiteral(".artifact"), Qt::CaseInsensitive);
  const bool saved = artifactOutput
      ? ArtifactCore::Serialization::ProjectSerializer::saveArtifact(outputPath_, obj)
      : ArtifactCore::Serialization::ProjectSerializer::save(
            outputPath_, obj,
            ArtifactCore::Serialization::SerializationFormat::Json);
  if (!saved) {
   result.errorStage = "file_write";
   result.errorMessage = "Failed to write project document";
   return result;
  }

  result.success = true;
  return result;
 }

 ArtifactProjectExporter::ArtifactProjectExporter():impl_(new Impl())
 {

 }

 ArtifactProjectExporter::~ArtifactProjectExporter()
 {
  delete impl_;
 }

 void ArtifactProjectExporter::setProject(ArtifactProjectPtr& ptr)
 {
  impl_->projectPtr_ = ptr;
 }

 void ArtifactProjectExporter::exportProject2()
 {
  impl_->exportProject2();
 }

 void ArtifactProjectExporter::setOutputPath(const QString& path)
 {
  impl_->setOutputPath(path);
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::exportProject()
 {
  return impl_->exportProject();
 }

};

