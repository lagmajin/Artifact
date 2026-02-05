module;
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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