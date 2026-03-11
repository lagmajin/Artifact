module;
#include <wobjectdefs.h>
#include <QSize>
#include <QString>
#include <QStringList>
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
import Artifact.Effect.Abstract;

import Artifact.Project;

W_REGISTER_ARGTYPE(QSize)
W_REGISTER_ARGTYPE(QString)


export namespace Artifact
{
 using namespace ArtifactCore;
 
 enum class PreviewQualityPreset {
  Draft,
  Preview,
  Final
 };

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
  bool hasProject() const;
  void changeProjectName(const UniString& string);
  void createProject(const ArtifactProjectSettings& setting);

  void addAssetFromPath(const UniString& path);
  QStringList importAssetsFromPaths(const QStringList& sourcePaths);
  void removeAllAssets();
  void setPreviewQualityPreset(PreviewQualityPreset preset);
  PreviewQualityPreset previewQualityPreset() const;

  void createComposition(const UniString& name);
  void createComposition(const ArtifactCompositionInitParams& params);

  ArtifactCompositionWeakPtr currentComposition();
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  FindCompositionResult findComposition(const CompositionID& id);
  QVector<ProjectItem*> projectItems() const;
  
  void addLayer(const CompositionID& id, const ArtifactLayerInitParams& params);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
  bool duplicateLayerInCurrentComposition(const LayerID& layerId);
  bool renameLayerInCurrentComposition(const LayerID& layerId, const QString& newName);
  bool isLayerVisibleInCurrentComposition(const LayerID& layerId);
  bool isLayerLockedInCurrentComposition(const LayerID& layerId);
  bool isLayerSoloInCurrentComposition(const LayerID& layerId);
  bool isLayerShyInCurrentComposition(const LayerID& layerId);
  bool setLayerVisibleInCurrentComposition(const LayerID& layerId, bool visible);
  bool setLayerLockedInCurrentComposition(const LayerID& layerId, bool locked);
  bool setLayerSoloInCurrentComposition(const LayerID& layerId, bool solo);
  bool setLayerShyInCurrentComposition(const LayerID& layerId, bool shy);
  bool soloOnlyLayerInCurrentComposition(const LayerID& layerId);
  bool setLayerParentInCurrentComposition(const LayerID& layerId, const LayerID& parentLayerId);
  bool clearLayerParentInCurrentComposition(const LayerID& layerId);
  bool layerHasParentInCurrentComposition(const LayerID& layerId);
  LayerID layerParentIdInCurrentComposition(const LayerID& layerId);
  QString layerNameInCurrentComposition(const LayerID& layerId);
  bool addEffectToLayerInCurrentComposition(const LayerID& layerId, std::shared_ptr<ArtifactAbstractEffect> effect);
  bool removeEffectFromLayerInCurrentComposition(const LayerID& layerId, const QString& effectId);
  bool setEffectEnabledInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, bool enabled);
  bool moveEffectInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, int direction);
  QString layerRemovalConfirmationMessage(const CompositionID& compositionId, const LayerID& layerId) const;
  bool removeProjectItem(ProjectItem* item);
  QString projectItemRemovalConfirmationMessage(ProjectItem* item) const;
  bool removeComposition(const CompositionID& id);
  int renderQueueCountForComposition(const CompositionID& id) const;
  QString compositionRemovalConfirmationMessage(const CompositionID& id) const;
  bool removeCompositionWithRenderQueueCleanup(const CompositionID& id, int* removedQueueCount = nullptr);
  bool duplicateComposition(const CompositionID& id);
  bool renameComposition(const CompositionID& id, const UniString& name);
  std::shared_ptr<ArtifactProject> getCurrentProjectSharedPtr() const;
 public /*signals*/:
  void layerRemoved(const CompositionID& compId, const LayerID& layerId)
   W_SIGNAL(layerRemoved, compId, layerId);
 public/*signals*/:
  void projectCreated()
  W_SIGNAL(projectCreated);
  void compositionCreated(const CompositionID& id)
   W_SIGNAL(compositionCreated, id);
  void currentCompositionChanged(const CompositionID& id)
   W_SIGNAL(currentCompositionChanged, id);
  void layerCreated(const CompositionID& compId, const LayerID& layerId)
    W_SIGNAL(layerCreated, compId, layerId);

  void projectChanged()
   W_SIGNAL(projectChanged);
  void layerSelected(const LayerID& id)
   W_SIGNAL(layerSelected, id);
  void previewQualityPresetChanged(PreviewQualityPreset preset)
   W_SIGNAL(previewQualityPresetChanged, preset);
  public:
   void selectLayer(const LayerID& id);
   void projectSettingChanged(const ArtifactProjectSettings& setting);
   W_SLOT(projectSettingChanged);

 

 };


};
