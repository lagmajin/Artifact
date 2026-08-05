module;
#include <utility>
#include <QFile>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QStringList>
module Artifact.Project.Exporter;

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

