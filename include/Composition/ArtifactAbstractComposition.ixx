module;
#include <utility>
#include <memory>
#include <QObject>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <QVariant>
#include <vector>

export module Artifact.Composition.Abstract;
import std;
import Memory.SharedPtr;
import Utils;
import Utils.String.UniString;
import Asset.File;

import Color.Float;

import Frame.Rate;
import Frame.Range;
import Frame.Position;
import Container.MultiIndex;
import Composition.Context;
import Artifact.Composition.Access;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Composition.Settings;
import Audio.Analyze;
import Audio.Segment;
import Audio.Mixer;
import Property.Abstract;
import Artifact.Layer.Abstract;

import Geometry.ResolutionRemap;
import Artifact.Effect.Abstract;
//import Artifact.Layer.Abstract;
//import Artifact.Preview.Controller;

export namespace Artifact {

 using namespace ArtifactCore;
 class ArtifactAbstractComposition;
 using ArtifactCompositionPtr = ArtifactCore::SharedPtr<ArtifactAbstractComposition>;
 using ArtifactCompositionWeakPtr = ArtifactCore::WeakPtr<ArtifactAbstractComposition>;

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

 class ArtifactAbstractComposition:public QObject, public ArtifactAbstractCompositionAccess {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactAbstractComposition(const CompositionID& id,const ArtifactCompositionInitParams& params);
  ~ArtifactAbstractComposition();
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
   void insertLayerAt(ArtifactAbstractLayerPtr layer, int index=0);
   void moveLayerToIndex(const LayerID& id, int newIndex);
   void removeLayer(const LayerID& id);
  void removeLayerById(const LayerID& id);
  void removeAllLayers();
  CompositionID id() const;
  int layerCount() const;
  
  CompositionSettings settings() const;
  CompositionContext& compositionContext();
  const CompositionContext& compositionContext() const;
  void setCompositionContext(const CompositionContext& context);
  void setCompositionName(const UniString& name);
  QString compositionNote() const;
  void setCompositionNote(const QString& note);
  void setCompositionSize(const QSize& size);

  ArtifactAbstractLayerPtr layerById(const LayerID& id) const;
  bool containsLayerById(const LayerID& id);

  ArtifactAbstractLayerPtr frontMostLayer() const;
  ArtifactAbstractLayerPtr backMostLayer() const;

   void bringToFront(const LayerID& id);
   void sendToBack(const LayerID& id);

  void setBackGroundColor(const FloatColor& color);
   FloatColor backgroundColor() const;

  void addEffect(std::shared_ptr<ArtifactAbstractEffect> effect);
  void removeEffect(const UniString& effectID);
  void clearEffects();
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> getEffects() const;
  std::shared_ptr<ArtifactAbstractEffect> getEffect(const UniString& effectID) const;
  int effectCount() const;

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
  std::optional<std::int64_t> layerComponentSimulationSnapshotAtOrBefore(
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
  std::shared_ptr<AudioMixer> getAudioMixer() const;


  QJsonDocument toJson() const;
  static ArtifactCompositionPtr fromJson(const QJsonDocument& doc);

  QList<ArtifactAbstractLayerPtr> allLayer();
  const QList<ArtifactAbstractLayerPtr>& allLayerRef() const;
  QList<ArtifactAbstractLayerPtr> childLayersOf(const LayerID& parentId) const;
  bool shouldEvaluateLayer(const LayerID& layerId) const;

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


