module;
#include <wobjectimpl.h>
//#include <QtCore/QString>


module Project;


namespace Artifact {
 W_OBJECT_IMPL(ArtifactProject)

 class ArtifactProjectPrivate {
 private:
  //QString projectName_;
 public:
  ArtifactProjectPrivate();
  ~ArtifactProjectPrivate();
  //QString projectName() const;
  //void setProjectName(const QString& name);
 };

 ArtifactProjectPrivate::ArtifactProjectPrivate()
 {

 }

 ArtifactProjectPrivate::~ArtifactProjectPrivate()
 {

 }
 /*
 QString ArtifactProjectPrivate::projectName() const
 {
  return projectName_;
 }

 void ArtifactProjectPrivate::setProjectName(const QString& name)
 {

 }

 */
 ArtifactProject::ArtifactProject()
 {

 }

 ArtifactProject::~ArtifactProject()
 {

 }
 /*

 QString ArtifactProject::projectName() const
 {
  return QString();
 }

 void ArtifactProject::setProjectName(const QString& name)
 {

 }

 */

}