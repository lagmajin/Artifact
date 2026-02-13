module;
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSaveFile>
module Artifact.Project.Exporter;





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

 void ArtifactProjectExporter::Impl::exportProject2()
 {
  if (projectPtr_==nullptr || projectPtr_->isNull())
  {
   return;
  }
 	
 	
 	
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::Impl::exportProject()
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  if (!projectPtr_ || projectPtr_->isNull()) {
   return result;
  }
  if (outputPath_.isEmpty()) {
   return result;
  }

  QJsonObject obj = projectPtr_->toJson();

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
    // TODO: レイヤー数などの情報を追加
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

  QJsonDocument doc(obj);
  QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

  QSaveFile file(outputPath_);
  if (!file.open(QIODevice::WriteOnly)) {
   return result;
  }
  if (file.write(jsonData) != jsonData.size()) {
   file.cancelWriting();
   return result;
  }
  if (!file.commit()) {
   return result;
  }

  result.success = true;
  return result;
 }

 void ArtifactProjectExporter::Impl::setOutputPath(const QString& str)
 {
  outputPath_ = str;
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