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
export import Artifact.Layer.Abstract;
import Artifact.Composition.Result;
import Composition.Settings;
import Audio.Segment;
import Geometry.ResolutionRemap;
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
  	
  bool hasVideo() const;
  bool hasAudio() const;
  bool isAudioOnly() const;
  bool isVisual() const;

  bool getAudio(ArtifactCore::AudioSegment &outSegment, const FramePosition &start,
                int frameCount, int sampleRate);

  QJsonDocument toJson() const;
  static ArtifactCompositionPtr fromJson(const QJsonDocument& doc);

  QList<ArtifactAbstractLayerPtr> allLayer();
  const QList<ArtifactAbstractLayerPtr>& allLayerRef() const;

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


