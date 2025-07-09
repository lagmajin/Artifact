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
  std::unique_ptr<ArtifactProjectPrivate> pImpl_;

 public:
  ArtifactProject();
  ~ArtifactProject();
  void addAssetFile();

 //public slots:
 };



}