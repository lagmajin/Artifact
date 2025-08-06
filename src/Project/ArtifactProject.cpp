module;
#include <wobjectimpl.h>
//#include <QtCore/QString>


module Project;


namespace Artifact {

 W_OBJECT_IMPL(ArtifactProject)

  ArtifactProjectSignalHelper::ArtifactProjectSignalHelper()
 {

 }

 ArtifactProjectSignalHelper::~ArtifactProjectSignalHelper()
 {

 }


 class ArtifactProject::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactProject::Impl::Impl()
 {

 }

 ArtifactProject::Impl::~Impl()
 {

 }

 ArtifactProject::ArtifactProject() :impl_(new Impl())
 {

 }

 ArtifactProject::ArtifactProject(const QString& name):impl_(new Impl())
 {

 }

 ArtifactProject::ArtifactProject(const ArtifactProjectSettings& setting):impl_(new Impl())
 {

 }

 ArtifactProject::~ArtifactProject()
 {
  delete impl_;
 }

 void ArtifactProject::createComposition(const QString& name)
 {

 }

 bool ArtifactProject::isNull() const
 {
  return false;
 }

 void ArtifactProject::addAssetFromPath(const QString& filepath)
 {

 }

}