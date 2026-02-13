module;
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSaveFile>
#include <QDateTime>
module Artifact.Project.Exporter;

import std;
import Artifact.Project;
import Utils.String.UniString;



namespace Artifact
{
 class ArtifactProjectExporter::Impl {
 private:
  QString outputPath_;
  eProjectFormat format_ = ProjectFormat_JSON;
  
 public:
  Impl();
  ~Impl();
  ArtifactProjectPtr projectPtr_;
  
  ArtifactProjectExporterResult exportProject();
  ArtifactProjectExporterResult exportTimelineData();
  ArtifactProjectExporterResult exportMetadata();
  void setOutputPath(const QString& str);
  void setFormat(eProjectFormat format);
  
  QJsonObject serializeProject();
  QJsonObject serializeTimeline();
  QJsonObject serializeCompositions();
  QJsonObject serializeMetadata();
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

 void ArtifactProjectExporter::Impl::setFormat(eProjectFormat format)
 {
  format_ = format;
 }

 QJsonObject ArtifactProjectExporter::Impl::serializeMetadata()
 {
  QJsonObject metadata;
  metadata["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  metadata["version"] = "1.0.0";
  metadata["format"] = (int)format_;
  return metadata;
 }

 QJsonObject ArtifactProjectExporter::Impl::serializeTimeline()
 {
  QJsonObject timeline;
  
  if (!projectPtr_ || projectPtr_->isNull()) {
   return timeline;
  }
  
  // TODO: Implement timeline serialization when TimelineScene is accessible
  // This will include:
  // - Track count and heights
  // - Clip positions, durations
  // - Layer-to-track mapping
  
  return timeline;
 }

 QJsonObject ArtifactProjectExporter::Impl::serializeCompositions()
 {
  QJsonObject compositions;
  
  if (!projectPtr_ || projectPtr_->isNull()) {
   return compositions;
  }
  
  // TODO: Serialize compositions with their layer hierarchy
  // This will include all composition data needed for project recovery
  
  return compositions;
 }

 QJsonObject ArtifactProjectExporter::Impl::serializeProject()
 {
  QJsonObject root;
  
  root["metadata"] = serializeMetadata();
  root["compositions"] = serializeCompositions();
  root["timeline"] = serializeTimeline();
  
  return root;
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::Impl::exportMetadata()
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  if (!projectPtr_ || projectPtr_->isNull()) {
   result.errorMessage = UniString("Project is null or invalid");
   return result;
  }
  
  if (outputPath_.isEmpty()) {
   result.errorMessage = UniString("Output path is not set");
   return result;
  }

  // Serialize metadata separately
  QJsonObject metadata = serializeMetadata();
  QJsonDocument doc(metadata);
  QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

  QString metadataPath = outputPath_ + ".metadata";
  QSaveFile file(metadataPath);
  
  if (!file.open(QIODevice::WriteOnly)) {
   result.errorMessage = UniString("Failed to open metadata file for writing");
   return result;
  }
  
  if (file.write(jsonData) != jsonData.size()) {
   file.cancelWriting();
   result.errorMessage = UniString("Failed to write metadata: incomplete write");
   return result;
  }
  
  if (!file.commit()) {
   result.errorMessage = UniString("Failed to commit metadata file");
   return result;
  }

  result.success = true;
  return result;
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::Impl::exportTimelineData()
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  if (!projectPtr_ || projectPtr_->isNull()) {
   result.errorMessage = UniString("Project is null or invalid");
   return result;
  }
  
  if (outputPath_.isEmpty()) {
   result.errorMessage = UniString("Output path is not set");
   return result;
  }

  // Serialize timeline data
  QJsonObject timeline = serializeTimeline();
  QJsonDocument doc(timeline);
  QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

  QString timelinePath = outputPath_ + ".timeline";
  QSaveFile file(timelinePath);
  
  if (!file.open(QIODevice::WriteOnly)) {
   result.errorMessage = UniString("Failed to open timeline file for writing");
   return result;
  }
  
  if (file.write(jsonData) != jsonData.size()) {
   file.cancelWriting();
   result.errorMessage = UniString("Failed to write timeline: incomplete write");
   return result;
  }
  
  if (!file.commit()) {
   result.errorMessage = UniString("Failed to commit timeline file");
   return result;
  }

  result.success = true;
  return result;
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::Impl::exportProject()
 {
  ArtifactProjectExporterResult result;
  result.success = false;

  if (!projectPtr_ || projectPtr_->isNull()) {
   result.errorMessage = UniString("Project is null or invalid");
   return result;
  }
  
  if (outputPath_.isEmpty()) {
   result.errorMessage = UniString("Output path is not set");
   return result;
  }

  QJsonObject obj = projectPtr_->toJson();
  QJsonDocument doc(obj);
  QByteArray jsonData = doc.toJson(QJsonDocument::Indented);

   QSaveFile file(outputPath_);
   if (!file.open(QIODevice::WriteOnly)) {
    result.errorMessage = UniString("Failed to open project file for writing");
    return result;
   }
   
   if (file.write(jsonData) != jsonData.size()) {
    file.cancelWriting();
    result.errorMessage = UniString("Failed to write project: incomplete write");
    return result;
   }
   
   if (!file.commit()) {
    result.errorMessage = UniString("Failed to commit project file");
    return result;
   }

   // Also export timeline and metadata
   exportTimelineData();
   exportMetadata();

   result.success = true;
   return result;
  }
  catch (const std::exception& ex) {
   result.errorMessage = UniString(std::string("Export exception: ") + ex.what());
   return result;
  }
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

 void ArtifactProjectExporter::setOutputPath(const QString& path)
 {
  impl_->setOutputPath(path);
 }

 void ArtifactProjectExporter::setOutputPath(const UniString& path)
 {
  impl_->setOutputPath(path.toQString());
 }

 void ArtifactProjectExporter::setFormat(eProjectFormat format)
 {
  impl_->setFormat(format);
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::exportProject()
 {
  return impl_->exportProject();
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::exportTimelineData()
 {
  return impl_->exportTimelineData();
 }

 ArtifactProjectExporterResult ArtifactProjectExporter::exportMetadata()
 {
  return impl_->exportMetadata();
 }
};
