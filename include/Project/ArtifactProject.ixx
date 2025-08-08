module;
#include <wobjectdefs.h>
//#include <memory>
#include <QObject>
#include <QPointer>


#include <QJsonObject>

#include <boost/signals2.hpp>

export module Project;

export import Project.Settings;

import std;

export namespace Artifact {

 class ArtifactProjectSignalHelper {
 private:

 public:
  ArtifactProjectSignalHelper();
  ~ArtifactProjectSignalHelper();
 };

 


 class ArtifactProject :public QObject{
  W_OBJECT(ArtifactProject)
 private:
  class Impl;
  Impl* impl_;

 public:
  ArtifactProject();
  ArtifactProject(const QString& name);
  ArtifactProject(const ArtifactProjectSettings& setting);
  ~ArtifactProject();
  ArtifactProjectSettings settings() const;
  void createComposition(const QString&name);
  void addAssetFile();
  void addAssetFromPath(const QString& filepath);
  bool isNull() const;

 //public slots:
 };

 typedef std::shared_ptr<ArtifactProject> ArtifactProject2DPtr;


}