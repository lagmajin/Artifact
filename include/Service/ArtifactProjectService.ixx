module;
#include <utility>
#include <memory>
#include <functional>
#include <vector>

//#include <winrt/impl/Windows.UI.Composition.1.h>

#include <wobjectdefs.h>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QObject>
export module Artifact.Service.Project;

import Utils;
import Utils.String.Like;
import Utils.String.UniString;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Core.Diagnostics.ProjectDiagnostic;
import Artifact.Layer.InitParams;
import Artifact.Project.Settings;
import Artifact.Project.Health;

import Artifact.Composition.Result;
import Artifact.Composition.InitParams;
import Artifact.Project.Items;
import Artifact.Effect.Abstract;

import Artifact.Project;
import Memory.SharedPtr;

W_REGISTER_ARGTYPE(QSize)
W_REGISTER_ARGTYPE(QString)

export enum class PreviewQualityPreset {
 Draft,
 Preview,
 Final
};

// Precompose mode mirrors the AE "Pre-compose" dialog radio options.
//   MoveSelected       - move only the chosen layers into the new comp.
//   MoveAllAttributes  - move the entire active composition into the new comp
//                        and place a single precomp layer that reproduces the
//                        original timeline extent (startTime + in/out).
export enum class PrecomposeMode {
 MoveSelected,
 MoveAllAttributes
};

W_REGISTER_ARGTYPE(PrecomposeMode)

// Outcome of a precompose operation: identifies the generated precomp layer
// and child composition so undo/redo commands can target them. Nil fields
// signal failure.
struct PrecomposeOutcome {
  ArtifactCore::LayerID precompLayerId;
  ArtifactCore::CompositionID childCompId;
};

struct RelinkCandidate {
  QString path;
  int score = 0;
  QString reason;
};

W_REGISTER_ARGTYPE(PreviewQualityPreset)

export namespace Artifact {
 using namespace ArtifactCore;
 
 class ArtifactProjectService;

 typedef SharedPtr<ArtifactProjectService> ArtifactProjectServicePtr;

	
 class ArtifactProjectService :public QObject
 {
  W_OBJECT(ArtifactProjectService)
 private:
  // M-UG-1: 子コンポ長さ変更を親プリコンポレイヤーへ伝播する（setFrameRange の
  // CompositionChangedEvent を購読）。
  void onChildCompositionFrameRangeChanged(const Artifact::CompositionChangedEvent& e);
  void propagateChildFrameRangeToParents(const CompositionID& childCompId);
  void propagateParentFrameRangeToChildren(const CompositionID& parentCompId);
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectService(QObject* parent = nullptr);
  ~ArtifactProjectService();
  static ArtifactProjectService* instance();
  UniString projectName() const;
  bool hasProject() const;
  bool ensureProject();
  QString currentProjectAssetsPath() const;
  void changeProjectName(const UniString& string);
  void createProject(const ArtifactProjectSettings& setting);

  void addAssetFromPath(const UniString& path);
  QStringList importAssetsFromPaths(const QStringList& sourcePaths);
  int importPsdLayersToCurrentComposition(const QString& filePath,
                                          bool visibleOnly = true);
  void importAssetsFromPathsAsync(const QStringList& sourcePaths,
                                 std::function<void(QStringList)> onFinished);
  void removeAllAssets();
  void setPreviewQualityPreset(PreviewQualityPreset preset);
  PreviewQualityPreset previewQualityPreset() const;

  void createComposition(const UniString& name);
  void createComposition(const ArtifactCompositionInitParams& params);

  ArtifactCompositionWeakPtr currentComposition();
  ChangeCompositionResult changeCurrentComposition(const CompositionID& id);

  FindCompositionResult findComposition(const CompositionID& id);
  QVector<ProjectItem*> projectItems() const;
  ProjectHealthReport currentProjectHealthReport() const;
  std::vector<ArtifactCore::ProjectDiagnostic> currentProjectDiagnostics() const;
  QString currentProjectHealthSummaryText() const;
  QString currentProjectHealthStateToken() const;
  
  void addLayer(const CompositionID& id, const ArtifactLayerInitParams& params);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params, bool selectNewLayer);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params,
                                    bool selectNewLayer,
                                    bool placeAtCurrentFrame);
  void addLayerToCurrentComposition(const ArtifactLayerInitParams& params,
                                    bool selectNewLayer,
                                    bool placeAtCurrentFrame,
                                    bool startHidden);
  void setDefaultNewLayerHidden(bool hidden);
  bool defaultNewLayerHidden() const;
  bool groupSelectedLayersInCurrentComposition(const UniString& groupName = UniString(QStringLiteral("Group 1")));
  bool ungroupSelectedGroupInCurrentComposition();
  bool removeLayerFromComposition(const CompositionID& compositionId, const LayerID& layerId);
  bool moveLayerInCurrentComposition(const LayerID& layerId, int newIndex);
  bool duplicateLayerInCurrentComposition(const LayerID& layerId);
  bool renameLayerInCurrentComposition(const LayerID& layerId, const QString& newName);
  bool replaceLayerSourceInCurrentComposition(const LayerID& layerId, const QString& sourcePath);
  bool localizeLayerSourceInCurrentComposition(const LayerID& layerId);
  bool relinkSharedLayerSourceInCurrentComposition(const LayerID& layerId);
  bool isLayerVisibleInCurrentComposition(const LayerID& layerId);
  bool isLayerLockedInCurrentComposition(const LayerID& layerId);
  bool isLayerSoloInCurrentComposition(const LayerID& layerId);
  bool isLayerShyInCurrentComposition(const LayerID& layerId);
  bool setLayerVisibleInCurrentComposition(const LayerID& layerId, bool visible);
  bool setLayerLockedInCurrentComposition(const LayerID& layerId, bool locked);
  bool setLayerSoloInCurrentComposition(const LayerID& layerId, bool solo);
  bool setLayerShyInCurrentComposition(const LayerID& layerId, bool shy);
  bool soloOnlyLayerInCurrentComposition(const LayerID& layerId);
  bool smartSoloOnlyLayerInCurrentComposition(const LayerID& layerId);
  bool setLayerParentInCurrentComposition(const LayerID& layerId, const LayerID& parentLayerId);
  bool clearLayerParentInCurrentComposition(const LayerID& layerId);
  bool layerHasParentInCurrentComposition(const LayerID& layerId);
  LayerID layerParentIdInCurrentComposition(const LayerID& layerId);
  QString layerNameInCurrentComposition(const LayerID& layerId);
  bool addEffectToLayerInCurrentComposition(const LayerID& layerId, SharedPtr<ArtifactAbstractEffect> effect);
  bool removeEffectFromLayerInCurrentComposition(const LayerID& layerId, const QString& effectId);
  bool setEffectEnabledInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, bool enabled);
  bool moveEffectInLayerInCurrentComposition(const LayerID& layerId, const QString& effectId, int direction);
  bool addEffectToCurrentComposition(SharedPtr<ArtifactAbstractEffect> effect);
  bool removeEffectFromCurrentComposition(const QString& effectId);
  bool setEffectEnabledInCurrentComposition(const QString& effectId, bool enabled);
  bool moveEffectInCurrentComposition(const QString& effectId, int direction);
  QString layerRemovalConfirmationMessage(const CompositionID& compositionId, const LayerID& layerId) const;
  bool removeProjectItem(ProjectItem* item);
  bool projectItemRemovalUndoAvailable(ProjectItem* item) const;
  bool createProjectFolder(const UniString& name, FolderItem* parentFolder = nullptr);
  bool renameProjectItem(ProjectItem* item, const UniString& name);
  bool setProjectItemTags(ProjectItem* item, const QStringList& tags);
  bool setFootageAssetRole(FootageItem* item, ProjectAssetUsage usage,
                           ProjectRenderInputRole role);
  bool moveProjectItem(ProjectItem* item, ProjectItem* newParent);
  QString projectItemRemovalConfirmationMessage(ProjectItem* item) const;
  bool removeComposition(const CompositionID& id);
  int renderQueueCountForComposition(const CompositionID& id) const;
  QString compositionRemovalConfirmationMessage(const CompositionID& id) const;
  bool removeCompositionWithRenderQueueCleanup(const CompositionID& id, int* removedQueueCount = nullptr);
   bool duplicateComposition(const CompositionID& id);
   bool renameComposition(const CompositionID& id, const UniString& name);
   // Keep composition-settings edits from different UI surfaces on the same
   // dirty-state and playback synchronization path.
   bool finalizeCompositionSettingsChange(const CompositionID& id);
  bool precomposeLayersInCurrentComposition(
       const QVector<LayerID>& layerIds,
       const UniString& newCompositionName,
       bool openNewComposition = true,
       bool matchWorkspaceDuration = true,
       PrecomposeMode mode = PrecomposeMode::MoveSelected);
  bool unprecomposeLayerInCurrentComposition(const LayerID& layerId,
                                             bool keepComposition = true);

  // M-UG-1: 子コンポ長さ変更時の親プリコンポレイヤーへの伝播を一時停止する。
  // ロード/修復/Python 一括操作の前後に set し、余計な伝播を抑止する。
  void setSuspendFrameRangeCascade(bool suspend);

  // === Precompose undo support ===
  // These return the ids touched by the most recent precompose / unprecompose
  // call. The UI layer reads them immediately after a successful mutation to
  // build an undo command targeting the exact generated/removed entities.
  // Returned ids are nil if the last operation failed or hasn't run.
  PrecomposeOutcome lastPrecomposeOutcome() const;
  LayerID lastUnprecomposePrecompLayerId() const;
  CompositionID lastUnprecomposeChildCompId() const;
  QVector<LayerID> lastUnprecomposeMovedLayerIds() const;
  UniString lastUnprecomposeChildName() const;

  // Undo-aware entry points. They perform the same mutation as the plain
  // variants but push a command onto the global undo stack first, so the
  // operation is reversible. The plain variants remain for code paths that
  // manage their own undo (e.g. inside another command's redo/undo).
  bool precomposeLayersWithUndo(const QVector<LayerID>& layerIds,
                                const UniString& newCompositionName,
                                bool openNewComposition,
                                bool matchWorkspaceDuration,
                                PrecomposeMode mode);
  bool unprecomposeLayerWithUndo(const LayerID& layerId, bool keepComposition);

  // Undo-aware group / ungroup
  bool groupSelectedLayersWithUndo(const UniString& groupName = UniString(QStringLiteral("Group 1")));
  bool ungroupSelectedGroupWithUndo();

  // Undo-aware split
  bool splitLayerWithUndo(const CompositionID& compositionId, const LayerID& layerId);

  // Undo-aware effect operations
  bool addEffectToLayerWithUndo(const LayerID& layerId,
                                SharedPtr<ArtifactAbstractEffect> effect);
  bool removeEffectFromLayerWithUndo(const LayerID& layerId,
                                     const QString& effectId,
                                     SharedPtr<ArtifactAbstractEffect> effect);
  bool setEffectEnabledWithUndo(const LayerID& layerId,
                                const QString& effectId,
                                bool enabled,
                                bool wasEnabled);
  bool moveEffectWithUndo(const LayerID& layerId,
                          const QString& effectId,
                          int direction);
   void splitLayerAtCurrentTime(const CompositionID& compositionId, const LayerID& layerId);
   ArtifactProjectPtr getCurrentProjectSharedPtr() const;
   // Relink functions
   bool relinkFootage(ProjectItem* footageItem, const QString& newFilePath);
   int relinkFootageItems(const QVector<FootageItem*>& footageItems, const QString& newFilePath);
   bool relinkFootageByPath(const QString& oldFilePath, const QString& newFilePath);
   QVector<RelinkCandidate> findRelinkCandidates(
       const QString& oldFilePath, const QString& searchRoot,
       int maxCandidates = 32) const;
   FootageItem* findFootageItemByPath(const QString& filePath) const;
  public:
   void selectLayer(const LayerID& id);
   void projectSettingChanged(const ArtifactProjectSettings& setting);
   W_SLOT(projectSettingChanged);

 

 };


};

export namespace Artifact {
std::vector<ArtifactCore::ProjectDiagnostic> convertProjectHealthReportToDiagnostics(const ProjectHealthReport& report);
}
