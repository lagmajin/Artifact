module;
#include <utility>
#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <QObject>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QUuid>
#include <QVector>
#include <QVariant>
#include <vector>

export module Artifact.Composition.Abstract;
import Memory.SharedPtr;
import Utils;
import Utils.Optional;
import Utils.String.UniString;
import Asset.File;
import Animation.Value;

import Color.Float;

import Frame.Rate;
import Frame.Range;
import Frame.Position;
import Time.Code;
import Container.MultiIndex;
import Composition.Context;
import Core.ArtifactArray;
import Artifact.Composition.Access;
import Artifact.Composition.InitParams;
import Composition.Settings;
import Audio.Analyze;
import Audio.Segment;
import Audio.Mixer;
import Artifact.Composition.InOutPoints;
import Artifact.Composition.Nodes;
import Memory.SharedPtr;
import Property.Abstract;
import Artifact.Layer.Abstract;

import Geometry.ResolutionRemap;
import Artifact.Effect.Abstract;
//import Artifact.Layer.Abstract;
//import Artifact.Preview.Controller;

export namespace Artifact {

 using namespace ArtifactCore;
 enum class AppendLayerToCompositionError {
  None = 0,
  CompositionNotFound,
  LayerNotFound,
  LayerTypeMismatch,
  UnknownError,
  LayerInitializationFailed
 };
 struct AppendLayerToCompositionResult {
  bool success = false;
  Status status{};
  AppendLayerToCompositionError error = AppendLayerToCompositionError::None;
  UniString message;
 };
 class ArtifactAbstractComposition;
 using ArtifactCompositionPtr = ArtifactCore::SharedPtr<ArtifactAbstractComposition>;
 using ArtifactCompositionWeakPtr = ArtifactCore::WeakPtr<ArtifactAbstractComposition>;

 // A standalone group container owns no layer state of its own, so its switch
 // column is always a read-only aggregate of the children. Writing a switch
 // means "apply to every child", never "store a container level flag".
 enum class GroupContainerSwitch {
  Visible,
  Locked,
  Solo,
  Shy
 };

 enum class GroupContainerSwitchState {
  Off,
  Mixed,
  On
 };

 struct GroupContainerSwitchStates {
  GroupContainerSwitchState visible = GroupContainerSwitchState::Off;
  GroupContainerSwitchState locked = GroupContainerSwitchState::Off;
  GroupContainerSwitchState solo = GroupContainerSwitchState::Off;
  GroupContainerSwitchState shy = GroupContainerSwitchState::Off;
  int childCount = 0;
 };

 struct ResponsiveLayoutVariant {
  QString variantId;
  QString displayName;
  QSize baseSize;
  qreal aspectRatio = 0.0;
  QRectF safeArea = QRectF(0.0, 0.0, 1.0, 1.0);
  QPointF contentAnchor = QPointF(0.5, 0.5);
  QJsonObject layoutRules;
  bool enabled = true;

  QJsonObject toJson() const;
  static ResponsiveLayoutVariant fromJson(const QJsonObject& obj);
 };

 struct ResponsiveLayoutSet {
  QString activeVariantId;
  QString defaultPolicy = QStringLiteral("manual");
  QVector<ResponsiveLayoutVariant> variants;

  QJsonObject toJson() const;
  static ResponsiveLayoutSet fromJson(const QJsonObject& obj);
  bool hasVariant(const QString& variantId) const;
 };

 struct CompositionTransformField {
  QString fieldId;
  QString displayName = QStringLiteral("Radial Transform Field");
  QString shape = QStringLiteral("radial");
  bool enabled = true;
  QPointF center;
  qreal radius = 1.0;
  qreal secondaryRadius = 1.0;
  qreal rotationDegrees = 0.0;
  qreal timeOffsetSeconds = 0.0;
  qreal strength = 1.0;
  QString blendMode = QStringLiteral("normal");
  bool invert = false;
  qreal expansion = 0.0;
  qreal edgeScale = 1.0;
  LayerID coordinateParentLayerId;
  QVector<LayerID> targetLayerIds;

  bool targetsLayer(const LayerID& layerId) const;
  QJsonObject toJson() const;
  static CompositionTransformField fromJson(const QJsonObject& obj);
 };

 struct CompositionFieldTransformAdjustment {
  QPointF positionOffset;
  qreal scaleMultiplier = 1.0;
  bool affected = false;
 };

 struct CompositionFieldInfluenceSample {
  qreal weight = 0.0;
  bool affected = false;
 };

 struct CompositionFieldChannelSample {
  qreal weight = 1.0;
  qreal scaleMultiplier = 1.0;
  qreal timeOffsetSeconds = 0.0;
  bool affected = false;
 };

 struct CompositionStatePropertyOverride {
  LayerID layerId;
  QString propertyPath;
  QVariant value;
  QVariant baselineValue;
  bool enabled = true;

  QJsonObject toJson() const;
  static CompositionStatePropertyOverride fromJson(const QJsonObject& obj);
 };

 struct CompositionStateVariant {
  QString stateId;
  QString displayName;
  bool enabled = true;
  QVector<CompositionStatePropertyOverride> overrides;

  QJsonObject toJson() const;
  static CompositionStateVariant fromJson(const QJsonObject& obj);
  bool hasOverride(const LayerID& layerId, const QString& propertyPath) const;
 };

 struct LiveControlRecordingOptions {
  QStringList addresses;
  int sampleEveryNFrames = 1;
  double deadZone = 0.001;
  bool restoreOnCancel = true;
 };

 struct LiveControlRecordingPropertyChange {
  LayerID layerId;
  QString propertyPath;
  QVariant beforeValue;
  QVariant afterValue;
  std::vector<ArtifactCore::KeyFrame> beforeKeyframes;
  std::vector<ArtifactCore::KeyFrame> afterKeyframes;
 };

 struct CompositionAudioReactiveBinding {
  QString bindingId;
  QString source = QStringLiteral("amplitude");
  LayerID layerId;
  QString propertyPath;
  double gain = 1.0;
  double offset = 0.0;
  bool clampEnabled = false;
  double clampMinimum = 0.0;
  double clampMaximum = 1.0;
  double smoothing = 0.0;
  double attackSeconds = 0.0;
  double releaseSeconds = 0.0;
  bool invert = false;
  bool enabled = true;

  QJsonObject toJson() const;
  static CompositionAudioReactiveBinding fromJson(const QJsonObject& obj);
 };

 struct CompositionAudioReactiveMonitor {
  double rawValue = 0.0;
  double processedValue = 0.0;
  bool valid = false;
 };

 // A transition is an addressable timeline object, not a layer.  Keeping it
 // value-semantic lets existing composition snapshots remain cheap while the
 // stable id gives the UI/undo layer an independent editing target.
 struct CompositionTimelineTransition {
  QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QString name = QStringLiteral("Transition");
  QString kind = QStringLiteral("Crossfade");
  QString easing = QStringLiteral("Linear");
  QString leftClipName;
  QString rightClipName;
  LayerID leftLayerId = LayerID::Nil();
  LayerID rightLayerId = LayerID::Nil();
  // Runtime-only indices refreshed on source or composition stack changes.
  // They are deliberately excluded from JSON; stable LayerIDs are the source
  // of truth and the indices keep frame evaluation out of linear searches.
  qint32 leftLayerStackIndex = -1;
  qint32 rightLayerStackIndex = -1;
  // Insertion point in the composition layer stack. -1 follows source A's
  // current stack index; runtime indices are refreshed from stable LayerIDs.
  qint32 stackPosition = -1;
  FrameRange range = FrameRange::zero();
  bool enabled = true;
  // Runtime-only cache of the stable id bytes. Filled once when the transition
  // enters a composition so frame evaluation never has to parse the id string.
  std::array<std::uint8_t, 16> stableUuid{};

  QJsonObject toJson() const;
  static CompositionTimelineTransition fromJson(const QJsonObject& object);
  bool isOpacityOnly() const;
};

struct CompositionTimelineTransitionEvaluation {
  bool active = false;
  bool hardCut = false;
  double progress = -1.0;
  qint32 stackPosition = -1;
  qint32 leftLayerStackIndex = -1;
  qint32 rightLayerStackIndex = -1;
  qint32 insertionIndex = -1;
  // LayerID is PImpl-backed; fixed-size UUID bytes keep frame evaluation
  // allocation-free when the result is copied through render loops.
  std::array<std::uint8_t, 16> leftLayerUuid{};
  std::array<std::uint8_t, 16> rightLayerUuid{};
  std::array<std::uint8_t, 16> transitionUuid{};

  bool isSourceLayer(const LayerID& layerId) const noexcept;
  double opacityMultiplierForLayer(const LayerID& layerId) const noexcept;
  static double combinedOpacityMultiplierForLayer(
      const ArtifactCore::ArtifactArray<CompositionTimelineTransitionEvaluation>& evaluations,
      const LayerID& layerId) noexcept;
  static bool containsSourceLayer(
      const ArtifactCore::ArtifactArray<CompositionTimelineTransitionEvaluation>& evaluations,
      const LayerID& layerId) noexcept;
};

// A transition-aware draw order for one frame. TransitionComposite steps carry
// an index into the evaluation workspace and are emitted immediately before
// the layer at insertionIndex. Render paths consume Layer steps as usual;
// TransitionComposite steps are reserved for the A/B temporary composite.
struct CompositionTimelineTransitionDrawStep {
  enum class Kind : quint8 { Layer, TransitionComposite };
  Kind kind = Kind::Layer;
  qint32 layerIndex = -1;
  qint32 evaluationIndex = -1;
};

 class ArtifactAbstractComposition:public QObject, public ArtifactAbstractCompositionAccess {
 private:
  class Impl;
  Impl* impl_;
 public:
  static constexpr int LegacyColorPipelineVersion = 1;
  static constexpr int CanonicalColorPipelineVersion = 2;

  explicit ArtifactAbstractComposition(const CompositionID& id,const ArtifactCompositionInitParams& params);
  ~ArtifactAbstractComposition();
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
   AppendLayerToCompositionResult insertLayerAt(
       ArtifactAbstractLayerPtr layer, int index=0);
   void moveLayerToIndex(const LayerID& id, int newIndex);
   void removeLayer(const LayerID& id);
  void removeLayerById(const LayerID& id);
  void removeAllLayers();
  CompositionID id() const;
  std::uint64_t runtimePhysicsWorldId() const;
  int layerCount() const;
  
  CompositionSettings settings() const;
  CompositionContext& compositionContext();
  const CompositionContext& compositionContext() const;
  void setCompositionContext(const CompositionContext& context);
  void setCompositionName(const UniString& name);
  QString compositionNote() const;
  void setCompositionNote(const QString& note);
  void setCompositionSize(const QSize& size);
  int colorPipelineVersion() const noexcept;
  void setColorPipelineVersion(int version);

  ArtifactAbstractLayerPtr layerById(const LayerID& id) const;
  bool containsLayerById(const LayerID& id);
  // Rebuilds the script file watch list from the current layer bindings.
  // Called after a layer script is assigned, cleared, or reloaded.
  void syncScriptWatcher();

  ArtifactAbstractLayerPtr frontMostLayer() const;
  ArtifactAbstractLayerPtr backMostLayer() const;

   void bringToFront(const LayerID& id);
   void sendToBack(const LayerID& id);

  void setBackGroundColor(const FloatColor& color);
   FloatColor backgroundColor() const;

  void addEffect(SharedPtr<ArtifactAbstractEffect> effect);
  void removeEffect(const UniString& effectID);
  void clearEffects();
  bool moveEffect(const UniString& effectID, int newIndex);
  std::vector<SharedPtr<ArtifactAbstractEffect>> getEffects() const;
  SharedPtr<ArtifactAbstractEffect> getEffect(const UniString& effectID) const;
  int effectCount() const;
  bool hasEnabledRasterizerEffect() const;

  // Reusable automation-clip patterns (Phase 2, Bitwig-inspired). Patterns are
  // composition-owned shared curve data; layers hold instances referencing a
  // pattern id. All methods are cold-path (UI/serialization); per-frame
  // evaluation uses findAutomationClipPattern + Animation.Value pure functions.
  std::vector<ArtifactCore::AutomationClipPattern> automationClipPatterns() const;
  const ArtifactCore::AutomationClipPattern* findAutomationClipPattern(
      std::uint32_t patternId) const;
  // Adds a pattern, assigning a fresh id when pattern.id == 0. Returns 0 when
  // the pattern has no usable points.
  std::uint32_t addAutomationClipPattern(
      ArtifactCore::AutomationClipPattern pattern);
  // Removes a pattern and purges layer instances referencing it.
  bool removeAutomationClipPattern(std::uint32_t patternId);
  // Builds a pattern from a layer property's keyframes in [startSeconds,
  // endSeconds] (composition timeline seconds) and assigns an instance on the
  // same layer/path with offsetSeconds = startSeconds. Returns the new pattern
  // id, or 0 when no keyframes were found. Non-destructive: source keys stay.
  std::uint32_t createAutomationClipFromLayerKeys(
      const ArtifactCore::LayerID& layerId, const QString& targetPath,
      double startSeconds, double endSeconds, const QString& name = QString());
  // Direct (undo-free) instance replacement; UI wraps this in
  // LayerAutomationClipInstancesCommand via UndoManager.
  bool setLayerAutomationClipInstances(
      const ArtifactCore::LayerID& layerId,
      const std::vector<ArtifactCore::AutomationClipInstance>& instances);

  // Increments for every composition change. Preview caches must include this
  // value with composition id and frame number in their key.
  uint64_t revision() const noexcept;

  void changed();
  void compositionNoteChanged(const QString& note);
  FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void setTimeCode();

  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frameNumber = 0);
   
  bool isPlaying() const;
  void play();
  void pause();
  void stop();
  void togglePlayPause();
  float playbackSpeed() const;
  void setPlaybackSpeed(float speed);
  bool isLooping() const;
  void setLooping(bool loop);
  FrameRange frameRange() const;
  void setFrameRange(const FrameRange& range);
  FrameRange workAreaRange() const;
  void setWorkAreaRange(const FrameRange& range);
  FrameRate frameRate() const;
  void setFrameRate(const FrameRate& rate);
  TimeCode startTimeCode() const;
  void setStartTimeCode(const TimeCode& timecode);

  ResponsiveLayoutSet responsiveLayout() const;
  void setResponsiveLayout(const ResponsiveLayoutSet& layout);
  QString activeResponsiveLayoutVariantId() const;
  void setActiveResponsiveLayoutVariantId(const QString& variantId);
  QVector<ResponsiveLayoutVariant> responsiveLayoutVariants() const;
  QSize effectiveCompositionSize() const;

  QVector<CompositionTransformField> transformFields() const;
  void setTransformFields(const QVector<CompositionTransformField>& fields);
  void addTransformField(const CompositionTransformField& field);
  bool removeTransformField(const QString& fieldId);
  void clearTransformFields();
  QString activeTransformFieldId() const;
  void setActiveTransformFieldId(const QString& fieldId);
  CompositionFieldTransformAdjustment evaluateTransformFields(
      const LayerID& layerId, const QPointF& basePosition) const;
  CompositionFieldInfluenceSample evaluateFieldInfluence(
      const LayerID& layerId, const QPointF& samplePosition) const;
  CompositionFieldInfluenceSample evaluateFieldInfluenceAtCanvasPoint(
      const LayerID& layerId, const QPointF& canvasPosition) const;
  CompositionFieldChannelSample evaluateFieldChannelsAtCanvasPoint(
      const LayerID& layerId, const QPointF& canvasPosition) const;
  bool applyExternalControlValue(const QString& address, double rawValue, bool resetSmoothing = false);
  bool applyAudioAnalysis(const ArtifactCore::AudioAnalyzer::AnalysisResult& analysis,
                          const QString& addressPrefix = QStringLiteral("audio"),
                          bool resetSmoothing = false);
  bool beginLiveControlRecording(const LiveControlRecordingOptions& options = {});
  bool isLiveControlRecordingActive() const;
  LiveControlRecordingOptions liveControlRecordingOptions() const;
  QVector<LiveControlRecordingPropertyChange> commitLiveControlRecording();
  void cancelLiveControlRecording();
  QVector<CompositionAudioReactiveBinding> audioReactiveBindings() const;
  void setAudioReactiveBindings(
      const QVector<CompositionAudioReactiveBinding>& bindings);
  void addTimelineTransition(const CompositionTimelineTransition& transition);
  QVector<CompositionTimelineTransition> timelineTransitions() const;
  qsizetype timelineTransitionCount() const noexcept;
  void setTimelineTransitions(const QVector<CompositionTimelineTransition>& transitions);
  std::optional<CompositionTimelineTransition> timelineTransitionById(const QString& transitionId) const;
  bool removeTimelineTransition(const QString& transitionId);
  bool setTimelineTransitionEnabled(const QString& transitionId, bool enabled);
  bool setTimelineTransitionRange(const QString& transitionId, const FrameRange& range);
  bool setTimelineTransitionName(const QString& transitionId, const QString& name);
  bool setTimelineTransitionSources(const QString& transitionId,
                                    const LayerID& leftLayerId,
                                    const LayerID& rightLayerId);
  bool setTimelineTransitionKind(const QString& transitionId, const QString& kind);
  bool setTimelineTransitionEasing(const QString& transitionId, const QString& easing);
  bool setTimelineTransitionStackPosition(const QString& transitionId,
                                          qint32 stackPosition);
  bool duplicateTimelineTransition(const QString& transitionId,
                                   CompositionTimelineTransition* outDuplicate = nullptr);
  bool timelineTransitionAtFrame(qint64 frame,
                                 CompositionTimelineTransition* outTransition = nullptr) const;
  double timelineTransitionProgressAtFrame(
      qint64 frame,
      CompositionTimelineTransition* outTransition = nullptr) const;
  CompositionTimelineTransitionEvaluation evaluateTimelineTransitionAtFrame(
      qint64 frame) const;
  // Reuse the caller's capacity across frames. Reserve/update this workspace
  // outside the render loop when transition metadata changes.
  void evaluateTimelineTransitionsAtFrame(
      qint64 frame,
      ArtifactCore::ArtifactArray<CompositionTimelineTransitionEvaluation>& outEvaluations) const;
  // Frame draw order: one Layer step per composition layer plus one
  // TransitionComposite step per active transition at its insertion index.
  // Steps with the same insertion index are ordered by stable transition id.
  // Both workspaces are caller-owned; reserve them outside the render loop.
  void buildTimelineTransitionDrawPlan(
      qint64 frame,
      ArtifactCore::ArtifactArray<CompositionTimelineTransitionEvaluation>& evaluations,
      ArtifactCore::ArtifactArray<CompositionTimelineTransitionDrawStep>& outSteps) const;
  ArtifactAbstractLayerPtr resolveTimelineTransitionSource(
      const CompositionTimelineTransition& transition, bool leftSource) const;
  void clearTimelineTransitions();
  void addAudioReactiveBinding(const CompositionAudioReactiveBinding& binding);
  bool removeAudioReactiveBinding(const QString& bindingId);
  CompositionAudioReactiveMonitor evaluateAudioReactiveBindingValue(
      const QString& bindingId, double rawValue, bool resetSmoothing = false);
  bool applyAudioReactiveBindingValue(
      const QString& bindingId, double rawValue, bool resetSmoothing = false);
  CompositionAudioReactiveMonitor audioReactiveBindingMonitor(
      const QString& bindingId) const;
  QVector<CompositionStateVariant> stateVariants() const;
  void setStateVariants(const QVector<CompositionStateVariant>& states);
  void addStateVariant(const CompositionStateVariant& state);
  bool removeStateVariant(const QString& stateId);
  QString activeStateVariantId() const;
  bool setActiveStateVariantId(const QString& stateId);
  QString stateComparisonAId() const;
  QString stateComparisonBId() const;
  bool setStateComparisonPair(const QString& stateAId, const QString& stateBId);
  void evaluateLayerComponentSimulation(const FramePosition& frame,
                                        bool interactive = false);
  void resetLayerComponentSimulation();
  bool hasAuthoritativeLayerComponentSimulation() const;
  bool usesLayerComponentSimulation() const;
  QJsonObject exportLayerComponentSimulationBake() const;
  bool importLayerComponentSimulationBake(const QJsonObject& bake);
  bool hasLayerComponentSimulationSnapshot(std::int64_t frame) const;
  ArtifactCore::Optional<std::int64_t> layerComponentSimulationSnapshotAtOrBefore(
      std::int64_t frame) const;
  bool bakeLayerComponentSimulation(
      const FrameRange& range,
      const std::function<bool(std::int64_t, std::int64_t)>& progress = {});
  	
  bool hasVideo() const;
  bool hasAudio() const;
  bool isAudioOnly() const;
  bool isVisual() const;

  bool getAudio(ArtifactCore::AudioSegment &outSegment, const FramePosition &start,
                int frameCount, int sampleRate);

  /**
   * @brief Enable AudioMixer-based audio processing for this composition.
   *        When enabled, each audio-capable layer gets its own AudioBus
   *        and the mixer handles volume/pan/mute/solo routing.
   *        Must be called before getAudio() to take effect.
   */
  void ensureAudioMixer();
  ArtifactCore::SharedPtr<AudioMixer> getAudioMixer() const;
  ArtifactInOutPoints* inOutPoints() const;


  QJsonDocument toJson() const;
  static ArtifactCompositionPtr fromJson(const QJsonDocument& doc);

  QList<ArtifactAbstractLayerPtr> allLayer();
  QList<ArtifactAbstractLayerPtr> allLayer() const;
  const CompositionNodeStore& nodeStore() const;
  CompositionNodeStore& nodeStore();
  // Changes a layer's owning parent in the composition node hierarchy. The
  // target may be another layer, a standalone Group Container, or empty for
  // the composition root. Validation and hierarchy notifications stay here
  // so panels do not mutate the node store directly.
  bool setLayerNodeParent(const LayerID& layerId, const QString& parentNodeId);
  QString createGroupContainer(const QString& displayName,
                               const QVector<LayerID>& childLayerIds,
                               const QString& preferredId = {});
  bool removeGroupContainer(const QString& containerId);
  bool setGroupContainerDisplayName(const QString& containerId,
                                    const QString& displayName);
  // Timeline disclosure state. It is persisted in the node so a reopened
  // composition restores the same outline, and falls back to the caller
  // supplied value when the node predates the property.
  bool groupContainerExpanded(const QString& containerId,
                              bool defaultExpanded = true) const;
  bool setGroupContainerExpanded(const QString& containerId, bool expanded);
  // Aggregate switch read for a standalone container. A container holds no
  // switch state of its own, so every value is derived from the children: On
  // when every child is on, Off when every child is off, Mixed otherwise. This
  // is the single source of truth for the container switch column so every
  // surface renders the same value. All four switches are resolved in one
  // child walk so paint does not repeat the lookup per cell.
  GroupContainerSwitchStates groupContainerSwitchStates(
      const QString& containerId) const;
  QVector<LayerID> groupContainerChildLayerIds(const QString& containerId) const;
  const QList<ArtifactAbstractLayerPtr>& allLayerRef() const;
  QList<ArtifactAbstractLayerPtr> childLayersOf(const LayerID& parentId) const;
  bool shouldEvaluateLayer(const LayerID& layerId) const;
  // Phase 2 read-only helper: NodeStore kind check for GroupContainer.
  // Existing isGroupLayer() branches are untouched; new code should prefer
  // these resolvers which fall back to the virtual when the node is absent.
  bool isGroupContainerNode(const QString& id) const;
  bool isGroupLayerResolved(const ArtifactAbstractLayerPtr& layer) const;
  bool isGroupLayerResolved(const LayerID& id) const;
  // Multiplexer (Single) selection resolved from the GroupContainer node first,
  // falling back to the layer virtuals when the node is absent. Returns nil
  // unless the group is in exclusive (Single) mode.
  LayerID selectedChildForGroupEvaluation(const LayerID& groupId) const;
  // True when the group is in exclusive (Single) multiplexer mode.
  // GroupContainer node first, layer virtual fallback.
  bool isGroupExclusive(const LayerID& groupId) const;
  // Share-mode evaluation gain resolved from the GroupContainer node first,
  // falling back to the layer virtual when the node is absent. Mirrors
  // ArtifactGroupLayer::childEvaluationGain: 1.0 outside Share mode,
  // 1/N over visible children inside it, 0.0 for non-rendered children.
  float groupEvaluationGainForChild(const LayerID& groupId, const LayerID& childId) const;

  // Asset usage tracking for unused asset detection
  QVector<ArtifactCore::AssetID> getUsedAssets() const;

  /*Thumbnail*/
  QImage getThumbnail(int width = 128, int height = 128) const;
  // Frame-aware thumbnail: seeks the composition to `frameNumber` before
  // sampling, and bypasses the cross-frame thumbnail cache so each call
  // reflects the requested time. Used by precomp-layer rendering where the
  // child must be sampled at the parent's mapped time.
  QImage getThumbnailAtFrame(int64_t frameNumber, int width, int height);
  /*Thumbnail*/

  // Resolution Remap
  void applyResolutionRemap(const QSize& newSize, ArtifactCore::RemapPolicy policy);
 };

 typedef MultiIndexContainer<ArtifactCompositionPtr, CompositionID> ArtifactCompositionMultiIndexContainer;
 using MultiIndexLayerContainer = MultiIndexContainer<ArtifactAbstractLayerPtr, LayerID>;
};
