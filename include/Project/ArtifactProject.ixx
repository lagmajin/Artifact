module;
#include <wobjectdefs.h>
#include <memory>
#include <QObject>
#include <QPointer>
export module Project;

export import Project.Settings;

//#include <QtCore/QObject>





export namespace Artifact {

 class ArtifactProjectSignalHelper {
 private:

 public:
  ArtifactProjectSignalHelper();
  ~ArtifactProjectSignalHelper();
 };


 class ArtifactProjectPrivate;

 class ArtifactProject :public QObject{
  W_OBJECT(ArtifactProject)
 private:
  class Impl;
  Impl* impl_;

 public:
  ArtifactProject();
  ArtifactProject(const QString& name);
  ArtifactProject(const ArtifactProjectSetting& setting);
  ~ArtifactProject();
  void createComposition(const QString&name);
  void addAssetFile();

 //public slots:
 };



}