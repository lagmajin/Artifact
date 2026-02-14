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
import Artifact.Layer.InitParams;
import Artifact.Project.Settings;

import Artifact.Composition.Result;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Project.Items;

import Artifact.Project;

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
  void createProject(const ArtifactProjectSettings& setting);

  void addAssetFromPath(const UniString& path);
  void removeAllAssets();

  void createComposition(const UniString& name);
  void createComposition(const ArtifactCompositionInitParams& params);

  ArtifactCompositionWeakPtr currentComposition();
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  FindCompositionResult findComposition(const CompositionID& id);
  QVector<ProjectItem*> projectItems() const;
  
  void addLayer(const CompositionID& id, const ArtifactLayerInitParams& params);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
  bool removeComposition(const CompositionID& id);
  bool renameComposition(const CompositionID& id, const UniString& name);
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr() const;
 public /*signals*/:
  void layerRemoved(const LayerID& id)
   W_SIGNAL(layerRemoved, id);
 public/*signals*/:
  void projectCreated()
  W_SIGNAL(projectCreated);
  void compositionCreated(const CompositionID& id)
   W_SIGNAL(compositionCreated, id);
  void layerCreated(const LayerID& id)
   W_SIGNAL(layerCreated, id);

  void projectChanged()
   W_SIGNAL(projectChanged);
  public:
   void projectSettingChanged(const ArtifactProjectSettings& setting);
   W_SLOT(projectSettingChanged);

 

 };


};