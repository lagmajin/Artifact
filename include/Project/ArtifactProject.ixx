module;
#include <wobjectdefs.h>
#include <memory>
#include <QObject>
export module ArtifactProject;


//#include <QtCore/QObject>





export namespace Artifact {

 class ArtifactProjectPrivate;

 class ArtifactProject :QObject{
  W_OBJECT(ArtifactProject)
 private:
  std::unique_ptr<ArtifactProjectPrivate> pImpl_;

 public:
  ArtifactProject();
  ~ArtifactProject();
  //QString projectName() const;
  //void setProjectName(const QString&name);
 //signals:
  void updated();
  //void projectNameChanged(const QString& name);

 //public slots:
 };



}