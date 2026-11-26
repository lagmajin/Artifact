module;

#include <DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>

#define NOMINMAX

#define QT_NO_KEYWORDS



#include <QApplication>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>

#include <QByteArray>

#include <QColor>
#include <QDebug>

#include <QElapsedTimer>

#include <QFont>
#include <QFontDatabase>

#include <QFontMetrics>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <QFuture>

#include <QHash>

#include <QImage>

#include <QList>

#include <QLoggingCategory>

#include <QLineF>

#include <QMatrix4x4>

#include <QMetaObject>

#include <QMutex>

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <QPointer>
#include <QPointF>

#include <QRect>

#include <QRectF>
#include <QInputDialog>

#include <QSizeF>

#include <QSet>
#include <QSettings>

#include <QStringList>
#include <QString>
#include <QTextDocument>
#include <QUuid>

#include <QTimer>

#include <QTransform>
#include <QVariant>
#include <QWidget>
#include <QScreen>
#include <QtGlobal>

#include <QVector3D>
#include <QVector2D>

#include <QVector4D>

#include <QVector>
#include <opencv2/opencv.hpp>
#include <deque>
#include <cstdio>


#include <algorithm>

#include <atomic>

#include <array>

#include <chrono>

#include <cmath>

#include <cstddef>

#include <cstdint>

#include <functional>

#include <limits>

#include <memory>
#include <optional>

#include <string_view>

#include <opencv2/core.hpp>

#include <opencv2/imgproc.hpp>

#include <utility>

#include <vector>



module Artifact.Widgets.CompositionRenderController:Impl;

import Artifact.Widgets.CompositionRenderController;
import Memory.SharedPtr;
import Core.ArtifactArray;
import Core.ArtifactTuple;
import Artifact.Layer.Construction;
import Artifact.Widgets.CompositionConstructionUndoCommand;

import Artifact.Render.IRenderer;
import Graphics.RayTracingManager;
import Graphics.SurfaceColorContract;
import Material.Material;
import Artifact.Widgets.ViewportOverlay;

import Artifact.Grid.System;
import ArtifactCore.Rig2D;

import Artifact.Render.GPUTextureCacheManager;

import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionMatteSurface;
import Artifact.Render.CompositionRenderer;

import Artifact.Render.Queue.Service;

import Artifact.Render.Config;

import Settings.Accessibility;

import Artifact.Render.ROI;

import Artifact.Render.Context;
import Analyze.SmartPalette;
import Color.Harmonizer;

import Artifact.Widgets.CompositionRenderOverlay;
import Artifact.Layer.Audio;
import Audio.Effect.Spectrum;

import Artifact.Preview.Pipeline;

import Core.Camera;

import Frame.Debug;

import Graphics.RenderGraph;

import Core.Diagnostics.Trace;

import Artifact.Composition.Abstract;

import Artifact.Layer.Abstract;
import Artifact.Layer.Particle;
import Artifact.Layer.MpmSand2D;
import Artifact.Layer.EnvironmentMap;
import Artifact.Layers.Abstract._2D;

import Artifact.Layer.Clone;

import Artifact.Layer.CloneEffectSupport;

import Artifact.Layer.Camera;

import Artifact.Layer.Light;

import Core.Light;

import Artifact.Effect.Abstract;

import Artifact.Effect.Context;
import Artifact.Effect.FrameSampler;


import Artifact.Layer.Image;

import Artifact.Layer.Svg;

import Artifact.Layer.Video;

import Video.VideoFrame;

import Artifact.Layer.Solid2D;

import Artifact.Layers.SolidImage;

import Artifact.Layer.Text;

import Artifact.Layer.Composition;
import Artifact.Layer.ParametricComposition;

import Artifact.Layer.Shape;

import Layer.Matte;

import Artifact.Layers.Model3D;

import Artifact.Render.Offscreen;

import Image.ImageF32x4_RGBA;
import Image.Cryptomatte.Pixel;

import FloatRGBA;

import Frame.Position;

import Artifact.Application.Manager;

import Artifact.Layers.Selection.Manager;

import Artifact.Service.ActiveContext;

import Artifact.Widgets.TransformGizmo;

import Artifact.Widgets.TextGizmo;

import Artifact.Widgets.ContentGizmo;

import Artifact.Widgets.Gizmo3D;

import Artifact.Widgets.ViewportMath;

import Artifact.Widgets.PointTrackerGizmo;

import Artifact.Tool.PointTracker;

import Tracking.MotionTracker;

import Artifact.Render.OffscreenComposition;
import Artifact.Render.PointwiseEffectFusion;
import Artifact.Widgets.CompositionHostFrameSnapshot;

import ExposureEffect;
import HueAndSaturation;
import LevelsEffect;
import BrightnessEffect;
import InvertEffect;
import GrayscaleEffect;
import Artifact.Effect.WhiteBalance;

import Artifact.Widgets.PieMenu;

import UI.View.Orientation.Navigator;

import Geometry.CameraGuide;

import Math.Interpolate;

import Artifact.Tool.Manager;

import Artifact.Tool.MotionSketchTool;

import Artifact.Tool.PuppetTool;

import Artifact.Tool.Brush;

import Artifact.Layer.Paint;

import Artifact.Mask.LayerMask;

import Artifact.Mask.Path;

import Artifact.Project.Health;

import Utils.Id;

import Time.Rational;

import Artifact.Render.Pipeline;
import :ChannelPresentation;
import :GizmoEditing;
import :LayerDrawing;
import :MaskEditing;
import :RasterEffects;
import :ViewportGeometry;
import Artifact.Render.MotionBlurPass;
import Artifact.Render.DepthOfFieldPass;
import Artifact.Render.FinalPostProcess;
import Artifact.Color.OCIOManager;
import Artifact.Widgets.ViewportColorPipeline;

import Graphics.LayerBlendPipeline;
import Container.NamedVector;

import Graphics.GPUcomputeContext;

import Graphics.Shader.Compute.MaskCutout;

import Widgets.Utils.CSS;



import Artifact.Service.Project;
import Artifact.Project.Items;
import Artifact.Layer.InitParams;

import Artifact.Service.Playback; // 追加

import Application.AppSettings;

import Playback.State;

import Thread.Helper;

import Thread.PreciseTicker;

import Event.Bus;

import Artifact.Event.Types;

import Undo.UndoManager;
import Utils.String.UniString;
import Configuration.LayeredConfigStore;
import Configuration.ConfigLayer;

import Frame.Rate;
import Math.Vec;

import Color.Float;

import Image;

import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;

import ArtifactCore.Utils.PerformanceProfiler;
import Artifact.Render.CompositionChangeDetector;
import Artifact.Render.CompositionPassState;
import Artifact.Render.CompositionRenderPassPlan;
import Artifact.Render.CompositionRenderPassContext;
import Artifact.Render.CompositionPassExecutor;
import Artifact.Widgets.CompositionMotionPathCommands;
import Artifact.Widgets.CompositionTrackerSessionUndoCommands;
import Artifact.Widgets.CompositionOnionSkinHistory;
import Artifact.Widgets.CompositionRigUndoCommands;
import Artifact.Widgets.CompositionTextPuppetUndoCommands;
import Artifact.Widgets.CompositionLayerUndoCommands;
import Artifact.Widgets.CompositionEditUndoCommands;
import Artifact.Widgets.CompositionGizmoUndoCommands;
import Artifact.Widgets.LayerEditor.Geometry;

namespace Artifact {

namespace CompositionFrameDebugPreview {

QString previewPixelStats(const QImage& source);
void addSnapshotPreview(ArtifactCore::FrameDebugSnapshot& snapshot,
                        const QString& key, const QString& label,
                        const QString& note, const QImage& beforeImage,
                        const QImage& afterImage, const QImage& alphaImage);

} // namespace CompositionFrameDebugPreview

using CompositionFrameDebugPreview::addSnapshotPreview;
using CompositionFrameDebugPreview::previewPixelStats;

QImage makeMayaGradientSprite(const FloatColor &baseColor);
void drawViewportCheckerboardBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh, float tileSize);
void drawCompositionBackgroundDirect(
    ArtifactIRenderer *renderer, float cw, float ch, const FloatColor &bgColor,
    CompositionBackgroundMode mode, float checkerboardTileSize,
    const QImage &mayaGradientSprite, const QMatrix4x4 *viewportView = nullptr,
    const QMatrix4x4 *viewportProjection = nullptr);
void drawViewportMayaGradientBackground(ArtifactIRenderer *renderer, float vw,
                                        float vh, const FloatColor &bgColor,
                                        const QImage &mayaGradientSprite);

Ray pickingLegacyRayFromWorldRay(const WorldRay &ray) noexcept;
bool intersectModelLayerPickingRay(const Artifact3DLayer &layer,
    const WorldRay &ray, ArtifactCore::Units::WorldLength &distance);

bool isLayerEffectivelyVisible(const ArtifactAbstractLayerPtr &layer);
namespace CompositionRenderControllerDetail {}
using namespace CompositionRenderControllerDetail;

namespace CompositionRenderControllerDetail {
QFont fixedWidthFont(const int pointSize);

QImage resolveProjectRenderInputImage(const ArtifactCore::Id& projectItemId,
                                      const std::int64_t frameNumber);

}

namespace CompositionRenderControllerDetail {

std::optional<LayerMask> g_maskClipboard;

enum class QuickMaskPreset {

  Full = 0,

  LeftHalf,

  RightHalf,

  TopHalf,

  BottomHalf,

  CenterHalf,

  QuarterTL,

  QuarterTR,

  Count

};



constexpr std::array<QuickMaskPreset, static_cast<size_t>(QuickMaskPreset::Count)> kQuickMaskPresetOrder = {

    QuickMaskPreset::Full,

    QuickMaskPreset::LeftHalf,

    QuickMaskPreset::RightHalf,

    QuickMaskPreset::TopHalf,

    QuickMaskPreset::BottomHalf,

    QuickMaskPreset::CenterHalf,

    QuickMaskPreset::QuarterTL,

    QuickMaskPreset::QuarterTR,

};



// Normal-blend compositions stay on the direct 2D path by default.  The blend

// path adds a full-screen layer-to-float plus a full-screen blend pass per

// layer, so it is only a win when the direct path would rasterize or

// post-process that layer on the CPU.  This switch exists so both paths can be

// measured with the CompositionView perf log before any default changes.

bool gpuBlendForNormalCompositionEnabled();



void renderCrashTrace(const char* phase, quint64 frame, const QString& detail = {});



constexpr size_t lineDebugKindCount()

{

  return static_cast<size_t>(LineDebugKind::Unknown);

}



size_t lineDebugKindIndex(LineDebugKind kind);



bool lineDebugKindVisible(

    const std::array<bool, lineDebugKindCount()> &visibility,

    LineDebugKind kind);

QString lineDebugVisibilitySettingKey(LineDebugKind kind);



void drawTaggedSolidLine(ArtifactIRenderer *renderer, const QPointF &a,

                         const QPointF &b, const FloatColor &color,

                         float thickness, bool enabled);



void drawTaggedRectOutline(ArtifactIRenderer *renderer, const QRectF &rect,

                           const FloatColor &color, bool enabled);



void drawRectOutline(ArtifactIRenderer *renderer, const QRectF &rect,

                     const FloatColor &color, float thickness);



QRectF maskPathCanvasBounds(const MaskPath &path, const QTransform &globalTransform);



QString densityAxisDisplayName(const QString &axis);



qint64 latestTimerMs(const std::string& timerName);



enum class SelectionMode { Replace, Add, Toggle };



enum class LayerDragMode { None, Move, ScaleTL, ScaleTR, ScaleBL, ScaleBR };



enum class RectangleToolMode { None, Mask, Shape, EllipseMask, EllipseShape, StarShape, PolygonShape, TriangleShape };



QRectF dragRectFromPoints(const QPointF &start, const QPointF &end);

QRectF dragRectFromPoints(const QPointF &start, const QPointF &end,
                          bool fromCenter)
{
  if (!fromCenter) {
    return dragRectFromPoints(start, end);
  }
  const QPointF delta = end - start;
  return QRectF(start - delta, start + delta).normalized();
}



QRectF dragRectFromPoints(const QPointF &start, const QPointF &end)

{

  return QRectF(start, end).normalized();

}

QRectF dragRectFromPoints(
    ArtifactCore::Coordinates::CompositionPoint2 start,
    ArtifactCore::Coordinates::CompositionPoint2 end,
    bool fromCenter = false) {
  return dragRectFromPoints(
      ArtifactCore::Coordinates::toQPointF(start),
      ArtifactCore::Coordinates::toQPointF(end), fromCenter);
}



std::array<QPointF, 4> rectCorners(const QRectF &rect)

{

  return {rect.topLeft(), rect.topRight(), rect.bottomRight(),

          rect.bottomLeft()};

}




QString uniqueLayerNameForCurrentComposition(const QString &baseName);



namespace CompositionRenderControllerDetail {
constexpr int kFallbackPreviewIntervalMs = 16;
constexpr float kMinimumViewportZoom = 0.05f;
constexpr float kMaximumViewportZoom = 64.0f;
}

int compositionPreviewIntervalMs(const ArtifactCompositionPtr &comp);

int compositionRenderDispatchIntervalMs(const ArtifactCompositionPtr &comp);



QRectF effectExpandedLayerBounds(const ArtifactAbstractLayer *layer);



bool layerUsesTextGizmo(const ArtifactAbstractLayerPtr &layer);

bool layerUsesContentGizmo(const ArtifactAbstractLayerPtr &layer);

bool layerUsesProjectedFrameGizmo(const ArtifactAbstractLayerPtr &layer);

enum class GizmoOwner { None, Text, Content, Transform2D, Transform3D };

// One surface decision for binding, drawing and picking. Camera availability
// selects the transform representation, never the owner of text/content edits.
GizmoOwner resolveGizmoOwner(const ArtifactAbstractLayerPtr &layer,
                            bool contentEditMode, bool cameraAvailable);

bool isTransformGizmoOwner(GizmoOwner owner);

bool isScaleHandle(TransformGizmo::HandleType handle);



} // namespace







QRectF viewportRectToCanvasRect(ArtifactIRenderer *renderer,

                                const QPointF &startViewportPos,

                                const QPointF &endViewportPos);



DetailLevel detailLevelFromZoom(float zoom);



QStringList selectedLayerIdList();



bool isLayerSelected(const QStringList &selectedIds,

                     const ArtifactAbstractLayerPtr &layer);

bool passesLayerRenderFilter(CompositionLayerRenderFilter filter,
                             const QStringList &selectedIds,
                             const LayerID &selectedLayerId,
                             const ArtifactAbstractLayerPtr &layer);

// P0-4: classify a layer into a viewport category by className(). The
// cache lives in the controller's Impl so we only re-classify when the
// composition is replaced or the cache is invalidated.
CompositionViewportLayerCategory classifyLayerCategory(
    const ArtifactAbstractLayerPtr &layer);

bool passesLayerCategoryMask(CompositionViewportLayerCategoryMask mask,
                             const ArtifactAbstractLayerPtr &layer,
                             CompositionViewportLayerCategory category);



// Motion-path interaction types and commands live in
// Artifact.Widgets.CompositionMotionPathCommands.



class RendererPanZoomScope final {
 public:
  explicit RendererPanZoomScope(ArtifactIRenderer* renderer)
      : renderer_(renderer) {
    if (renderer_) {
      zoom_ = renderer_->getZoom();
      renderer_->getPan(panX_, panY_);
    }
  }

  ~RendererPanZoomScope() {
    if (renderer_) {
      renderer_->setZoom(zoom_);
      renderer_->setPan(panX_, panY_);
    }
  }

  RendererPanZoomScope(const RendererPanZoomScope&) = delete;
  RendererPanZoomScope& operator=(const RendererPanZoomScope&) = delete;

 private:
  ArtifactIRenderer* renderer_ = nullptr;
  float zoom_ = 1.0f;
  float panX_ = 0.0f;
  float panY_ = 0.0f;
};

ArtifactCompositionPtr

resolvePreferredComposition(ArtifactProjectService *service);



ArtifactAbstractLayerPtr

hitTopmostLayerAtViewportPos(const ArtifactCompositionPtr &comp,

                             ArtifactIRenderer *renderer,

                             const QPointF &viewportPos);

ArtifactAbstractLayerPtr hitNearest3DModelLayerAtPickingRay(
    const ArtifactCompositionPtr &comp, const WorldRay &ray);

struct GizmoVisualSnapshot {
  ArtifactCore::Units::EulerDegrees3 rotation{};
  ArtifactCore::Units::Scale3 scale{};
  bool is3D = false;
};

class CompositionRenderController::Impl {

public:

  std::unique_ptr<ArtifactIRenderer> renderer_;
  bool revealFallbackReported_ = false;
  CompositionViewportPresentationLayout presentationLayout_ =
      CompositionViewportPresentationLayout::Single;
  std::unique_ptr<ArtifactCore::OpenCVRotoBrushEngine> rotoBrushEngine_ =
      std::make_unique<ArtifactCore::OpenCVRotoBrushEngine>();
  cv::Mat rotoBrushMask_;
  ArtifactCore::Id rotoBrushLayerId_;
  int64_t rotoBrushFrame_ = -1;
  int rotoBrushMaskIndex_ = -1;
  std::vector<ArtifactCore::RotoBrushStroke> rotoBrushStrokes_;
  cv::Mat rotoBrushSource_;
  ArtifactCore::RotoBrushStrokeType rotoBrushStrokeType_ =
      ArtifactCore::RotoBrushStrokeType::Foreground;
  std::unique_ptr<ViewportColorPipeline> viewportColorPipeline_;
  std::unique_ptr<MotionBlurPass> motionBlurPass_;
  std::unique_ptr<DepthOfFieldPass> depthOfFieldPass_;

  Diligent::ITextureView *applyDisplayColorTransform(
      Diligent::ITextureView *source,
      Diligent::ITextureView *destination,
      int width,
      int height);

  struct PrecompGpuOutputEntry {
    void* colorTargetView = nullptr;
    void* depthTargetView = nullptr;
    Diligent::ITextureView* colorShaderResourceView = nullptr;
    QSize size;
    qint64 frame = std::numeric_limits<qint64>::min();
    quint64 generation = 0;
    bool ready = false;
  };

  struct MatteGpuOutputEntry {
    void* colorTargetView = nullptr;
    void* depthTargetView = nullptr;
    Diligent::ITextureView* colorShaderResourceView = nullptr;
    QSize size;
    qint64 frame = std::numeric_limits<qint64>::min();
    quint64 generation = 0;
    bool ready = false;
  };

  QHash<QString, PrecompGpuOutputEntry> precompGpuOutputs_;
  QHash<ArtifactCore::Id, MatteGpuOutputEntry> matteGpuOutputs_;

  void clearPrecompGpuOutputs();

  void clearMatteGpuOutputs();

  Diligent::ITextureView* renderMatteGpuOutput(
      ArtifactAbstractLayer* sourceLayer, qint64 frame, const QSize& size,
      float restoreViewportW, float restoreViewportH, float canvasW,
      float canvasH, Diligent::ITextureView* accumSRV,
      const std::function<QImage(const ArtifactCore::Id&)>& matteResolver,
      const std::vector<SceneLightEntry>& sceneLights, bool has3DCamera,
      const QMatrix4x4& cameraViewMatrix, const QMatrix4x4& cameraProjMatrix,
      RenderPipeline* renderPipeline);

  void clearCompositionSpaceGpuCache();

  bool ensureCompositionSpaceGpuCache(const QSize& size);

  PrecompGpuOutputEntry* preparePrecompGpuOutput(
      const QString& compositionId, qint64 frame, const QSize& size,
      quint64 generation);

  Diligent::ITextureView* renderPrecomp2DGpuOutput(
      ArtifactAbstractLayer* containerLayer, qint64 childFrame,
      const QSize& childSize, float restoreViewportW, float restoreViewportH,
      float restoreCanvasW, float restoreCanvasH);

  std::unique_ptr<CompositionRenderer> compositionRenderer_;

  ArtifactPreviewCompositionPipeline previewPipeline_;

  std::unique_ptr<TransformGizmo> gizmo_;

  std::unique_ptr<TextGizmo> textGizmo_;

  std::unique_ptr<ContentGizmo> contentGizmo_;

  // Content edit mode (image crop / solid size+gradient handles).
  bool contentEditMode_ = false;

  std::unique_ptr<Artifact3DGizmo> gizmo3D_;

  std::unique_ptr<ArtifactPointTrackerGizmo> trackerGizmo_;

  ArtifactCore::MotionTracker* trackerMotionTracker_ = nullptr;
  int trackerMotionTrackerId_ = 0;
  LayerID trackerSourceLayerId_;
  LayerID trackerCaptureSourceLayerId_;
  int trackerMaskIndex_ = 0;
  int trackerMaskPathIndex_ = 0;
  // Snapshot taken before a solve / clear / smoothing mutates the tracker, so
  // the operation can be pushed as an undoable command once it succeeds.
  QString trackerPreSolveJson_;
  QString trackerPreDragJson_;
  std::unique_ptr<ArtifactCore::MotionTracker> trackerSolveWorkingCopy_;
  QString trackerSolveModeTitle_;

  std::unique_ptr<Artifact::OffscreenCompositionRenderer> trackerOffscreenRenderer_;

  enum class TrackerJobDirection {
    None,
    Forward,
    Backward,
    StepForward,
    StepBackward,
    All
  };
  TrackerJobDirection trackerJobDirection_ = TrackerJobDirection::None;
  QFuture<void> trackerSolveFuture_;
  std::atomic_bool trackerCancelRequested_{false};
  std::atomic_bool trackerSolveFinished_{false};
  std::atomic_bool trackerSolveSucceeded_{false};
  bool trackerLastJobFailed_ = false;
  std::atomic<double> trackerSolveProgress_{0.0};
  quint64 trackerJobGeneration_ = 0;
  bool trackerCaptureActive_ = false;
  bool trackerPointPlacementArmed_ = false;
  int64_t trackerCaptureStartFrame_ = 0;
  int64_t trackerCaptureEndFrame_ = 0;
  int64_t trackerCaptureNextFrame_ = 0;
  int64_t trackerCapturedFrameCount_ = 0;
  int64_t trackerCaptureFrameCount_ = 0;
  int trackerCaptureFrameRetryCount_ = 0;
  double trackerFrameStep_ = 0.0;

  std::unique_ptr<ArtifactCore::LayerBlendPipeline> blendPipeline_;
  // Cached adjustment-layer mask rasterization.  Holds one composition-sized
  // image; invalidated implicitly by the key comparison at the use site.
  QImage adjustmentMaskCache_;
  AdjustmentMaskCacheKey adjustmentMaskCacheKey_;

  std::unique_ptr<ArtifactCore::MaskCutoutPipeline> maskCutoutPipeline_;

  static constexpr int kPreviewRenderPipelineSlotCount = 2;

  struct PreviewRenderPipelineSlot {

    enum class State {

      Free,

      Retirable,

      Ready,

      Submitted,

    };

    RenderPipeline pipeline;

    void* depthTargetView = nullptr;

    void* msaaColorTargetView = nullptr;

    void* msaaDepthTargetView = nullptr;

    QSize msaaTargetSize;

    QSize depthTargetSize;

    quint64 lastSubmittedFrame = 0;

    RenderDamageTracker damageTracker;

    std::uint64_t damageTileCursor = 0;

    bool retainedCompositeValid = false;

    Diligent::ITexture* retainedAccumTexture = nullptr;

    CompositionID retainedCompositionId;

    qint64 retainedFramePosition = std::numeric_limits<qint64>::min();

    QSize retainedTargetSize;

    int retainedDownsample = 0;

    float retainedCanvasWidth = 0.0f;

    float retainedCanvasHeight = 0.0f;

    float retainedZoom = 1.0f;

    float retainedPanX = 0.0f;

    float retainedPanY = 0.0f;

    State state = State::Free;

  };

  struct CompositionSpaceGpuCache {
    void* colorTargetView = nullptr;
    Diligent::ITextureView* colorShaderResourceView = nullptr;
    void* postProcessTargetView = nullptr;
    Diligent::ITextureView* postProcessShaderResourceView = nullptr;
    Diligent::ITextureView* postProcessUnorderedAccessView = nullptr;
    QSize size;
    QString contentKey;

    bool isReady() const {
      return colorTargetView && colorShaderResourceView && !size.isEmpty() &&
             !contentKey.isEmpty();
    }
  };

  struct PreviewFrameRequest {

    qint64 framePos = 0;

    qint64 buildTargetFrame = -1;

    QString compositionId;

    int previewDownsample = 1;

    int effectiveDownsample = 1;

    bool pipelineEnabled = false;

    QString priorityReason;

    quint64 buildGeneration = 0;

    bool capturedDuringPlayback = false;

  };

  std::array<PreviewRenderPipelineSlot, kPreviewRenderPipelineSlotCount>

      previewRenderPipelineSlots_;

  int activePreviewRenderPipelineSlot_ = 0;

  QString lastPreviewRenderPipelineSlotAcquireReason_ =

      QStringLiteral("initial");

  bool lastPreviewRenderPipelineAcquireHazard_ = false;

  quint64 previewRenderPipelineAcquireCount_ = 0;

  quint64 previewRenderPipelineFreeAcquireCount_ = 0;

  quint64 previewRenderPipelineReuseRetirableCount_ = 0;

  quint64 previewRenderPipelineReuseReadyCount_ = 0;

  quint64 previewRenderPipelineReuseSubmittedCount_ = 0;

  quint64 previewRenderPipelineConsecutiveReuseCount_ = 0;

  quint64 previewRenderPipelineMaxConsecutiveReuseCount_ = 0;

  bool initialized_ = false;

  bool running_ = false;

  float devicePixelRatio_ = 1.0f;

  // P0-3b.0 Render context owned by the controller. The context reflects
  // the current render mode / pan / zoom / canvas size and is exposed via
  // a read-only getter. ROI integration is deferred to P0-3b.1; this
  // stage only verifies the getter + ownership round-trip.
  RenderContext renderContext_;

  bool renderScheduled_ = false;

  // P1-5: display-only viewport exposure. Applied to a scratch surface right
  // before presentation, so the composited surface that readback paths (color
  // sampler, Color Science scopes, magnifier, RAM preview) observe is never
  // modified, and Render Queue output is unaffected. Defaults are an exact
  // identity: gain 0 stops, gamma 1, saturation 1.
  bool viewportExposureEnabled_ = true;
  float viewportExposureGain_ = 0.0f;
  float viewportExposureGamma_ = 1.0f;
  float viewportExposureSaturation_ = 1.0f;
  bool viewportClippingWarningsEnabled_ = false;
  float viewportClippingUnderThreshold_ = 0.01f;
  float viewportClippingOverThreshold_ = 1.0f;

  CompositionCompareMode compareMode_ = CompositionCompareMode::Off;
  CompositionLayerRenderFilter layerRenderFilter_ =
      CompositionLayerRenderFilter::All;
  // P0-4: per-viewport category mask (independent from layerRenderFilter_
  // so changing the mask never affects the render queue output). All is
  // the default, meaning every category is rendered.
  CompositionViewportLayerCategoryMask viewportLayerCategoryMask_ =
      static_cast<CompositionViewportLayerCategoryMask>(
          CompositionViewportLayerCategory::All);
  // Cached layer-id -> category for the active composition. Invalidated
  // when the composition changes; consulted by passesLayerCategoryMask
  // so the per-frame check is a QHash lookup, not a string compare on
  // every layer on every frame.
  QHash<LayerID, CompositionViewportLayerCategory> layerCategoryCache_;
  QString compareRestoreStateId_;
  bool stateCompareSessionActive_ = false;

  bool referencePinned_ = false;

  int referenceFrame_ = 0;



  // Fixed-rate render tick (Phase 1: infrastructure only)

  std::unique_ptr<ArtifactCore::PreciseTicker> renderTickDriver_;

  std::atomic_bool renderTickPosted_{false};
  // Diagnostic values are owned only by the ticker's worker callback.
  uint32_t renderDispatchProfileCount_ = 0;
  // Accessed only by the GUI callback; diagnostic sampling is bounded.
  uint32_t renderGuiDispatchProfileCount_ = 0;
  uint32_t renderSlowGuiDispatchProfileCount_ = 0;
  bool profilePresentStages_ = false;
  uint32_t presentStageProfileCount_ = 0;
  uint32_t renderDurationProfileCount_ = 0;
  uint32_t playbackSetupProfileCount_ = 0;
  uint32_t renderIdleStopProfileCount_ = 0;
  std::chrono::steady_clock::time_point renderDispatchPreviousTick_{};

  std::atomic_bool renderDirty_{false};

  std::atomic_bool failedFrameRetryIssued_{false};

  std::atomic_bool damageContinuationPending_{false};

  static constexpr int kRenderTickIntervalMs = 16; // ~60fps

  QElapsedTimer startupTimer_;

  bool blendPipelineReady_ = false;

  bool blendPipelineInitScheduled_ = false;

  bool blendPipelineInitInProgress_ = false;

  int blendPipelineInitAttemptCount_ = 0;

  quint64 blendPipelineInitGeneration_ = 0;

  static constexpr int kBlendPipelineMaxInitAttempts = 3;

  QString blendPipelineInitState_ = QStringLiteral("not-scheduled");

  bool gpuBlendEnabled_ =

      !qEnvironmentVariableIsSet("ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND");

  void scheduleBlendPipelineInitialization(
      CompositionRenderController* owner, int delayMs, const QString& reason);

  QString lastVideoDebug_;

  QString lastEmittedVideoDebug_;

  QString lastRenderPathSummary_;

  std::size_t pointwiseAppliedCount_ = 0;

  std::size_t giDispatchedCount_ = 0;

  // Fallback observability for multipass planning: per-frame counts of
  // precomp fast-path skips and adjustment full-quality fallbacks. Render
  // output is unchanged; the diagnostic graph reports them.
  std::size_t precompFastPathSkipCount_ = 0;
  std::size_t adjustmentFallbackCount_ = 0;

  // Overlay/composite separation phase flag. Set once the clean composite
  // is finalized (GPU path) or the backbuffer is re-armed after the direct
  // composite (fallback path); overlay entries warn in debug builds when it
  // is not set, so a future reorder that bakes gizmos into accum/readback
  // becomes visible instead of silent contamination.
  bool compositeFinalizedThisFrame_ = false;

  QString lastCompositionVisibilitySummary_;

  quint64 lastLayerVisibilityReportSerial_ = 0;

  QString lastBlendMaskSummary_;

  QString lastFrameRenderPassPlanSummary_;

  int lastFrameRenderPassPlanKey_ = -1;

  qint64 lastSetupMs_ = 0;

  qint64 lastBasePassMs_ = 0;

  qint64 lastSurfacePassMs_ = 0;

  qint64 lastMaskPassMs_ = 0;

  qint64 lastCompositePassMs_ = 0;

  qint64 lastPostPassMs_ = 0;

  qint64 lastOverlayMs_ = 0;

  qint64 lastFlushMs_ = 0;

  qint64 lastSubmit2DMs_ = 0;

  qint64 lastPresentMs_ = 0;

  QVector<QMetaObject::Connection> layerChangedConnections_;

  ArtifactCore::EventBus::Subscription compositionChangedSubscription_;
  ArtifactCore::EventBus::Subscription trackerUndoSubscription_;
  ArtifactCore::ArtifactArray<CompositionTimelineTransitionEvaluation>
      transitionEvaluationWorkspace_;
  ArtifactCore::ArtifactArray<CompositionTimelineTransitionDrawStep>
      transitionDrawStepWorkspace_;



  // 変更検出器 (差分レンダリング用)

  CompositionChangeDetector changeDetector_;



  // LOD (Level of Detail)

  std::unique_ptr<LODManager> lodManager_;

  bool lodEnabled_ = true;



  std::vector<FrameRenderPass> buildFrameRenderPassPlan(

      bool useRamPreviewFallback, bool pipelineEnabled,

      bool hasComposition) const;



  QString summarizeFrameRenderPassPlan(

      const std::vector<FrameRenderPass>& plan) const;



  ArtifactCore::ComputeRegion mapDamageRectToRenderTarget(
      const QRectF& damageRect, Diligent::Uint32 targetWidth,
      Diligent::Uint32 targetHeight) const;

  QRectF mapRenderTargetRegionToCanvas(
      const ArtifactCore::ComputeRegion& region) const;

  GpuBasePassState beginGpuBasePass(

      RenderPipeline& renderPipeline, PreviewRenderPipelineSlot& previewRenderSlot,

      float rcw, float rch, float cw, float ch, FloatColor layerBgColor,

      CompositionBackgroundMode backgroundMode,
      const QRectF* damageRect,
      ArtifactCore::ComputeRegion* outDamageRegion,
      bool* outPartialRecompose,
      QElapsedTimer* profileTimer = nullptr,
      qint64* depthClearNs = nullptr,
      qint64* accumulationClearNs = nullptr,
      qint64* partialRegionClearNs = nullptr,
      qint64* fullTargetClearNs = nullptr,
      qint64* computeUnbindNs = nullptr);



  bool seedGpuBasePassBackground(

      RenderPipeline& renderPipeline, Diligent::ITextureView* layerRTV,

      Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView*& accumSRV,

      Diligent::ITextureView*& tempUAV, int& layerToFloatConvertCount,

      int& blendDispatchCount, int& blendFailureCount, float cw, float ch,

      FloatColor layerBgColor, CompositionBackgroundMode backgroundMode,
      const ArtifactCompositionPtr& composition,
      const ArtifactCore::ComputeRegion* damageRegion = nullptr);



  void drawGpuLayerToIntermediate(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* layerRTV,

      Diligent::ITextureView* layerDepthDSV,

      Diligent::ITextureView* accumSRV, float rcw, float rch, float cw,

      float ch, DetailLevel lod, const FramePosition& currentFrame,

      const std::function<QImage(const ArtifactCore::Id&)>& matteResolver,

      const std::vector<SceneLightEntry>& sceneLights, bool has3DCamera,

      const QMatrix4x4& cameraViewMatrix, const QMatrix4x4& cameraProjMatrix,

      bool preserveSceneDepth, RenderPipeline* renderPipeline,
      Diligent::ITextureView* msaaColorRTV = nullptr,
      Diligent::ITextureView* msaaDepthDSV = nullptr,
      GpuRasterEffectPlan* outRasterEffectPlan = nullptr);



  void drawGpuLayerEmissionToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* emissionRTV,
      Diligent::ITextureView* depthDSV);



  void drawGpuLayerNormalToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* normalRTV,
      Diligent::ITextureView* depthDSV);



  void drawGpuLayerVelocityToTarget(

      ArtifactAbstractLayer* layer, Diligent::ITextureView* velocityRTV,
      Diligent::ITextureView* depthDSV);



  void drawGpuLayerIdToTarget(ArtifactAbstractLayer* layer,

                              Diligent::ITextureView* idRTV,

                              Diligent::ITextureView* depthDSV,

                              ArtifactIRenderer::ChannelType channel,

                              float encodedId);



  void drawGpuLayerAlbedoToTarget(ArtifactAbstractLayer* layer,

                                  Diligent::ITextureView* albedoRTV,
                                  Diligent::ITextureView* depthDSV);



  void drawGpuLayerPositionToTarget(ArtifactAbstractLayer* layer,

                                    Diligent::ITextureView* positionRTV,
                                    Diligent::ITextureView* depthDSV);



  void drawGpuLayerUvToTarget(ArtifactAbstractLayer* layer,

                              Diligent::ITextureView* uvRTV,
                              Diligent::ITextureView* depthDSV);



  bool applyGpuPointwiseToLayer(
      RenderPipeline& renderPipeline, Diligent::ITextureView* inputSRV,
      Diligent::ITextureView* outputUAV, Diligent::ITextureView* scratchUAV,
      const ArtifactCore::PointwiseEffectStack& stack);

  Diligent::ITextureView* prepareGpuLayerForBlend(

      ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

      Diligent::ITextureView* layerSRV, Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, Diligent::ITextureView* tempUAV,

      const GpuRasterEffectPlan* rasterEffectPlan,

      const QHash<ArtifactCore::Id, QImage>& matteSourceImages,

      const QHash<ArtifactCore::Id, Diligent::ITextureView*>& matteSourceGpuViews,

      int& layerToFloatConvertCount, bool& convertedLayerToFloat,
      const ArtifactCore::ComputeRegion* damageRegion = nullptr);



  GpuLayerBlendResult blendGpuLayerIntoAccum(

      ArtifactAbstractLayer* layer, RenderPipeline& renderPipeline,

      Diligent::ITextureView* layerSRV,

      Diligent::ITextureView* preparedBlendSRV,

      Diligent::ITextureView*& accumSRV, Diligent::ITextureView*& tempUAV,

      ArtifactCore::BlendMode blendMode, float opacity, float cw, float ch,

      int& blendDispatchCount, int& blendRetryNormalCount,

      int& blendFailureCount, int& directBlendFallbackCount,

      bool convertedLayerToFloat,
      const ArtifactCore::ComputeRegion* damageRegion = nullptr);



  Diligent::ITextureView* finalizeGpuRenderToViewport(

      RenderPipeline& renderPipeline, Diligent::ITextureView* accumSRV,

      Diligent::ITextureView* layerFloatSRV,

      Diligent::ITextureView* layerFloatUAV, float rcw, float rch, float cw,

      float ch, float origViewW, float origViewH, float origZoom,

      float origPanX, float origPanY, FloatColor bgColor,

      FloatColor layerBgColor, FloatColor origClearColor,

      CompositionBackgroundMode backgroundMode,

      int& layerToFloatConvertCount);



  PresentStageResult presentAndUpdateVideoLayers(

      const QList<ArtifactAbstractLayerPtr>& layers,

      const FramePosition& currentFrame, bool useRamPreviewFallback,

      const ArtifactRamPreviewFrameCacheState& playbackPreviewState,

      const QString& ramPreviewFallbackReason);



  LayerID selectedLayerId_;

  // A newly created layer should enter the viewport with the complete
  // transform affordance, regardless of the mode left by the previous layer.
  LayerID pendingFullGizmoLayerId_;

  bool isDraggingLayer_ = false;

  bool gizmoDragActive_ = false;
  struct GizmoInteractionSession {
    GizmoOwner owner = GizmoOwner::None;
    ArtifactAbstractLayerWeak layer;
    ArtifactCompositionWeakPtr composition;
    LayerID layerId;
    int64_t frame = 0;
    ToolType tool = ToolType::Selection;
    bool contentEditMode = false;
    TransformGizmo::Mode mode = TransformGizmo::Mode::All;
    float devicePixelRatio = 1.0f;
    QTransform viewportToCanvas;
  } gizmoSession_;
  bool gizmoSessionEnding_ = false;

  struct GizmoSessionEndGuard {
    Impl &impl;
    bool active;
    explicit GizmoSessionEndGuard(Impl &state)
        : impl(state), active(state.gizmoSession_.owner != GizmoOwner::None) {
      if (active) impl.gizmoSessionEnding_ = true;
    }
    ~GizmoSessionEndGuard() {
      if (!active) return;
      impl.gizmoSession_ = {};
      impl.gizmoSessionEnding_ = false;
      impl.gizmoDragActive_ = false;
      // Cancellation may originate in selection/tool/frame changes, without
      // a mouse release event. Release only this viewport's mouse capture.
      if (auto *host = impl.hostWidget_.data()) {
        auto *grabber = QWidget::mouseGrabber();
        if (grabber && (grabber == host || host->isAncestorOf(grabber)))
          grabber->releaseMouse();
      }
    }
    GizmoSessionEndGuard(const GizmoSessionEndGuard &) = delete;
    GizmoSessionEndGuard &operator=(const GizmoSessionEndGuard &) = delete;
  };

  GizmoOwner gizmoOwnerForLayer(const ArtifactAbstractLayerPtr &layer) const {
    if (gizmoSession_.owner != GizmoOwner::None) return gizmoSession_.owner;
    return resolveGizmoOwner(layer, contentEditMode_,
                             viewportOrientationMatricesValid_ ||
                                 gizmo3DCameraMatricesValid_);
  }

  void assertGizmoSession() const {
#ifndef QT_NO_DEBUG
    if (gizmoSessionEnding_) return;
    const bool text = textGizmo_ && textGizmo_->isDragging();
    const bool content = contentGizmo_ && contentGizmo_->isDragging();
    const bool planar = gizmo_ && gizmo_->isDragging();
    const bool spatial = gizmo3D_ && gizmo3D_->isDragging();
    Q_ASSERT(int(text) + int(content) + int(planar) + int(spatial) <= 1);
    Q_ASSERT((gizmoSession_.owner == GizmoOwner::Text) == text);
    Q_ASSERT((gizmoSession_.owner == GizmoOwner::Content) == content);
    Q_ASSERT((gizmoSession_.owner == GizmoOwner::Transform2D) == planar);
    Q_ASSERT((gizmoSession_.owner == GizmoOwner::Transform3D) == spatial);
#endif
  }

  void beginGizmoSession(GizmoOwner owner,
                         const ArtifactAbstractLayerPtr &layer) {
    Q_ASSERT(gizmoSession_.owner == GizmoOwner::None);
    const auto composition = previewPipeline_.composition();
    gizmoSession_.owner = owner;
    gizmoSession_.layer = layer;
    gizmoSession_.composition = ArtifactCompositionWeakPtr(composition);
    gizmoSession_.layerId = layer->id();
    gizmoSession_.frame = composition->framePosition().framePosition();
    auto *app = ArtifactApplicationManager::instance();
    auto *tools = app ? app->toolManager() : nullptr;
    gizmoSession_.tool = tools ? tools->activeTool() : ToolType::Selection;
    gizmoSession_.contentEditMode = contentEditMode_;
    gizmoSession_.mode = gizmoMode_;
    gizmoSession_.devicePixelRatio = devicePixelRatio_;
    const float sampleWidth = std::max(1.0f, hostWidth_);
    const float sampleHeight = std::max(1.0f, hostHeight_);
    const auto origin = renderer_->viewportToCanvas({0.0f, 0.0f});
    const auto x = renderer_->viewportToCanvas({sampleWidth, 0.0f});
    const auto y = renderer_->viewportToCanvas({0.0f, sampleHeight});
    gizmoSession_.viewportToCanvas = QTransform(
        (x.x - origin.x) / sampleWidth, (x.y - origin.y) / sampleWidth,
        (y.x - origin.x) / sampleHeight, (y.y - origin.y) / sampleHeight,
        origin.x, origin.y);
    assertGizmoSession();
  }

  bool gizmoSessionValid() const {
    if (gizmoSession_.owner == GizmoOwner::None) return true;
    const auto comp = previewPipeline_.composition();
    const auto layer = gizmoSession_.layer.lock();
    auto *app = ArtifactApplicationManager::instance();
    auto *tools = app ? app->toolManager() : nullptr;
    const bool primaryValid = comp && comp == gizmoSession_.composition.lock() && layer &&
        comp->layerById(gizmoSession_.layerId) == layer &&
        selectedLayerId_ == gizmoSession_.layerId &&
        comp->framePosition().framePosition() == gizmoSession_.frame &&
        !layer->isLocked() && !layer->isSelectionLocked() &&
        isLayerEffectivelyVisible(layer) &&
        (!tools || tools->activeTool() == gizmoSession_.tool) &&
        contentEditMode_ == gizmoSession_.contentEditMode &&
        devicePixelRatio_ == gizmoSession_.devicePixelRatio &&
        gizmoMode_ == gizmoSession_.mode;
    if (!primaryValid) return false;
    // Reuse the target snapshots already owned by the active gizmo; a removed
    // or newly locked secondary target must not keep receiving group edits.
    const auto targetValid = [&comp](const ArtifactAbstractLayerPtr &target) {
      return target && comp->layerById(target->id()) == target &&
          !target->isLocked() && !target->isSelectionLocked() &&
          isLayerEffectivelyVisible(target);
    };
    if (gizmoSession_.owner == GizmoOwner::Transform3D &&
        gizmoGroupTransformActive_) {
      for (const auto &state : gizmoGroupLayers_)
        if (!targetValid(state.layer)) return false;
    } else if (gizmoSession_.owner == GizmoOwner::Transform2D && gizmo_) {
      for (const auto &target : gizmo_->targetLayers())
        if (!targetValid(target)) return false;
    }
    return true;
  }

  // Keep planar drags on the canvas mapping captured at press. The gizmo's
  // existing API converts viewport input, so remap into the current viewport.
  QPointF planarGizmoPointer(const QPointF &physicalPointer) const {
    const QPointF canvas = gizmoSession_.viewportToCanvas.map(physicalPointer);
    const auto viewport = renderer_->canvasToViewport(
        {static_cast<float>(canvas.x()), static_cast<float>(canvas.y())});
    return QPointF(viewport.x, viewport.y);
  }
  ArtifactAbstractLayerWeak physicsDragLayer_;
  bool gizmoModalTransformActive_ = false;
  GizmoSpace gizmoModalPreviousSpace_ = GizmoSpace::World;
  QRectF projectedFrameWidthBadgeRect_;
  QRectF projectedFrameHeightBadgeRect_;
  bool projectedFrameSizeBadgesVisible_ = false;

  // In-viewport text editing session (quick edit).
  bool textEditSessionActive_ = false;
  LayerID textEditSessionLayerId_;
  QString textEditSessionStartText_;
  QString textEditSessionEditText_;
  int textEditCursorPos_ = 0;
  int textEditSelectionAnchor_ = 0;
  QString textEditPreeditText_;
  bool textEditCaretVisible_ = true;

  TransformGizmo::HandleType projectedFrameHoverHandle_ =
      TransformGizmo::HandleType::None;
  TransformGizmo::HandleType projectedFrameHandle_ =
      TransformGizmo::HandleType::None;

  bool projectedFrameMove_ = false;
  ArtifactCore::Coordinates::WorldVector3 projectedFrameStartAxisX_{
      1.0f, 0.0f, 0.0f};
  ArtifactCore::Coordinates::WorldVector3 projectedFrameStartAxisY_{
      0.0f, 1.0f, 0.0f};
  ArtifactCore::Coordinates::WorldPoint3 projectedFrameStartWorldAnchor_{};
  QMatrix4x4 projectedFrameParentWorld_;
  QMatrix4x4 projectedFrameParentWorldInverse_;
  bool projectedFrameParentWorldInvertible_ = true;
  ArtifactCore::Coordinates::LayerLocalPoint3
      projectedFrameCorrectedLocalPosition_{};
  bool projectedFrameCorrectedLocalPositionValid_ = false;
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      projectedFrameLastPointer_{};
  ProjectedFrameSnapCache projectedFrameSnapCache_;
  ArtifactCore::SharedPtr<ArtifactConstructionLayer> constructionDragLayer_;
  ConstructionHandle constructionDragHandle_;
  ConstructionItem constructionDragBefore_;
  QPointF constructionDragStart_;
  QMatrix4x4 constructionDragInverse_, constructionDragView_, constructionDragProjection_;
  QRect constructionDragViewport_;
  int64_t constructionDragFrame_ = 0;
  bool projectedFrameLastPointerValid_ = false;
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      projectedFrameScaleStartPointer_{};
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      projectedFrameScaleFixedPointer_{};
  bool projectedFrameScalePointerBasisValid_ = false;
  // SPEC 13.1: projected handle position captured at drag start. It is only
  // drawn while the pointer basis stays valid, so release/cancel clears the
  // mark without an extra reset path.
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      projectedFrameScaleStartHandlePoint_{};
  bool projectedFrameScaleStartHandlePointValid_ = false;
  bool projectedFrameSnapVerticalValid_ = false;
  bool projectedFrameSnapHorizontalValid_ = false;
  float projectedFrameSnapVertical_ = 0.0f;
  float projectedFrameSnapHorizontal_ = 0.0f;
  GizmoSpace projectedFramePreviousSpace_ = GizmoSpace::World;
  bool projectedFrameForcedLocal_ = false;
  GizmoMode projectedFramePreviousMode_ = GizmoMode::Move;
  bool projectedFrameForcedMode_ = false;

  bool puppetRotationDragging_ = false;
  QString puppetRotationPinId_;
  float puppetRotationStartAngle_ = 0.0f;
  bool puppetPinDragging_ = false;
  ArtifactCore::Coordinates::CompositionPoint2 puppetPinDragStartCanvas_{};
  QPointF puppetPinDragStartPosition_;
  QString puppetPinUndoId_;
  QJsonObject puppetLayerUndoSnapshot_;
  QJsonObject puppetPinAnimationBefore_;
  bool puppetLayerUndoSnapshotValid_ = false;
  QPointF puppetPinUndoBeforePosition_;
  float puppetPinUndoBeforeRotation_ = 0.0f;

  ArtifactAbstractLayerWeak gizmoUndoLayer_;

  GizmoVisualSnapshot gizmoUndoBefore_;
  ArtifactCore::Coordinates::WorldPoint3 gizmoUndoWorldPivotBefore_{};

  GizmoTransformSnapshot gizmoLayerTransformBefore_;
  // Anchor drag reference captured at press time.  An anchor drag rewrites the
  // layer every frame, so the delta must always be applied to the original
  // anchor/position rather than to whatever the previous frame left behind.
  ArtifactAbstractLayerPtr gizmoAnchorDragStartLayer_;
  ArtifactCore::Coordinates::LayerLocalPoint3 gizmoAnchorDragStart_{};
  ArtifactCore::Coordinates::LayerLocalPoint3 gizmoAnchorDragStartPosition_{};
  ArtifactCore::Coordinates::LayerLocalPoint3 gizmoAnchorDragCurrent_{};
  ArtifactCore::Coordinates::LayerLocalPoint3 gizmoAnchorDragCurrentPosition_{};
  bool gizmoAnchorDragHasResult_ = false;

  int64_t gizmoUndoFrame_ = 0;

  bool gizmoUndoSnapshotValid_ = false;
  QMatrix4x4 gizmoDragViewMatrix_;
  QMatrix4x4 gizmoDragProjectionMatrix_;
  bool gizmoGroupTransformActive_ = false;
  ArtifactCore::Coordinates::WorldPoint3 gizmoGroupPivotBefore_{};
  ArtifactCore::Units::EulerDegrees3 gizmoGroupRotationBefore_{};
  ArtifactCore::Units::Scale3 gizmoGroupScaleBefore_{};
  ArtifactCore::Coordinates::WorldVector3 gizmoGroupBasisX_{
      1.0f, 0.0f, 0.0f};
  ArtifactCore::Coordinates::WorldVector3 gizmoGroupBasisY_{
      0.0f, 1.0f, 0.0f};
  ArtifactCore::Coordinates::WorldVector3 gizmoGroupBasisZ_{
      0.0f, 0.0f, 1.0f};
  bool gizmoGroupProjectedBasisValid_ = false;
  std::vector<GizmoGroupLayerState> gizmoGroupLayers_;

  bool designReorderActive_ = false;

  LayerID designReorderLayerId_;

  LayerID designReorderParentId_;

  int designReorderTargetIndex_ = -1;

  bool trackerGizmoDragActive_ = false;

  bool motionSketchWasPlaying_ = false;
  ArtifactCore::Coordinates::CompositionPoint2 motionSketchLastCanvasPos_{};

  bool pendingMaskCreation_ = false;
  // Pen-tool custom shape path creation (active when the selected layer is a
  // shape layer). Mirrors the pending-mask creation flow.
  bool pendingShapePathCreation_ = false;
  LayerID pendingShapePathLayerId_;
  std::vector<CustomPathVertex> pendingShapePathVertices_;

  LayerID pendingMaskLayerId_;

  MaskPath pendingMaskPath_;

  bool maskSnapPreviewValid_ = false;

  ArtifactCore::Coordinates::CompositionPoint2 maskSnapPreviewCanvasPos_{};

  std::chrono::steady_clock::time_point quickMaskPresetStartedAt_{};

  LayerID quickMaskPresetLayerId_;

  int quickMaskPresetMaskIndex_ = -1;

  int quickMaskPresetIndex_ = -1;

  bool rectangleToolDragging_ = false;
  bool rectangleToolFromCenter_ = false;
  float rectangleToolRoundness_ = 0.0f;
  ShapeType rectangleToolShapeType_ = ShapeType::Rect;

  RectangleToolMode rectangleToolMode_ = RectangleToolMode::None;

  ArtifactCore::Coordinates::CompositionPoint2 rectangleToolStartCanvasPos_{};

  ArtifactCore::Coordinates::CompositionPoint2 rectangleToolCurrentCanvasPos_{};

  ArtifactCore::Coordinates::CompositionPoint2 brushCursorCanvasPos_{};

  ArtifactCore::Coordinates::ScreenPhysicalPoint2 brushLastViewportPos_{};

  bool brushCursorVisible_ = false;

  bool cloneStampSourceSet_ = false;
  bool cloneStampDragging_ = false;
  ArtifactAbstractLayerWeak cloneStampSourceLayer_;
  ArtifactCore::Coordinates::CompositionPoint2 cloneStampSourceCanvas_{};
  ArtifactCore::Coordinates::CompositionPoint2 cloneStampStartCanvas_{};
  ArtifactCore::Coordinates::CompositionPoint2 cloneStampLastCanvas_{};


  ArtifactAbstractLayerWeak rectangleToolTargetLayer_;

  bool penToolPreviewVisible_ = false;

  bool penMaskPreviewValid_ = false;

  Detail::float2 penMaskPreviewCanvasPos_ = {0.0f, 0.0f};

  void clearPendingMaskCreation();

  void beginPendingMaskCreation(const ArtifactAbstractLayerPtr &layer,
                                     const QPointF &localPos);

  void beginPendingShapePathCreation(const ArtifactAbstractLayerPtr &layer,
                                     const QPointF &localPos);
  void clearPendingShapePathCreation();
  bool finalizePendingShapePathCreation(const ArtifactAbstractLayerPtr &layer);
  bool removeLastPendingShapePathVertex();

  bool finalizePendingMaskCreation(const ArtifactAbstractLayerPtr &layer);

  void clearRectangleToolSession();

  void beginRectangleToolSession(RectangleToolMode mode,

                                 const ArtifactAbstractLayerPtr &layer,

                                 ArtifactCore::Coordinates::CompositionPoint2
                                     canvasPos);

  void renderMotionPathOverlayForLayer(
      const ArtifactAbstractLayerPtr &layer, const ArtifactCompositionPtr &comp,
      int currentFrameNum, float zoom, float invZoom,
      const LayerID &selectedLayerId, const QVector<LayerID> &selectedIds,
      bool hasSelectedIds, quint64 overlayInvalidationSerial,
      bool useSelectedIdFilter);



  // MayaGradient sprite cache — regenerated only when bgColor changes

  QImage cachedMayaGradientSprite_;

  FloatColor cachedMayaGradientBgColor_ = {-1.f, -1.f, -1.f, -1.f};

  FloatColor cachedMayaGradientWarmupBgColor_ = {-1.f, -1.f, -1.f, -1.f};

  QFuture<QImage> cachedMayaGradientWarmupFuture_;

  bool cachedMayaGradientWarmupQueued_ = false;

  QString lastMayaGradientDebugState_;



  // Solo layer presence cache — invalidated on any layer solo/visible change

  bool hasSoloLayerCache_ = false;

  bool soloLayerCacheDirty_ = true;



  LayerDragMode dragMode_ = LayerDragMode::None;

  TransformGizmo::Mode gizmoMode_ = TransformGizmo::Mode::All;

  QPointF dragStartCanvasPos_;

  QPointF dragStartLayerPos_;

  float dragStartScaleX_ = 1.0f;

  float dragStartScaleY_ = 1.0f;

  QRectF dragStartBoundingBox_;

  int64_t dragFrame_ = 0;

  QPointF dragAppliedDelta_;

  bool showGrid_ = false;

  bool showViewportRuler_ = QSettings().value(
      QStringLiteral("viewport/showRuler"), false).toBool();
  ViewportRulerUnit viewportRulerUnit_ = ViewportRulerUnit::Pixels;
  ViewportOriginMode viewportOriginMode_ = ViewportOriginMode::TopLeft;
  bool showPixelGrid_ = false;
  bool showOutsideComposition_ = true;

  float viewportRulerCacheZoom_ = -1.0f;
  float viewportRulerCachePanX_ = 0.0f;
  float viewportRulerCachePanY_ = 0.0f;
  float viewportRulerCacheWidth_ = 0.0f;
  float viewportRulerCacheHeight_ = 0.0f;
  float viewportRulerCacheCanvasWidth_ = 0.0f;
  float viewportRulerCacheCanvasHeight_ = 0.0f;
  std::vector<ViewportRulerTick> viewportRulerHorizontalTicks_;
  std::vector<ViewportRulerTick> viewportRulerVerticalTicks_;

  // Composition canvas fill mode, separate from the renderer's viewport clear

  // color.

  CompositionBackgroundMode compositionBackgroundMode_ =

      CompositionBackgroundMode::Checkerboard;

  float checkerboardTileSize_ = 16.0f;

  Artifact::Grid::GridSettings gridSettings_{};

  bool gridAutoStepEnabled_ = true;

  float gridAutoStepTargetViewportInterval_ = 100.0f;

  bool gridPolarMode_ = false;

  bool gridIsometricMode_ = false;

  bool showGuides_ = false;
  bool snapToGuides_ = QSettings().value(
      QStringLiteral("viewport/snapGuides"), false).toBool();

  bool showSafeMargins_ = false;

  bool showMotionPathOverlay_ = true;

  bool showEffectHitboxOverlay_ = false;

  bool showDensityHeatmapOverlay_ = false;


  ViewportChannelDisplayMode viewportChannelDisplayMode_ =
      ViewportChannelDisplayMode::Color;
  // Retain auxiliary channel views across the render pass.  These views often
  // point at ping-pong AOV textures and must remain valid until the overlay is
  // drawn after the main composition pass.
  Diligent::RefCntAutoPtr<Diligent::ITextureView> viewportChannelDisplaySRV_;

  bool showReferenceOverlay_ = false;

  bool showColorSamplerOverlay_ = false;

  bool showAutoColorPaletteOverlay_ = false;

  bool showAnchorCenterOverlay_ = false;
  bool showOriginOverlay_ = false;

  bool showCameraFrustumOverlay_ = false;

  CompositionRenderController::ViewportCullingStats viewportCullingStats_;

  QImage referenceOverlayImage_;

  bool colorSamplerHasSample_ = false;

  QColor colorSamplerColor_;

  QPoint colorSamplerImagePixel_;

  ArtifactCore::Coordinates::CompositionPoint2 colorSamplerCanvasPos_{};

  LayerID colorSamplerLayerId_;

  QVector<FloatColor> referenceDominantPalette_;

  QVector<FloatColor> referenceHarmonyPalette_;

  bool showFrameInfo_ = false; // Changed to false by default

  bool showGizmoOverlay_ = true;

  bool showXRayOverlay_ = false;

  // P1-4: attenuation applied to non-selected layers while X-Ray is on.
  // Default keeps the previous hardcoded 0.35 behavior.
  float xRayOverlayOpacity_ = 0.35f;

  // P1-4: viewport-wide shading mode for 3D mesh layers. Viewport-only
  // override; never persisted and never applied to Render Queue instances.
  CompositionViewportShadingMode viewportShadingMode_ =
      CompositionViewportShadingMode::Shaded;

  // P1-4: viewport-only backface culling. Default off preserves the existing
  // CULL_MODE_NONE behavior for assets with mismatched winding.
  bool viewportBackfaceCulling_ = false;

  // P1-14: display-only aspect mask; default off, not persisted.
  CompositionViewportAspectMask viewportAspectMask_ =
      CompositionViewportAspectMask::Off;

  bool showIsolationOverlay_ = false;
  // P1-1: viewport-only isolate set captured at enter. Layer.visible is
  // never mutated, so disabling isolation restores the previous draw set.
  QSet<LayerID> isolatedLayerIds_;
  uint32_t isolationSetRevision_ = 0;

  bool showCompositionRegionOverlay_ =

      false; // Temporarily disable the blue frame.

  bool previewOrbitActive_ = false;

  // Onion Skin

  bool showOnionSkin_ = false;

  bool showRigOverlay_ = false;
  ArtifactCore::Id selectedRigBoneId_;
  ArtifactCore::Id selectedRigControlId_;
  bool rigSelectionDragging_ = false;
  bool rigWeightPainting_ = false;
  ArtifactCore::Coordinates::LayerLocalPoint2 rigWeightLastLocalPoint_{};
  int rigWeightBoneIndex_ = 0;
  float rigWeightRadius_ = 36.0f;
  float rigWeightOpacity_ = 0.35f;
  float rigWeightFlow_ = 1.0f;
  std::vector<ArtifactCore::SkinVertex> rigWeightBeforeVertices_;
  ArtifactAbstractLayerWeak rigWeightLayer_;
  ArtifactCore::PoseSnapshot rigPoseClipboard_;
  bool rigPoseClipboardValid_ = false;
  ArtifactCore::Coordinates::LayerLocalPoint2 rigDragStartLocalPoint_{};
  float rigDragStartRotation_ = 0.0f;
  ArtifactAbstractLayerWeak rigDragLayer_;
  ArtifactCore::BoneTransform rigDragStartTransform_;
  QVariant rigDragStartControlValue_;

  bool showAudioWaveformOverlay_ = true;
  bool showAudioSpectrumOverlay_ = true;
  QString audioWaveformCacheSourcePath_;
  std::vector<float> audioWaveformCachePeaks_;
  std::vector<float> audioWaveformCacheRms_;
  qint64 audioWaveformCacheFileSize_ = -1;
  qint64 audioWaveformCacheModifiedMs_ = -1;
  QString audioSpectrumCacheSourcePath_;
  qint64 audioSpectrumCacheFileSize_ = -1;
  qint64 audioSpectrumCacheModifiedMs_ = -1;
  qint64 audioSpectrumCacheFrame_ = -1;
  std::vector<float> audioSpectrumCache_;

  int onionSkinFrameCount_ = 2;

  int onionSkinOpacity_ = 30;
  CompositionOnionSkinHistory onionSkinHistory_;

  void resetOnionSkinHistory() { onionSkinHistory_.reset(); }

  std::array<bool, lineDebugKindCount()> lineDebugVisibility_ = {

      true,  // Grid

      true,  // Axis

      true,  // Bounds

      true,  // MaskPath

      true,  // MaskHandle

      true,  // SelectionRect

      false, // MotionPath

      false, // DebugProbe

      true,  // RigBone

      true,  // RigControl

      false, // RigSkin

  };

  ArtifactCore::ViewOrientationNavigator viewportOrientationNavigator_;

  // Every Composition Viewer state has an orientation. Front is its 2D,
  // orthographic presentation; all other orientations are spatial views.
  bool isFrontOrthographicViewport() const {
    // Fly / Walk drives a free perspective camera, so the front orthographic
    // presentation (and the 2D-only overlays keyed off it) stays off while the
    // mode is active.
    if (viewportNavigationMode_ !=
        CompositionRenderController::ViewportNavigationMode::Off) {
      return false;
    }
    return viewportOrientationNavigator_.activeHotspot() ==
               ArtifactCore::ViewOrientationHotspot::Front &&
           !viewportOrientationNavigator_.isAnimating();
  }
  bool usesSpatialViewportOrientation() const {
    return viewportOrientationMatricesValid_ &&
           !isFrontOrthographicViewport();
  }
  bool viewportOrientationMatricesValid_ = false;
  QMatrix4x4 viewportOrientationViewForOverlay_;
  QMatrix4x4 viewportOrientationProjectionForOverlay_;
  // Keep the 3D gizmo on the exact camera used for 3D layer rendering.
  // This is intentionally separate from the 2D composition pan/zoom matrices.
  bool gizmo3DCameraMatricesValid_ = false;
  QMatrix4x4 gizmo3DViewMatrix_;
  QMatrix4x4 gizmo3DProjectionMatrix_;

  // Experimental viewport Fly / Walk navigation state. All of it is
  // viewport-only and cleared when the mode ends.
  CompositionRenderController::ViewportNavigationMode
      viewportNavigationMode_ =
          CompositionRenderController::ViewportNavigationMode::Off;
  QVector3D viewportNavigationEye_;
  float viewportNavigationYawDeg_ = 0.0f;
  float viewportNavigationPitchDeg_ = 0.0f;
  float viewportNavigationForward_ = 0.0f;
  float viewportNavigationStrafe_ = 0.0f;
  float viewportNavigationLift_ = 0.0f;
  float viewportNavigationBaseSpeed_ = 600.0f;
  float viewportNavigationSpeedScale_ = 1.0f;
  // Walk keeps a constant offset along the ground-plane normal chosen at
  // entry: world Y for a normal view, world Z for a (near) top view so an XY
  // canvas can be walked on instead of hovered above.
  QVector3D viewportNavigationWalkUp_ = QVector3D(0.0f, 1.0f, 0.0f);
  float viewportNavigationWalkHeight_ = 0.0f;
  std::chrono::steady_clock::time_point viewportNavigationLastTick_{};
  bool viewportNavigationSavedViewValid_ = false;
  float viewportNavigationSavedPanX_ = 0.0f;
  float viewportNavigationSavedPanY_ = 0.0f;
  float viewportNavigationSavedZoom_ = 1.0f;
  QQuaternion viewportNavigationSavedOrientation_;

  static QVector3D viewportNavigationForwardVector(const float yawDegrees,
                                                   const float pitchDegrees) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kRadiansPerDegree = kPi / 180.0f;
    const float yaw = yawDegrees * kRadiansPerDegree;
    const float pitch =
        std::clamp(pitchDegrees, -89.0f, 89.0f) * kRadiansPerDegree;
    const float cosPitch = std::cos(pitch);
    return QVector3D(std::sin(yaw) * cosPitch, std::sin(pitch),
                     -std::cos(yaw) * cosPitch);
  }

  static QMatrix4x4 viewportNavigationViewMatrixFor(
      const QVector3D& eye, const float yawDegrees,
      const float pitchDegrees) {
    const QVector3D forward =
        viewportNavigationForwardVector(yawDegrees, pitchDegrees);
    QVector3D up(0.0f, 1.0f, 0.0f);
    if (std::abs(QVector3D::dotProduct(forward, up)) > 0.999f) {
      up = QVector3D(0.0f, 0.0f, forward.y() >= 0.0f ? -1.0f : 1.0f);
    }
    QMatrix4x4 view;
    view.lookAt(eye, eye + forward, up.normalized());
    return view;
  }

  static QVector3D viewportNavigationWalkUpFor(const QVector3D& forward) {
    if (std::abs(forward.y()) > 0.9f) {
      return QVector3D(0.0f, 0.0f, forward.y() < 0.0f ? 1.0f : -1.0f);
    }
    return QVector3D(0.0f, 1.0f, 0.0f);
  }

  QMatrix4x4 viewportNavigationViewMatrix() const {
    return viewportNavigationViewMatrixFor(
        viewportNavigationEye_, viewportNavigationYawDeg_,
        viewportNavigationPitchDeg_);
  }

  // Seeds the free camera from the view the viewport is showing so entering
  // the mode does not jump the image. The derivation matches the pan / zoom /
  // orientation math used for the regular viewport camera.
  void beginViewportNavigationState(
      const CompositionRenderController::ViewportNavigationMode mode) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kFovDegrees = 45.0f;
    const float viewportW = std::max(1.0f, hostWidth_);
    const float viewportH = std::max(1.0f, hostHeight_);
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    if (renderer_) {
      zoom = std::max(0.001f, renderer_->getZoom());
      renderer_->getPan(panX, panY);
    }
    const float distance = (viewportH * 0.5f) /
        (zoom * std::tan(kFovDegrees * 0.5f * (kPi / 180.0f)));
    const float targetX = (viewportW * 0.5f - panX) / zoom;
    const float targetY = (viewportH * 0.5f - panY) / zoom;
    const QQuaternion cameraOrientation =
        viewportOrientationNavigator_.currentOrientation().conjugated();
    viewportNavigationEye_ =
        QVector3D(targetX, targetY, 0.0f) +
        cameraOrientation.rotatedVector(QVector3D(0.0f, 0.0f, 1.0f)) *
            std::max(1.0f, distance);
    const QVector3D forward =
        cameraOrientation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f));
    viewportNavigationYawDeg_ =
        std::atan2(forward.x(), -forward.z()) * (180.0f / kPi);
    viewportNavigationPitchDeg_ =
        std::asin(std::clamp(forward.y(), -1.0f, 1.0f)) * (180.0f / kPi);
    viewportNavigationWalkUp_ = viewportNavigationWalkUpFor(forward);
    viewportNavigationWalkHeight_ =
        QVector3D::dotProduct(viewportNavigationEye_, viewportNavigationWalkUp_);
    viewportNavigationBaseSpeed_ = std::max(1.0f, distance) * 0.6f;
    viewportNavigationForward_ = 0.0f;
    viewportNavigationStrafe_ = 0.0f;
    viewportNavigationLift_ = 0.0f;
    viewportNavigationLastTick_ = std::chrono::steady_clock::now();
    viewportNavigationMode_ = mode;
  }

  // Frame-rate independent movement. Called from the GUI render tick with the
  // elapsed tick time; returns true when the camera actually moved.
  bool advanceViewportNavigation(const double deltaSeconds);

  int currentFrameForOverlay_ = 0;

  // Composition frame of the last render tick.  A step that is not exactly
  // +1 (a seek, a reverse, a dropped tick) means retained temporal history is
  // no longer a neighbour and is dropped.  -1 means "not yet evaluated".
  qint64 lastEvaluatedTemporalFrame_ = -1;
  qint64 lastPresentedRamPreviewFrame_ = -1;
  CompositionID lastPresentedRamPreviewComposition_;

  quint64 renderFrameCounter_ = 0;

  std::deque<double> recentFrameTimesMs_;

  double recentFrameTimeSumMs_ = 0.0;

  double lastFrameTimeMs_ = 0.0;

  double averageFrameTimeMs_ = 0.0;

  bool renderQueueActive_ =

      false; // When true, suppress cache invalidation during Render Queue

  int lastPipelineStateMask_ = -1;

  QSize lastDispatchWarningSize_;



  PreviewRenderPipelineSlot &acquirePreviewRenderPipelineSlot();



  static QString previewRenderPipelineSlotStateText(

      const PreviewRenderPipelineSlot& slot);



  static quint64 estimatePreviewRenderPipelineSlotBytes(

      const PreviewRenderPipelineSlot& slot);



  QString previewRenderPipelineSlotsSummary() const;



  quint64 previewRenderPipelineEstimatedBytes() const;



  QString previewRenderPipelinePressureSummary() const;



  int previewRenderPipelineSlotCountByState(

      const PreviewRenderPipelineSlot::State state) const;



  QString previewRenderPipelineAcquirePolicySummary() const;



  static QString multiFramePreviewFallbackReason(

      const bool gpuBlendEnabled,

      const bool blendPipelineReady,

      const bool hasGpuBlendJustification,

      const bool hasCpuRasterizerUpload,

      const bool gpuBlendPathRequested,

      const bool renderPipelineReady,

      const bool hasDepthSlot,

      const bool acquireHazard);



  bool ensurePreviewRenderPipelineDepthSlot(

      PreviewRenderPipelineSlot &slot,

      const int width,

      const int height);

  bool ensurePreviewRenderPipelineMsaaSlot(
      PreviewRenderPipelineSlot& slot, const int width, const int height);



  static void publishPreviewFrameRequestResult(

      const QPointer<ArtifactPlaybackService>& weakPlayback,

      const PreviewFrameRequest& request,
      const QPointer<CompositionRenderController>& weakOwner,

      ArtifactCore::SharedPtr<const ArtifactCore::ImageUploadBuffer> capturedFrame);



  // Packed render key state for fast change detection.

  // Replaces the per-frame QByteArray string concatenation.

  struct RenderKeyState {

    CompositionID compId;

    quint64 baseSerial = 0;

    quint64 overlaySerial = 0;

    int64_t frame = 0;

    int32_t viewW = 0, viewH = 0, downsample = 0;

    float zoom = 0, panX = 0, panY = 0;

    float bgR = 0, bgG = 0, bgB = 0, bgA = 0;

    int32_t bgMode = 0;

    float checkerboardTileSize = 0.0f;

    float gridMajorInterval = 0.0f;

    int32_t gridSubdivisions = 0;

    uint8_t gridShowMajor = 0, gridShowMinor = 0, gridShowAxis = 0;

    float gridMajorR = 0.0f, gridMajorG = 0.0f, gridMajorB = 0.0f,

          gridMajorA = 0.0f;

    float gridMinorR = 0.0f, gridMinorG = 0.0f, gridMinorB = 0.0f,

          gridMinorA = 0.0f;

    float gridAxisR = 0.0f, gridAxisG = 0.0f, gridAxisB = 0.0f,

          gridAxisA = 0.0f;

    int32_t gizmoMode = -1, gizmoHover = -1, gizmoActive = -1;

    uint8_t gpuBlend = 0, gpuBlendReady = 0, showGrid = 0, showGuides = 0,
            showSafeMargins = 0,

            showAnchorCenter = 0, showCameraFrustum = 0, showXRay = 0,
            showIsolation = 0,

            viewportInteracting = 0;

    int32_t channelDisplay = 0;

    LayerID selectedLayerId;

    uint32_t isolationSetRevision = 0;



    bool operator==(const RenderKeyState &o) const {

      return compId == o.compId && baseSerial == o.baseSerial &&

             overlaySerial == o.overlaySerial && frame == o.frame &&

             viewW == o.viewW && viewH == o.viewH &&

             downsample == o.downsample && zoom == o.zoom && panX == o.panX &&

             panY == o.panY && bgR == o.bgR && bgG == o.bgG && bgB == o.bgB &&

             bgA == o.bgA && bgMode == o.bgMode && gizmoMode == o.gizmoMode &&

             checkerboardTileSize == o.checkerboardTileSize &&

             gridMajorInterval == o.gridMajorInterval &&

             gridSubdivisions == o.gridSubdivisions &&

             gridShowMajor == o.gridShowMajor &&

             gridShowMinor == o.gridShowMinor &&

             gridShowAxis == o.gridShowAxis &&

             gridMajorR == o.gridMajorR && gridMajorG == o.gridMajorG &&

             gridMajorB == o.gridMajorB && gridMajorA == o.gridMajorA &&

             gridMinorR == o.gridMinorR && gridMinorG == o.gridMinorG &&

             gridMinorB == o.gridMinorB && gridMinorA == o.gridMinorA &&

             gridAxisR == o.gridAxisR && gridAxisG == o.gridAxisG &&

             gridAxisB == o.gridAxisB && gridAxisA == o.gridAxisA &&

             gizmoHover == o.gizmoHover && gizmoActive == o.gizmoActive &&

             gpuBlend == o.gpuBlend && gpuBlendReady == o.gpuBlendReady &&
             showGrid == o.showGrid &&

             showGuides == o.showGuides &&

             showSafeMargins == o.showSafeMargins &&

             showAnchorCenter == o.showAnchorCenter &&

             showCameraFrustum == o.showCameraFrustum &&

             showXRay == o.showXRay &&

              viewportInteracting == o.viewportInteracting &&

              showIsolation == o.showIsolation &&
              channelDisplay == o.channelDisplay &&
              selectedLayerId == o.selectedLayerId &&
              isolationSetRevision == o.isolationSetRevision;

    }

    bool operator!=(const RenderKeyState &o) const { return !(*this == o); }

  };

  RenderKeyState lastRenderKeyState_{};
  const QUuid ramPreviewTextureOwner_ = QUuid::createUuid();
  const ArtifactCore::ArtifactStaticArray<QUuid, 32> ramPreviewResidentOwners_ = [] {
    ArtifactCore::ArtifactStaticArray<QUuid, 32> owners{};
    for (auto& id : owners) id = QUuid::createUuid();
    return owners;
  }();

  QString lastOverlayDebugSummary_;

  quint64 baseInvalidationSerial_ = 1;

  quint64 overlayInvalidationSerial_ = 1;

  RenderDamageTracker damageTracker_;

  bool compositionHasMatteDependencies_ = false;

  CompositionID damageCompositionId_;

  int64_t damageFrame_ = 0;

  int32_t damageViewportWidth_ = 0;

  int32_t damageViewportHeight_ = 0;

  int32_t damageDownsample_ = 0;

  float damageCanvasWidth_ = 0.0f;

  float damageCanvasHeight_ = 0.0f;

  bool damageStateInitialized_ = false;

  bool fullRedrawPending_ = true;

  QHash<QString, QRectF> lastLayerDamageBounds_;

  bool layerDamageBoundsInitialized_ = false;

  QRectF lastConsumedDamageRect_;

  int lastConsumedDamageLayerCount_ = 0;

  int lastConsumedDamageTileCount_ = 0;

  BoundedTileRefinementPlan lastConsumedDamageTilePlan_;

  bool lastConsumedDamageWasFullRedraw_ = false;



  // Motion path cache: avoids 300x getGlobalTransformAt() calls per frame.
  // The single entry remains the active hit-test snapshot; the per-layer map
  // keeps selected layers warm when one overlay pass visits several layers.

  struct MotionPathCacheEntry {

    LayerID layerId;

    int64_t framePos = INT64_MIN;

    float zoom = 1.0f;

    quint64 overlaySerial = UINT64_MAX;

    struct Pt {

      int frame;

      float x, y;

      int interpolation = static_cast<int>(ArtifactCore::InterpolationType::Linear);
      float inTangentX = 0.0f;
      float inTangentY = 0.0f;
      float outTangentX = 0.0f;
      float outTangentY = 0.0f;
      bool hasSpatialTangents = false;
      bool tangentsLinked = true;

    };

    std::vector<Pt> pathPoints;

    // Equal-time samples used only for the velocity-reading dots. These stay
    // independent of the adaptive path tessellation so the dot spacing keeps
    // representing motion speed even where curvature adds line samples.
    std::vector<Pt> timeDotPoints;

    std::vector<Pt> keyPoints;

    bool valid = false;

  };

  MotionPathCacheEntry motionPathCache_;

  QHash<QString, MotionPathCacheEntry> motionPathCacheByLayer_;

  bool isDraggingMotionPathKeyframe_ = false;

  // A historical fixed-Plane frame is edited without moving the playhead.
  bool isDraggingPastPlaneFrame_ = false;
  ArtifactAbstractLayerWeak historicalScaleLayer_;
  bool historicalScaleDragging_ = false;
  bool historicalScaleChanged_ = false;
  int64_t historicalScaleFrame_ = 0;
  ArtifactCore::RationalTime historicalScaleTime_;
  MotionPathScaleState historicalScaleBefore_;
  QTransform historicalScaleInverse_;
  QPointF historicalScaleAnchor_;
  QPointF historicalScaleStart_;
  // Two-node camera POI handle drag state.
  bool isDraggingCameraPoi_ = false;
  // Main-VP custom path vertex editing (Pen/Selection on shape layers).
  bool isDraggingShapePathVertex_ = false;
  bool isShapeVertexMarqueeSelecting_ = false;
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      shapeVertexMarqueeStartViewportPos_{};
  ArtifactCore::Coordinates::ScreenPhysicalPoint2
      shapeVertexMarqueeCurrentViewportPos_{};
  SelectionMode shapeVertexMarqueeSelectionMode_ = SelectionMode::Replace;
  // Line endpoint editing uses the existing shape width/height and 2D transform.
  bool isDraggingLineEndpoint_ = false;
  int draggingLineEndpoint_ = -1; // 0=start, 1=end
  ArtifactAbstractLayerWeak draggingLineLayer_;
  int draggingLineBeforeWidth_ = 0;
  int draggingLineBeforeHeight_ = 0;
  QPointF draggingLineBeforePosition_;
  float draggingLineBeforeRotation_ = 0.0f;
  int draggingShapePathVertexIndex_ = -1;
  int draggingShapePathTangent_ = 0; // 0=vertex, 1=inTangent, 2=outTangent
  int hoveredShapePathVertex_ = -1;
  int hoveredShapePathTangent_ = 0;
  int hoveredShapePathSegment_ = -1; // segment start index, -1 = none
  std::vector<int> selectedShapePathVertices_; // F4 multi-selection
  // F2: Rect/Square cornerRadius and Star innerRadius parameter handles.
  int shapeParamDragMode_ = 0; // 0 = none, 1 = cornerRadius, 2 = starInnerRadius
  ArtifactAbstractLayerWeak shapeParamDragLayer_;
  float shapeParamBefore_ = 0.0f;
  float shapeParamStart_ = 0.0f;
  float shapeParamMax_ = 0.0f;
  float shapeParamAnchorX_ = 0.0f;
  int hoveredShapeParam_ = 0; // 0 = none, 1 = cornerRadius, 2 = starInnerRadius
  // F2: custom polygon vertex drag/insert (customPathVertices stays on its
  // existing begin/update/end path above).
  bool isDraggingShapePolygon_ = false;
  int draggingShapePolygonIndex_ = -1;
  ArtifactAbstractLayerWeak shapePolygonDragLayer_;
  std::vector<QPointF> shapePolygonBefore_;
  bool shapePolygonBeforeClosed_ = true;
  bool shapePolygonDragDirty_ = false;
  // F5: operator handle drags (trim start/end/offset, single-value diamond).
  int shapeOpDragOp_ = -1;
  int shapeOpDragField_ = 0; // 1=trimStart 2=trimEnd 3=trimOffset 4=primary
  ArtifactAbstractLayerWeak shapeOpDragLayer_;
  double shapeOpBefore_ = 0.0;
  double shapeOpStartValue_ = 0.0;
  float shapeOpAnchorCanvasX_ = 0.0f;
  float shapeOpPathLength_ = 1.0f;
  int hoveredShapeOp_ = -1;
  int hoveredShapeOpField_ = 0;
  bool shapePathEditPending_ = false;
  bool shapePathEditDirty_ = false;
  ArtifactAbstractLayerWeak shapePathEditLayer_;
  std::vector<CustomPathVertex> shapePathEditBefore_;
  bool shapePathEditBeforeClosed_ = true;
  ArtifactAbstractLayerWeak draggingCameraPoiLayer_;
  ArtifactCore::Coordinates::LayerParentPoint3 cameraPoiBefore_{};
  ArtifactCore::Coordinates::WorldPoint3 cameraPoiDragPlanePoint_{};
  ArtifactCore::Coordinates::WorldVector3 cameraPoiDragPlaneNormal_{};

  ArtifactAbstractLayerWeak draggingPastPlaneLayer_;
  int64_t draggingPastPlaneFrame_ = 0;
  MotionPathPositionSnapshot draggingPastPlaneBefore_;
  QVector3D draggingPastPlaneStartHit_;

  ArtifactAbstractLayerWeak draggingMotionPathLayer_;

  int64_t draggingMotionPathFrame_ = 0;

  MotionPathPositionSnapshot draggingMotionPathBefore_;

  QVector<int64_t> selectedMotionPathFrames_;
  QVector<MotionPathKeySnapshot> draggingMotionPathGroupBefore_;
  Qt::KeyboardModifiers draggingMotionPathModifiers_ = Qt::NoModifier;
  MotionPathGroupTransform draggingMotionPathGroupTransform_ =
      MotionPathGroupTransform::Translate;
  ArtifactCore::Coordinates::LayerParentBoundsPoint2
      draggingMotionPathGroupPivot_{};
  float draggingMotionPathGroupStartAngle_ = 0.0f;
  float draggingMotionPathGroupStartRadius_ = 1.0f;

  ArtifactCore::Coordinates::LayerParentPoint2
      draggingMotionPathStartLocalPos_{};

  int hoveredMotionPathFrame_ = -1;
  MotionPathTangentHandle hoveredMotionPathTangentHandle_ =
      MotionPathTangentHandle::None;

  bool isDraggingMotionPathTangent_ = false;
  ArtifactAbstractLayerWeak draggingMotionPathTangentLayer_;
  int64_t draggingMotionPathTangentFrame_ = 0;
  MotionPathTangentHandle draggingMotionPathTangentHandle_ =
      MotionPathTangentHandle::None;
  MotionPathTangentSnapshot draggingMotionPathTangentBefore_;
  bool draggingMotionPathTangentBroken_ = false;

  bool dropGhostVisible_ = false;

  QRectF dropGhostRect_;

  QString dropGhostTitle_;

  QString dropGhostHint_;

  QString dropCandidateLabel_;

  bool infoOverlayVisible_ = false;

  QString infoOverlayTitle_;

  QString infoOverlayDetail_;

  // Transient result outline for VP Fit/Fill/Stretch operations. The comp
  // frame plus the post-operation bounds make Fit's letterbox area explicit.
  bool fitGuideVisible_ = false;
  QRectF fitGuideCompRect_;
  QRectF fitGuideResultRect_;

  bool commandPaletteVisible_ = false;

  QString commandPaletteQuery_;

  QStringList commandPaletteItems_;

  bool contextMenuVisible_ = false;

  ArtifactCore::Coordinates::ScreenPhysicalPoint2 contextMenuViewportPos_{};

  QString contextMenuTitle_;

  QString contextMenuSubtitle_;

  QStringList contextMenuItems_;

  QVector<bool> contextMenuItemEnabled_;

  int contextMenuSelectedIndex_ = -1;

  bool pieMenuVisible_ = false;

  PieMenuModel pieMenuModel_;

  ArtifactCore::Coordinates::ScreenPhysicalPoint2 pieMenuViewportPos_{};

  int pieMenuSelectedIndex_ = -1;

  bool workCursorVisible_ = false;

  WorkCursorState workCursorState_;

  QString workCursorLabel_;

  struct ViewState {
    QPointF pan;
    float zoom = 1.0f;
    QQuaternion orientation;
  };

  std::vector<ViewState> viewUndoStack_;
  std::vector<ViewState> viewRedoStack_;
  bool restoringViewState_ = false;
  bool smoothZoomActive_ = false;
  float smoothZoomStart_ = 1.0f;
  float smoothZoomTarget_ = 1.0f;
  QPointF smoothZoomAnchorViewportPx_;
  std::chrono::steady_clock::time_point smoothZoomStartedAt_{};
  quint64 smoothZoomGeneration_ = 0;
  std::chrono::steady_clock::time_point lastPanHistoryAt_{};

  ViewState captureViewState() const {
    ViewState state;
    if (renderer_) {
      float panX = 0.0f;
      float panY = 0.0f;
      renderer_->getPan(panX, panY);
      state.pan = QPointF(panX, panY);
      state.zoom = renderer_->getZoom();
    }
    state.orientation = viewportOrientationNavigator_.currentOrientation();
    return state;
  }

  void applyViewState(const ViewState &state) {
    if (!renderer_) {
      return;
    }
    restoringViewState_ = true;
    renderer_->setPan(static_cast<float>(state.pan.x()),
                      static_cast<float>(state.pan.y()));
    renderer_->setZoom(state.zoom);
    viewportOrientationNavigator_.setCurrentOrientation(state.orientation);
    restoringViewState_ = false;
    invalidateOverlayComposite();
    renderDirty_.store(true, std::memory_order_release);
    if (running_ && renderTickDriver_ && !renderTickDriver_->isRunning()) {
      renderTickDriver_->start();
    }
  }

  void pushViewHistory() {
    if (!renderer_ || restoringViewState_) {
      return;
    }
    // Fly / Walk owns a viewport-only camera. View-cube snaps, pans and zooms
    // that arrive while it is active must not append entries to the shared
    // view history: the visible camera ignores them and the entry would only
    // undo to an identical-looking state.
    if (viewportNavigationMode_ !=
        CompositionRenderController::ViewportNavigationMode::Off) {
      return;
    }
    viewUndoStack_.push_back(captureViewState());
    if (viewUndoStack_.size() > 32) {
      viewUndoStack_.erase(viewUndoStack_.begin());
    }
    viewRedoStack_.clear();
  }

  // Full-frame clear color used before composition content is drawn.

  FloatColor viewportClearColor_;

  QImage lastLayerRtPreview_;

  QImage lastAccumRtPreview_;

  QString lastLayerRtPixelStats_;

  QString lastAccumRtPixelStats_;

  // Keep the presented source view alive until the next frame.  The view may
  // refer to a ping-pong texture owned by RenderPipeline; retaining it avoids
  // a stale raw pointer when a screenshot is requested between render passes.
  Diligent::RefCntAutoPtr<Diligent::ITextureView> lastPresentedReadbackSRV_;
  ArtifactCore::SurfaceChannelOrder lastPresentedReadbackChannelOrder_ =
      ArtifactCore::SurfaceChannelOrder::RGBA;

  // Accessibility viewport magnifier (loupe).  Draws the composited frame
  // texture scaled into a scissored inset; no GPU readback is performed.
  bool magnifierEnabled_ = false;
  int magnifierScale_ = 2;
  bool magnifierFollowCursor_ = false;
  ArtifactCore::Coordinates::ScreenPhysicalPoint2 magnifierCursorViewportPos_{};
  QFont magnifierLabelFont_;
  QString magnifierLabelText_;
  int magnifierLabelScale_ = -1;
  int magnifierLabelWidth_ = 0;
  int magnifierLabelHeight_ = 0;

  FloatColor lastBgColorCache_ = {-1.f, -1.f, -1.f, -1.f};

  CompositionID lastBackgroundCompositionId_;

  QHash<QString, LayerSurfaceCacheEntry> surfaceCache_;
  bool layerSurfaceCacheDiagnosticsEnabled_ = false;
  QHash<QString, quint64> layerSurfaceCacheHitCounts_;
  QHash<QString, quint64> layerSurfaceCacheMissCounts_;
  QHash<QString, quint64> layerSurfaceCacheInvalidationCounts_;
  QHash<QString, quint64> layerSurfaceCacheBuildCounts_;
  SolidPointwisePreviewCache solidPointwiseCache_;

  QHash<QString, quint64> surfaceGenerations_;

  CompositionSpaceGpuCache compositionSpaceGpuCache_;

  const bool compositionSpaceGpuCachePresentationReady_ =
      QSettings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"))
          .value(QStringLiteral("Render/Experimental/CompositionSpaceGpuCache"),
                 false)
          .toBool();

  const bool tgfxPartialRecomposeEnabled_ =
      QSettings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"))
          .value(QStringLiteral("Render/Experimental/TGFXPartialRecompose"),
                 false)
          .toBool();

  std::unique_ptr<GPUTextureCacheManager> gpuTextureCacheManager_;

  // Render debounce timer: coalesces rapid LayerChangedEvent notifications

  // into a single renderOneFrame() call, preventing GPU saturation during drag

  static constexpr qint64 kRenderDebounceIntervalMs = 16; // ~60fps baseline



  // Render tick infrastructure

  bool renderInProgress_ = false;  // Guards against concurrent renderOneFrameImpl() calls
  std::atomic<quintptr> lastFrameExecutionThreadId_{0};
  std::atomic<quint64> sceneMutationGeneration_{1};

  QElapsedTimer gizmoDragRenderTimer_;  // Throttles render frequency during gizmo drag



  // Guide positions (composition-space pixels)

  QVector<float> guideVerticals_;   // X positions

  QVector<float> guideHorizontals_; // Y positions

  float lastCanvasWidth_ = 1920.0f;

  float lastCanvasHeight_ = 1080.0f;



  // Resolution scaling

  int previewDownsample_ = 1;
  // P0-3d: remember the user-selected preset so D4 (Preview-only while IRR
  // is active) can restore it on clear. Mirrors previewDownsample_ but
  // keeps the enum so the downgrade/restore does not have to reverse-
  // engineer the integer factor.
  PreviewQualityPreset previewQualityPreset_ = PreviewQualityPreset::Final;
  bool irrForcedPreview_ = false;
  // Diagnostics-only counters exposed for future debug overlays.
  RenderQuality lastPartialRenderQuality_ = RenderQuality::Final;
  quint64 partialRenderCount_ = 0;

  int interactivePreviewDownsampleFloor_ = 4;

  float hostWidth_ = 0.0f;

  float hostHeight_ = 0.0f;

  QPointer<QWidget> hostWidget_;

  /// True while the user is actively interacting with the viewport.
  ///
  /// Drives continuous redraw and gates Detached Task execution.  Keep the
  /// single definition so both callers cannot drift apart.
  bool interactionBusy() const {
    return viewportInteracting_ || isRubberBandSelecting_ ||
           isShapeVertexMarqueeSelecting_ || dropGhostVisible_ ||
           isBoxZooming_ || interactiveRenderRegionHandleDrag_ != 0 ||
           gizmoSession_.owner != GizmoOwner::None;
  }

  bool viewportInteracting_ = false;

  // POD samples only on the input/render thread. No formatting or file I/O.
  struct InteractionPerfSample {
    qint64 elapsedUs, frameUs, setupMs, baseMs, layerMs, overlayMs, flushMs, presentMs;
    quint64 draws;
    float width, height, zoom, panX, panY;
    int downsample, action;
  };
  struct InteractionPerfBatch {
    std::array<InteractionPerfSample, 1024> samples{};
    std::size_t count = 0;
    quint64 dropped = 0;
    struct Event { qint64 elapsedUs; int kind; };
    std::array<Event, 4096> events{};
    std::size_t eventCount = 0;
    quint64 eventsDropped = 0;
  };
  // Opt in by placing this empty marker next to Artifact.exe. Keeping the
  // switch beside the executable makes diagnostics available from Explorer,
  // IDE, and packaged launches without changing their environment.
  const bool interactionPerfEnabled_ = []() {
    const QString markerPath = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("ArtifactViewportPerf.enable"));
    return QFileInfo::exists(markerPath);
  }();
  InteractionPerfBatch interactionPerf_;
  QElapsedTimer interactionPerfTimer_;
  QFuture<void> interactionPerfWrite_;
  int interactionPerfAction_ = 0; // 1=pan, 2=zoom, 3=mixed
  unsigned interactionPerfFiles_ = 0;

  void recordInteractionPerfEvent(int kind) {
    if (!interactionPerfEnabled_ || interactionPerfAction_ == 0 || interactionPerfFiles_ >= 64)
      return;
    if (!interactionPerfTimer_.isValid()) interactionPerfTimer_.start();
    if (interactionPerfWrite_.isRunning() || interactionPerf_.eventCount == interactionPerf_.events.size()) {
      ++interactionPerf_.eventsDropped;
      return;
    }
    interactionPerf_.events[interactionPerf_.eventCount++] = {
        interactionPerfTimer_.nsecsElapsed() / 1000, kind};
  }

  QElapsedTimer viewportInteractionElapsedTimer_;

  int viewportInteractionIdleMs_ = 120;

  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();

  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QSize pendingResizeSize_;

  bool isRubberBandSelecting_ = false;
  bool isLassoSelecting_ = false;

  bool dragGroupMove_ = false;

  ArtifactCore::Coordinates::ScreenPhysicalPoint2 rubberBandStartViewportPos_{};

  ArtifactCore::Coordinates::ScreenPhysicalPoint2 rubberBandCurrentViewportPos_{};
  QVector<ArtifactCore::Coordinates::ScreenPhysicalPoint2>
      lassoViewportPoints_;

  SelectionMode selectionMode_ = SelectionMode::Replace;

  // P0-1 Box zoom/crop interaction state. Mutually exclusive with the
  // rubber-band selection marquee. Only honors beginBoxZoomInteraction()
  // while usesSpatialViewportOrientation() or front orthographic are both
  // valid for navigation; the overlay stays visible until release/cancel.
  bool isBoxZooming_ = false;
  bool boxZoomCropWindow_ = false;
  ArtifactCore::Coordinates::ScreenPhysicalPoint2 boxZoomStartViewportPos_{};
  ArtifactCore::Coordinates::ScreenPhysicalPoint2 boxZoomCurrentViewportPos_{};

  // P0-2 Tumble-pivot-under-cursor state. Camera layer parameters are not
  // touched: tumblePivotCanvasPos_ is fed into viewportOrientationViewMatrix
  // as the temporary view target. Cleared on composition reset / view undo.
  bool tumblePivotOverrideEnabled_ = false;
  ArtifactCore::Coordinates::CompositionPoint2 tumblePivotCanvasPos_{};

  // P0-3a Interactive Render Region state. Canvas pixel rectangle that the
  // viewport exposes as a partial-render area. Resolution slider is stored
  // for the HUD readout; RenderContext::roi wiring is deferred to P0-3b.
  bool interactiveRenderRegionActive_ = false;
  QRectF interactiveRenderRegionRect_;
  float interactiveRenderRegionResolutionScale_ = 1.0f;
  // 2D modal handle interaction state for moving/resizing the IRR rect.
  // 0 = none, 1 = move, 2-9 = corner/edge handles (NW, N, NE, E, SE, S, SW, W).
  int interactiveRenderRegionHandleDrag_ = 0;
  ArtifactCore::Coordinates::ScreenLogicalPoint2
      interactiveRenderRegionDragStartViewport_{};
  QRectF interactiveRenderRegionDragStartRect_;

  QVector<ArtifactAbstractLayerPtr> dragGroupLayers_;

  QHash<QString, QPointF> dragGroupStartPositions_;



  // Live field editing state

  bool isDraggingTransformField_ = false;

  QString draggingTransformFieldId_;

  CompositionTransformField draggingTransformFieldBefore_;

  TransformFieldDragMode draggingTransformFieldMode_ =
      TransformFieldDragMode::None;

  QString hoveredTransformFieldId_;

  TransformFieldDragMode hoveredTransformFieldMode_ =
      TransformFieldDragMode::None;




  // Mask editing state

  int hoveredMaskIndex_ = -1;

  int hoveredPathIndex_ = -1;

  int hoveredVertexIndex_ = -1;

  int hoveredMaskHandleType_ = -1;

  int draggingMaskIndex_ = -1;

  int draggingPathIndex_ = -1;

  int draggingVertexIndex_ = -1;

  ArtifactCore::Coordinates::LayerLocalPoint2 dragStartVertexLocal_{};

  std::vector<MaskVertexAddress> selectedMaskVertices_;
  bool maskProportionalEditingEnabled_ = false;
  float maskProportionalEditRadius_ = 120.0f;
  bool maskRubberBandCandidate_ = false;
  bool isMaskRubberBandSelecting_ = false;
  ArtifactCore::Coordinates::CompositionPoint2 maskRubberBandStartCanvas_{};
  ArtifactCore::Coordinates::CompositionPoint2 maskRubberBandCurrentCanvas_{};
  bool textToolCandidate_ = false;
  bool textToolDragging_ = false;
  ArtifactCore::Coordinates::CompositionPoint2 textToolStartCanvas_{};
  ArtifactCore::Coordinates::CompositionPoint2 textToolCurrentCanvas_{};

  int draggingMaskHandleType_ = -1;

  ArtifactCore::Coordinates::LayerLocalPoint2 draggingMaskHandleStartLocal_{};

  ArtifactCore::Units::LayerLocalLength draggingMaskHandleStartFeather_{};

  bool isDraggingVertex_ = false;

  bool isDraggingMaskHandle_ = false;

  bool isDraggingMaskGeometry_ = false;

  ArtifactCore::Coordinates::LayerLocalPoint2 draggingMaskGeometryStartLocal_{};

  ArtifactCore::Units::LayerLocalLength draggingMaskGeometryStartFeather_{};

  ArtifactCore::Units::LayerLocalLength draggingMaskGeometryStartExpansion_{};

  bool draggingMaskGeometryExpansion_ = false;

  bool maskEditPending_ = false;

  bool maskEditDirty_ = false;

  ArtifactAbstractLayerWeak maskEditLayer_;

  std::vector<LayerMask> maskEditBefore_;

  std::chrono::steady_clock::time_point lastLayerMutationNotify_{};

  bool maskViewNavigationLocked() const {
    const auto *app = ArtifactApplicationManager::instance();
    const auto *toolManager = app ? app->toolManager() : nullptr;
    if (toolManager && toolManager->activeTool() == ToolType::Pen) {
      return true;
    }
    return maskEditPending_ ||
           rectangleToolMode_ == RectangleToolMode::Mask ||
           rectangleToolMode_ == RectangleToolMode::EllipseMask;
  }



  // Cyclic selection state

  QPointF lastHitPosition_;

  LayerID lastHitLayerId_;



  // ROI debug state

  bool debugMode_ = false; // ROI デバッグ表示フラグ



  void beginMaskEditTransaction(const ArtifactAbstractLayerPtr &layer);



  void markMaskEditDirty();



  void resetLayerMutationNotify();



  bool shouldPublishLayerMutation();



  void publishLayerModified(const ArtifactAbstractLayerPtr& layer, bool force = false);



  bool commitMaskEditTransaction();



  bool cyclePresetLayerMaskForLayer(const ArtifactAbstractLayerPtr &layer,

                                    bool reverse);



  void beginMotionPathDrag(const ArtifactAbstractLayerPtr &layer,

                           int64_t frame,
                           ArtifactCore::Coordinates::CompositionPoint2 canvasPos,
                           const MotionPathPositionSnapshot &before,
                           Qt::KeyboardModifiers modifiers = Qt::NoModifier);



  void clearMotionPathDragState();



  bool setHoveredMotionPathFrame(int frame);

  bool hitTestMotionPathTangent(const QPointF &canvasPos, float threshold,
                                int64_t &outFrame,
                                MotionPathTangentHandle &outHandle) const;

  void beginMotionPathTangentDrag(const ArtifactAbstractLayerPtr &layer,
                                  int64_t frame,
                                  MotionPathTangentHandle handle,
                                  Qt::KeyboardModifiers modifiers =
                                      Qt::NoModifier);

  void clearMotionPathTangentDragState();

  bool applyMotionPathTangentDrag(const QPointF &canvasPos);



  void beginPastPlaneFrameDrag(const ArtifactAbstractLayerPtr &layer,
                               int64_t frame, const QVector3D &startHit,
                               const MotionPathPositionSnapshot &before);

  void clearPastPlaneFrameDrag();

  void beginCameraPoiDrag(const ArtifactAbstractLayerPtr &layer,
                          ArtifactCore::Coordinates::LayerParentPoint3 before,
                          ArtifactCore::Coordinates::WorldPoint3 planePoint,
                          ArtifactCore::Coordinates::WorldVector3 planeNormal);

  void clearCameraPoiDrag();

  // Intersect a picking ray with the POI drag plane. The plane passes through
  // the POI start position and faces the viewing camera, so the drag maps
  // pointer motion into the world plane the user sees.
  bool applyCameraPoiDrag(
      const WorldRay &ray,
      ArtifactCore::Coordinates::WorldPoint3 &outPoi) const;

  bool applyPastPlaneFrameDrag(const Ray &ray);

  bool applyMotionPathDrag(
      const ArtifactAbstractLayerPtr &layer,
      ArtifactCore::Coordinates::CompositionPoint2 canvasPos);



  void sync2DGizmosForLayer(const ArtifactAbstractLayerPtr &layer);



  void

  syncSelectedLayerOverlayState(const ArtifactCompositionPtr &composition) {

    ArtifactAbstractLayerPtr layer;

    if (composition && !selectedLayerId_.isNil()) {

      layer = composition->layerById(selectedLayerId_);

    }



    sync2DGizmosForLayer(layer);



    if (gizmo3D_) {

      if (layer) {

        syncGizmo3DFromLayer(layer);

      } else {

        gizmo3D_->resetState();

      }

    }

  }



  void syncGizmo3DFromLayer(const ArtifactAbstractLayerPtr &layer);



  QRectF rubberBandCanvasRect() const {
    return viewportRectToCanvasRect(
        renderer_.get(),
        ArtifactCore::Coordinates::toQPointF(rubberBandStartViewportPos_),
        ArtifactCore::Coordinates::toQPointF(rubberBandCurrentViewportPos_));
  }

  QRectF shapeVertexMarqueeCanvasRect() const {
    return viewportRectToCanvasRect(
        renderer_.get(),
        ArtifactCore::Coordinates::toQPointF(
            shapeVertexMarqueeStartViewportPos_),
        ArtifactCore::Coordinates::toQPointF(
            shapeVertexMarqueeCurrentViewportPos_));
  }

  QPolygonF lassoCanvasPolygon() const {
    QPolygonF polygon;
    if (!renderer_ || lassoViewportPoints_.size() < 3) {
      return polygon;
    }
    polygon.reserve(lassoViewportPoints_.size());
    for (const auto &point : lassoViewportPoints_) {
      const auto canvas = renderer_->viewportToCanvas(
          {point.x, point.y});
      polygon.push_back(QPointF(canvas.x, canvas.y));
    }
    return polygon;
  }



  void clearSelectionGestureState();



  void clearTransformFieldDragState();



  void clearTransformFieldHoverState();



  bool beginTransformFieldDrag(const ArtifactCompositionPtr &comp,
                               const QString &fieldId,
                               TransformFieldDragMode mode);



  bool applyTransformFieldDrag(const ArtifactCompositionPtr &comp,
                               const QPointF &canvasPos);



  bool updateTransformFieldHover(const ArtifactCompositionPtr &comp,
                                 const QPointF &canvasPos,
                                 const QSet<LayerID> &selectedLayerIds,
                                 qreal hitThreshold);



  void applyCompositionState(const ArtifactCompositionPtr &composition);



  void bindCompositionChanged(CompositionRenderController *owner,

                              const ArtifactCompositionPtr &composition);



  void invalidateLayerSurfaceCache(const ArtifactAbstractLayerPtr &layer);



  void recordLayerDamage(

      const ArtifactAbstractLayerPtr &layer,

      LayerInvalidationRegion::Source source);



  void resetLayerDamageBounds(const ArtifactCompositionPtr& composition);



  void invalidateBaseComposite(bool preserveLayerDamage = false,
                               bool preserveViewportTransform = false);



  void invalidateOverlayComposite();



  void invalidateMotionPathCache();



  RenderDamageTracker& damageTracker() { return damageTracker_; }

  void scheduleFailedFrameRetry();

  void markAllPreviewSlotsFullRedraw();



  quint64 surfaceGeneration(const ArtifactAbstractLayer *layer) const {

    return layer ? surfaceGenerations_.value(layer->id().toString(), 1) : 1;

  }



  using HostFrameSnapshot = CompositionHostFrameSnapshot;

  void drawViewportGhostOverlay(const HostFrameSnapshot &hostSnapshot,

                                const ArtifactCompositionPtr &comp,

                                const ArtifactAbstractLayerPtr &selectedLayer,

                                const FramePosition &currentFrame);

  void drawCameraFrustumOverlay(
      ArtifactIRenderer *renderer,
      const CompositionRenderController::CameraFrustumVisual &visual,
      const QMatrix4x4 &viewportView, const QMatrix4x4 &viewportProjection,
      bool activeCamera);

  QRectF commandPaletteRect() const;

  QRectF contextMenuRect() const;

  QRectF pieMenuRect() const;

  QRectF viewportOverlayItemRect(int index) const;

  int viewportOverlayItemAt(
      ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos) const;

  int pieMenuItemAt(
      ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos) const;

  void drawPieMenuOverlay();

  void updateContextMenuOverlayMousePos(
      ArtifactCore::Coordinates::ScreenPhysicalPoint2 viewportPos);

  void drawViewportOverlayPass(const HostFrameSnapshot &hostSnapshot,

                               const ArtifactCompositionPtr &comp,

                               ArtifactCameraLayer *activeCamera,

                               const QList<ArtifactAbstractLayerPtr> &layers,

                               const QStringList &selectedIds,

                               const FramePosition &currentFrame, float cw,

                               float ch, bool has3DCamera,

                               const QMatrix4x4 &cameraViewMatrix,

                               const QMatrix4x4 &cameraProjMatrix);

  void drawViewportCanvasOverlay(const HostFrameSnapshot &hostSnapshot,
                                 float cw, float ch);
  void drawThreeDimensionalGroundGrid();

  void drawReferenceOverlayImage(float canvasWidth, float canvasHeight);
  void drawViewportChannelOverlayImage(float canvasWidth, float canvasHeight);
  void syncViewportChannelReadbackConfiguration();
  void drawOnionSkinOverlay(const ArtifactCompositionPtr &composition,
                            const FramePosition &currentFrame,
                            float canvasWidth, float canvasHeight);
  bool hasCalculatedOnionSkinSource(
      const ArtifactCompositionPtr &composition) const;
  void queueOnionSkinCapture(CompositionRenderController *owner,
                             const ArtifactCompositionPtr &composition,
                             const FramePosition &currentFrame,
                             float canvasWidth, float canvasHeight);

  bool updateColorSamplerOverlay(CompositionRenderController *owner,
                                 const QPointF &viewportPos);

  void rebuildReferencePaletteOverlay();

  void drawColorSamplerOverlay(int overlayW, int overlayH);

  bool magnifierLoupeRect(QRectF &outRect) const;

  void drawViewportMagnifierOverlay(float cw, float ch);

  void drawAutoColorPaletteOverlay(int overlayW, int overlayH);

  void drawViewportGuideOverlay(const ArtifactCompositionPtr &comp, float cw,

                                float ch);

  void drawViewportInteractionOverlay(
                                      const HostFrameSnapshot &hostSnapshot,

                                      const FramePosition &currentFrame,

                                      float cw, float ch);

  static void drawTextSessionOverlay(
      ArtifactIRenderer &renderer,
      const ArtifactCore::SharedPtr<ArtifactTextLayer> &textLayer,
      const QString &editText, int cursorPos, int selectionAnchor,
      const QString &preeditText, bool caretVisible);

  void drawSelectionEditingOverlay(

      const HostFrameSnapshot &hostSnapshot,

      const ArtifactCompositionPtr &comp,

      const QList<ArtifactAbstractLayerPtr> &layers,

      const QStringList &selectedIds,

      const FramePosition &currentFrame, float cw, float ch,

      bool has3DCamera, const QMatrix4x4 &cameraViewMatrix,

      const QMatrix4x4 &cameraProjMatrix);

  void drawViewportUiOverlay();



  // 変更検出器へのアクセス (デバッグ用)

  const CompositionChangeDetector &changeDetector() const {

    return changeDetector_;

  }



  CompositionChangeDetector &changeDetector() { return changeDetector_; }



  HostFrameSnapshot captureHostFrameSnapshot(
      CompositionRenderController *owner);
  bool prepareHostFrame(CompositionRenderController *owner,
                        const HostFrameSnapshot &snapshot);
  void renderOneFrameImpl(CompositionRenderController *owner,
                          const HostFrameSnapshot &hostSnapshot,
                          bool forceRender);
  // P0-3d: render-only subpath used when an Interactive Render Region is
  // active. The caller picks a RenderQuality (Draft/Preview/Final) based
  // on interactiveRenderRegionResolutionScale_; the function applies the
  // IRR scissor and re-issues the composition draw calls. Implementation
  // is intentionally minimal in this milestone — the actual draw loop is
  // shared with renderOneFrameImpl via the existing composition path,
  // gated by the active scissor. Future P0-3d.1 will introduce a real
  // partial-render callback pipeline.
  void renderPartialRegion(CompositionRenderController *owner,
                           RenderQuality quality,
                           const RenderROI &roi);
  void captureIsolationLayerIds();
  void clearIsolationLayerIds();
  bool isolationHidesLayer(const LayerID &id) const;
  void refreshIsolationOverlayHud(CompositionRenderController *owner);

};

namespace CompositionRenderControllerDetail {

bool isDesignWorkspace(const QObject *controller) {
  if (!controller) {
    return false;
  }
  const QObject *workspaceOwner = controller->parent();
  return workspaceOwner &&
         workspaceOwner->property("artifactWorkspaceMode").toString() ==
             QStringLiteral("Design");
}

bool isAbsoluteLayoutLayer(const ArtifactAbstractLayerPtr &layer) {
  const auto property = layer
                            ? layer->getProperty(
                                  QStringLiteral("component.layout.mode"))
                            : nullptr;
  return property && property->getValue().toInt() == 2;
}

// The legacy 2D TransformGizmo is not drawn for non-text layers: the 3D
// manipulator and the camera-projected frame own their visible handles.  Its
// only remaining legitimate owner is the Design workspace's sibling-reorder
// gesture, which is driven from the Move handle.  Everywhere else the press
// must not be offered to it, otherwise a click on empty space inside the
// projected frame (or on the axis arrows) is claimed by a gizmo the user
// cannot see and is committed through a different transform/undo path.
bool legacy2DGizmoShouldOwnPress(const QObject *controller, GizmoOwner owner) {
  return owner == GizmoOwner::Transform2D ||
      (owner == GizmoOwner::Transform3D && isDesignWorkspace(controller));
}

QString trackerModeTitle(const ArtifactCore::MotionTracker *tracker) {
  return tracker && tracker->trackerType() == ArtifactCore::TrackerType::Planar
             ? QStringLiteral("Planar Tracker")
             : QStringLiteral("Point Tracker");
}

} // namespace CompositionRenderControllerDetail

} // namespace Artifact
