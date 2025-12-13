module;
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
module Artifact.Project.Exporter;





namespace Artifact
{
	
	
 class ArtifactProjectExporter::Impl {
 private:
  
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

  return ArtifactProjectExporterResult();
 }

 void ArtifactProjectExporter::Impl::setOutputPath(const QString& str)
 {

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
 	

 }

 void ArtifactProjectExporter::exportProject2()
 {
  impl_->exportProject2();
 }

 void ArtifactProjectExporter::setOutputPath(const QString& path)
 {

 }

 ArtifactProjectExporterResult ArtifactProjectExporter::exportProject()
 {
  return impl_->exportProject();
 }


};