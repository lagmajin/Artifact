module;
#include <wobjectdefs.h>
#include <QSize>
#include <QString>
#include <QObject>
//#include <winrt/impl/Windows.UI.Composition.1.h>

export module Artifact.Service.Project;

import std;
import Utils;
import Utils.String.Like;
import Utils.String.UniString;
import Artifact.Project.Settings;
import Artifact.Layer.InitParams;
import Artifact.Composition.Abstract;


W_REGISTER_ARGTYPE(QSize)
W_REGISTER_ARGTYPE(QString)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactProjectService;

 typedef std::shared_ptr<ArtifactProjectService> ArtifactProjectServicePtr;


 class ArtifactProjectService :public QObject
 {
  W_OBJECT(ArtifactProjectService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectService(QObject* parent = nullptr);
  ~ArtifactProjectService();
  static ArtifactProjectService* instance();
  UniString projectName() const;
  void changeProjectName(const UniString& string);
  void addAssetFromPath(const UniString& path);
  ArtifactCompositionWeakPtr findComposition(const CompositionID& id);
  
  void addLayer(const CompositionID& id, const ArtifactLayerInitParams& params);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
 public:
  void projectCreated()
  W_SIGNAL(projectCreated);
  void projectSettingChanged(const ArtifactProjectSettings& setting);
  W_SLOT(projectSettingChanged);

 };


};