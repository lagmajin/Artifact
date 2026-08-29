module;
#include <algorithm>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QDebug>
#include <memory>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Service.Effect;

import Utils.String.UniString;
import Utils.Id;
import Memory.SharedPtr;
import Artifact.Effect.Abstract;
import Artifact.Effects.Manager;
import Artifact.Effect.Ofx.Host;
import Artifact.Effect.Ofx.Impl;
import Artifact.Project.PresetManager;
import Artifact.Service.Project;
import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;
import Audio.Modulation.Router;
import Core.Diagnostics.FallbackPolicy;
import BrightnessEffect;
import Artifact.Effect.Creative;
import Artifact.Effect.SurfaceFX;
import Artifact.Effect.DirectionalGlow;
import Artifact.Effect.Rasterizer.Sharpen;
import Artifact.Effect.Rasterizer.FindEdges;
import Artifact.Effect.Rasterizer.Edge;
import Artifact.Effect.Rasterizer.RadialBlur;
import Artifact.Effect.Rasterizer.AddNoise;
import Artifact.Effect.Rasterizer.RadialShadow;
import Artifact.Effect.Rasterizer.OpticsCompensation;
import Artifact.Effect.Rasterizer.Mosaic;
import Artifact.Effect.Rasterizer.TurbulentDisplace;
import Artifact.Effect.Rasterizer.Bevel;
import Artifact.Effect.Rasterizer.LinearWipe;
import Artifact.Effect.Glow;
import Artifact.Effect.Glow.EdgeBloom;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.ReactiveGlow;
import Artifact.Effect.Glow.LiquidGlow;
import Artifact.Effect.Glow.ResidualGlow;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Keying.LumaKey;
import Artifact.Effect.Keying.DifferenceKey;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.LensDistortion;
import Artifact.Effect.LiftGammaGain;
import ExposureEffect;
import GrayscaleEffect;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.Rasterizer.InnerShadow;
import Artifact.Effect.Rasterizer.Stroke;
import Artifact.Effect.Rasterizer.Satin;
import Artifact.Effect.Rasterizer.Echo;
import Artifact.Effect.Rasterizer.Ghost;
import Artifact.Effect.Rasterizer.Feedback;
import Artifact.Effect.Rasterizer.FrameAccumulation;
import Artifact.Effect.Rasterizer.FrameBlend;
import Artifact.Effect.Rasterizer.FrameAverage;
import Artifact.Effect.Rasterizer.FreezeFrame;
import Artifact.Effect.Rasterizer.TemporalDenoise;
import Artifact.Effect.Rasterizer.TemporalMedian;
import Artifact.Effect.Rasterizer.TemporalSmear;
import Artifact.Effect.Rasterizer.TimeBlur;
import Artifact.Effect.Rasterizer.TrailFade;
import Artifact.Effect.AutoMosaic;
import Artifact.Effect.CornerPin;
import Artifact.Effect.Rasterizer.ChromaticAberration;
import Artifact.Effect.Rasterizer.DataMosh;
import Artifact.Effect.Rasterizer.Deflicker;
import Artifact.Effect.Rasterizer.LightTrails;
import Artifact.Effect.Rasterizer.MotionTrail;
import Artifact.Effect.Rasterizer.OpticalFlowBlur;
import Artifact.Effect.Rasterizer.PixelSort;
import Artifact.Effect.Rasterizer.PosterizeTime;
import Artifact.Effect.Rasterizer.SlitScan;
import Artifact.Effect.Rasterizer.Strobe;
import Artifact.Effect.Rasterizer.TimeWarp;
import Artifact.Effect.Rasterizer.VectorBlur;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.Spherize;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Wave;
import HueAndSaturation;
import Artifact.Effect.WhiteBalance;
import TritoneEffect;
import ColoramaEffect;
import PhotoFilterEffect;
import GradientRampEffect;
import FillEffect;
import ColorBalanceEffect;
import LevelsEffect;
import ChannelMixerEffect;
import SelectiveColorEffect;
import ColorWheelsEffect;
import CurvesEffect;
import Artifact.Effect.Distort.DisplacementMap;
import Artifact.Effect.Distort.ImageMorph;
import Artifact.Effect.Distort.TimeDisplacement;
import Artifact.Effect.Generate.RadioWaves;
import Artifact.Effect.Liquify;
import Artifact.Effect.Kaleidoscope;
import Artifact.Effect.Dithering;
import Artifact.Effect.Kuwahara;
import Artifact.Effect.Rasterizer.AnisotropicFlowBlur;
import Artifact.Effect.Rasterizer.VectorFlowGlitch;
import Artifact.Effect.Glow.PhysicalHalation;
import Artifact.Effect.Rasterizer.ReactionDiffusionBlur;
import Artifact.Effect.Glow.LuminescenceCaustics;
import Artifact.Effect.Rasterizer.ApertureShapeBlur;
import Artifact.Effect.Rasterizer.ScreenShake;
import Artifact.Effect.Generate.SimpleRain;
import Artifact.Effect.Rasterizer.ChromaticRelief;
import Artifact.Effect.Rasterizer.FilmDamage;
import Artifact.Effect.Rasterizer.Vignette;
import Artifact.Effect.Rasterizer.Stripes;
import Artifact.Effect.Rasterizer.Voronoi;
import Artifact.Effect.Rasterizer.Bricks;
import Artifact.Effect.Rasterizer.HexGrid;
import InvertEffect;

namespace Artifact
{
 using namespace ArtifactCore;

namespace {
QString stripDuplicateSuffix(const QString &effectId) {
  QString base = effectId.trimmed();
  while (true) {
    if (base.endsWith(QStringLiteral("__copy"))) {
      base.chop(QStringLiteral("__copy").size());
      continue;
    }

    const int copyPos = base.lastIndexOf(QStringLiteral("__copy"));
    if (copyPos >= 0) {
      const QString copyTail = base.mid(copyPos + QStringLiteral("__copy").size());
      const bool hasNumericSuffix =
          !copyTail.isEmpty() &&
          std::all_of(copyTail.begin(), copyTail.end(), [](const QChar ch) {
            return ch.isDigit();
          });
      if (hasNumericSuffix) {
        base = base.left(copyPos);
        continue;
      }
    }

    const int dashPos = base.lastIndexOf(QLatin1Char('-'));
    if (dashPos >= 0) {
      const QString dashTail = base.mid(dashPos + 1);
      const bool hasNumericSuffix =
          !dashTail.isEmpty() &&
          std::all_of(dashTail.begin(), dashTail.end(), [](const QChar ch) {
            return ch.isDigit();
          });
      if (hasNumericSuffix) {
        base = base.left(dashPos);
        continue;
      }
    }

    break;
  }
  return base;
}
} // namespace

W_OBJECT_IMPL(ArtifactEffectService)

 class ArtifactEffectService::Impl
 {
 public:
  ArtifactGlobalEffectManager* effectManager_ = nullptr;

  Impl() {
   effectManager_ = ArtifactGlobalEffectManager::effectManager();
  }
  ~Impl() = default;
 };

 ArtifactEffectService::ArtifactEffectService(QObject* parent)
  : QObject(parent), impl_(new Impl())
 {
 }

 ArtifactEffectService::~ArtifactEffectService()
 {
  delete impl_;
 }

 ArtifactEffectService* ArtifactEffectService::instance()
 {
  static ArtifactEffectService svc;
  return &svc;
 }

 std::unique_ptr<ArtifactAbstractEffect> ArtifactEffectService::createEffect(const EffectID& id) const
 {
  const QString effectId = id.toString();
  if (effectId == QStringLiteral("surfacefx")) {
   return std::make_unique<SurfaceFXEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.colorwheels")) {
   return std::make_unique<ColorWheelsEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.curves")) {
   return std::make_unique<CurvesEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.brightness")) {
   auto effect = std::make_unique<BrightnessEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Brightness / Contrast"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.hsl")) {
   auto effect = std::make_unique<HueAndSaturation>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Hue / Saturation"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.exposure")) {
   auto effect = std::make_unique<ExposureEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Exposure"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.grayscale")) {
   auto effect = std::make_unique<GrayscaleEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Grayscale"));
   return effect;
  }
  if (effectId == QStringLiteral("brightness")) {
   auto effect = std::make_unique<BrightnessEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Brightness / Contrast"));
   return effect;
  }
  if (effectId == QStringLiteral("hue_saturation")) {
   auto effect = std::make_unique<HueAndSaturation>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Hue / Saturation"));
   return effect;
  }
  if (effectId == QStringLiteral("exposure")) {
   auto effect = std::make_unique<ExposureEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Exposure"));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.tint")) {
   auto effect = std::make_unique<WhiteBalanceEffect>();
   effect->setDisplayName(UniString("Tint"));
   effect->setEffectID(UniString::fromQString(effectId));
   return effect;
  }
  if (effectId == QStringLiteral("effect.colorcorrection.photofilter")) {
   return std::make_unique<PhotoFilterEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.gradientramp")) {
   return std::make_unique<GradientRampEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.fill")) {
   return std::make_unique<FillEffect>();
  }
  if (effectId == QStringLiteral("effect.layerstyle.coloroverlay")) {
   auto effect = std::make_unique<FillEffect>();
   effect->setDisplayName(UniString("Color Overlay"));
   effect->setEffectID(UniString::fromQString(effectId));
   return effect;
  }
  if (effectId == QStringLiteral("effect.layerstyle.gradientoverlay")) {
   auto effect = std::make_unique<GradientRampEffect>();
   effect->setDisplayName(UniString("Gradient Overlay"));
   effect->setEffectID(UniString::fromQString(effectId));
   return effect;
  }
  if (effectId == QStringLiteral("effect.layerstyle.patternoverlay")) {
   return std::make_unique<PatternOverlayEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.tritone")) {
   return std::make_unique<TritoneEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.colorama")) {
   return std::make_unique<ColoramaEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.colorbalance")) {
   return std::make_unique<ColorBalanceEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.levels")) {
   return std::make_unique<LevelsEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.channelmixer")) {
   return std::make_unique<ChannelMixerEffect>();
  }
  if (effectId == QStringLiteral("effect.colorcorrection.selectivecolor")) {
   return std::make_unique<SelectiveColorEffect>();
  }
  if (effectId == QStringLiteral("chroma_key")) {
   auto effect = std::make_unique<ChromaKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chroma Key"));
   return effect;
  }
  if (effectId == QStringLiteral("Effect.Keying.ChromaKey")) {
   auto effect = std::make_unique<ChromaKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chroma Key"));
   return effect;
  }
  if (effectId == QStringLiteral("luma_key") ||
      effectId == QStringLiteral("Effect.Keying.LumaKey")) {
   auto effect = std::make_unique<LumaKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Luma Key"));
   return effect;
  }
  if (effectId == QStringLiteral("difference_key") ||
      effectId == QStringLiteral("Effect.Keying.DifferenceKey")) {
    auto effect = std::make_unique<DifferenceKeyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Difference Key"));
    return effect;
  }
  if (effectId == QStringLiteral("difference_matte") ||
      effectId == QStringLiteral("Effect.Rasterizer.DifferenceMatte")) {
    auto effect = std::make_unique<DifferenceMatteEffect>();
    effect->setEffectID(UniString::fromQString(effectId));
    effect->setDisplayName(QStringLiteral("Difference Matte"));
    return effect;
  }
  if (effectId == QStringLiteral("ibk_keyer") ||
      effectId == QStringLiteral("Effect.Keying.IBKKeyer")) {
    auto effect = std::make_unique<IBKKeyerEffect>();
    effect->setEffectID(UniString::fromQString(effectId));
    effect->setDisplayName(QStringLiteral("IBK Keyer"));
    return effect;
  }
  if (effectId == QStringLiteral("effect.blur.gaussian")) {
   auto effect = std::make_unique<GaussianBlur>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Gaussian Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("drop_shadow")) {
   auto effect = std::make_unique<DropShadowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Drop Shadow"));
   return effect;
  }
  if (effectId == QStringLiteral("inner_shadow")) {
   auto effect = std::make_unique<InnerShadowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Inner Shadow"));
   return effect;
  }
  if (effectId == QStringLiteral("stroke")) {
   auto effect = std::make_unique<StrokeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Stroke"));
   return effect;
  }
  if (effectId == QStringLiteral("satin")) {
   auto effect = std::make_unique<SatinEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Satin"));
   return effect;
  }
  if (effectId == QStringLiteral("echo") ||
      effectId == QStringLiteral("Effect.Rasterizer.Echo")) {
   auto effect = std::make_unique<EchoEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Echo / Afterimage"));
   return effect;
  }
  if (effectId == QStringLiteral("ghost") ||
      effectId == QStringLiteral("Effect.Rasterizer.Ghost")) {
   auto effect = std::make_unique<GhostEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Ghost"));
   return effect;
  }
  if (effectId == QStringLiteral("feedback") ||
      effectId == QStringLiteral("Effect.Rasterizer.Feedback")) {
   auto effect = std::make_unique<FeedbackEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Feedback"));
   return effect;
  }
  if (effectId == QStringLiteral("frame_accumulation") ||
      effectId == QStringLiteral("Effect.Rasterizer.FrameAccumulation")) {
   auto effect = std::make_unique<FrameAccumulationEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Frame Accumulation"));
   return effect;
  }
  if (effectId == QStringLiteral("frame_blend") ||
      effectId == QStringLiteral("Effect.Rasterizer.FrameBlend")) {
   auto effect = std::make_unique<FrameBlendEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Frame Blend"));
   return effect;
  }
  if (effectId == QStringLiteral("frame_average") ||
      effectId == QStringLiteral("Effect.Rasterizer.FrameAverage")) {
   auto effect = std::make_unique<FrameAverageEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Frame Average"));
   return effect;
  }
  if (effectId == QStringLiteral("freeze_frame") ||
      effectId == QStringLiteral("Effect.Rasterizer.FreezeFrame")) {
   auto effect = std::make_unique<FreezeFrameEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Freeze Frame"));
   return effect;
  }
  if (effectId == QStringLiteral("temporal_denoise") ||
      effectId == QStringLiteral("Effect.Rasterizer.TemporalDenoise")) {
   auto effect = std::make_unique<TemporalDenoiseEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Temporal Denoise"));
   return effect;
  }
  if (effectId == QStringLiteral("temporal_median") ||
      effectId == QStringLiteral("Effect.Rasterizer.TemporalMedian")) {
   auto effect = std::make_unique<TemporalMedianEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Temporal Median"));
   return effect;
  }
  if (effectId == QStringLiteral("temporal_smear") ||
      effectId == QStringLiteral("Effect.Rasterizer.TemporalSmear")) {
   auto effect = std::make_unique<TemporalSmearEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Temporal Smear"));
   return effect;
  }
  if (effectId == QStringLiteral("time_blur") ||
      effectId == QStringLiteral("Effect.Rasterizer.TimeBlur")) {
   auto effect = std::make_unique<TimeBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Time Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("trail_fade") ||
      effectId == QStringLiteral("Effect.Rasterizer.TrailFade")) {
   auto effect = std::make_unique<TrailFadeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Trail Fade"));
   return effect;
  }
  if (effectId == QStringLiteral("chromatic_aberration")) {
   auto effect = std::make_unique<ChromaticAberrationEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chromatic Aberration"));
   return effect;
  }
  if (effectId == QStringLiteral("data_mosh")) {
   auto effect = std::make_unique<DataMoshEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Data Mosh"));
   return effect;
  }
  if (effectId == QStringLiteral("deflicker")) {
   auto effect = std::make_unique<DeflickerEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Deflicker"));
   return effect;
  }
  if (effectId == QStringLiteral("light_trails")) {
   auto effect = std::make_unique<LightTrailsEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Light Trails"));
   return effect;
  }
  if (effectId == QStringLiteral("motion_trail")) {
   auto effect = std::make_unique<MotionTrailEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Motion Trail"));
   return effect;
  }
  if (effectId == QStringLiteral("optical_flow_blur")) {
   auto effect = std::make_unique<OpticalFlowBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Optical Flow Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("pixel_sort")) {
   auto effect = std::make_unique<PixelSortEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Pixel Sort"));
   return effect;
  }
  if (effectId == QStringLiteral("posterize_time")) {
   auto effect = std::make_unique<PosterizeTimeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Posterize Time"));
   return effect;
  }
  if (effectId == QStringLiteral("slit_scan")) {
   auto effect = std::make_unique<SlitScanEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Slit Scan"));
   return effect;
  }
  if (effectId == QStringLiteral("strobe")) {
   auto effect = std::make_unique<StrobeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Strobe"));
   return effect;
  }
  if (effectId == QStringLiteral("time_warp")) {
   auto effect = std::make_unique<TimeWarpEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Time Warp"));
   return effect;
  }
  if (effectId == QStringLiteral("vector_blur")) {
   auto effect = std::make_unique<VectorBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Vector Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("glow")) {
   auto effect = std::make_unique<GlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("edge_bloom") ||
      effectId == QStringLiteral("effect.glow.edgebloom")) {
   auto effect = std::make_unique<EdgeBloomEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Edge Bloom"));
   return effect;
  }
  if (effectId == QStringLiteral("chromatic_glow") ||
      effectId == QStringLiteral("effect.glow.chromatic")) {
   auto effect = std::make_unique<ChromaticGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chromatic Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("reactive_glow") ||
      effectId == QStringLiteral("effect.glow.reactive")) {
   auto effect = std::make_unique<ReactiveGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Reactive Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("liquid_glow") ||
      effectId == QStringLiteral("effect.glow.liquid")) {
   auto effect = std::make_unique<LiquidGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Liquid Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("residual_glow") ||
      effectId == QStringLiteral("effect.glow.residual")) {
   auto effect = std::make_unique<ResidualGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Residual Glow"));
   return effect;
  }
  if (effectId == QStringLiteral("blur")) {
   auto effect = std::make_unique<BlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("wave")) {
   auto effect = std::make_unique<WaveEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Wave"));
   return effect;
  }
  if (effectId == QStringLiteral("spherize")) {
   auto effect = std::make_unique<SpherizeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Spherize"));
   return effect;
  }
  if (effectId == QStringLiteral("directional_glow")) {
   auto effect = std::make_unique<DirectionalGlowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Directional Glow / Streaks"));
   return effect;
  }
  if (effectId == QStringLiteral("lift_gamma_gain")) {
   auto effect = std::make_unique<LiftGammaGainEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Lift / Gamma / Gain"));
   return effect;
  }
  if (effectId == QStringLiteral("lens_distortion")) {
   auto effect = std::make_unique<LensDistortionEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Lens Distortion"));
   return effect;
  }
  if (effectId == QStringLiteral("twist")) {
   auto effect = std::make_unique<TwistTransform>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Twist (Geo Transform)"));
   return effect;
  }
  if (effectId == QStringLiteral("bend")) {
   auto effect = std::make_unique<BendTransform>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Bend (Geo Transform)"));
   return effect;
  }
  if (effectId == QStringLiteral("pbr_material")) {
   auto effect = std::make_unique<PBRMaterialEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("PBR Material"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.glitch") ||
      effectId == QStringLiteral("glitch")) {
   auto effect = std::make_unique<ArtifactGlitchEffect>();
   effect->setEffectID(UniString::fromQString(QStringLiteral("builtin.glitch")));
   effect->setDisplayName(QStringLiteral("Glitch"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.halftone")) {
   auto effect = std::make_unique<ArtifactHalftoneEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Halftone"));
   return effect;
  }
  if (effectId == QStringLiteral("halftone")) {
   auto effect = std::make_unique<ArtifactHalftoneEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Halftone"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.old_tv") ||
      effectId == QStringLiteral("old_tv")) {
   auto effect = std::make_unique<ArtifactOldTVEffect>();
   effect->setEffectID(UniString::fromQString(QStringLiteral("builtin.old_tv")));
   effect->setDisplayName(QStringLiteral("Old TV"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.pixel_sort_pro")) {
   return std::make_unique<PixelSortProEffect>();
  }
  if (effectId == QStringLiteral("builtin.optical_glow")) {
   return std::make_unique<OpticalGlowEffect>();
  }
  if (effectId == QStringLiteral("builtin.film_grunge")) {
   return std::make_unique<ArtifactFilmGrungeEffect>();
  }
  if (effectId == QStringLiteral("builtin.heatwave")) {
   return std::make_unique<ArtifactHeatwaveEffect>();
  }
  if (effectId == QStringLiteral("builtin.cinematic_lens_flare")) {
   return std::make_unique<ArtifactCinematicLensFlareEffect>();
  }
  if (effectId == QStringLiteral("builtin.volumetric_shine")) {
   return std::make_unique<VolumetricShineEffect>();
  }
  if (effectId == QStringLiteral("builtin.glint_star_filter")) {
   return std::make_unique<GlintStarFilterEffect>();
  }
  if (effectId == QStringLiteral("builtin.magic_sharp")) {
   return std::make_unique<MagicSharpEffect>();
  }
  if (effectId == QStringLiteral("builtin.depth_bokeh")) {
   return std::make_unique<DepthBokehEffect>();
  }
  if (effectId == QStringLiteral("builtin.texturize_motion")) {
   return std::make_unique<ArtifactTexturizeMotionEffect>();
  }
  if (effectId == QStringLiteral("builtin.deband")) {
   return std::make_unique<ArtifactDebandEffect>();
  }
  if (effectId == QStringLiteral("builtin.deblock")) {
   return std::make_unique<ArtifactDeblockEffect>();
  }
  if (effectId == QStringLiteral("builtin.beauty_studio")) {
   return std::make_unique<ArtifactBeautyStudioEffect>();
  }
  if (effectId == QStringLiteral("builtin.energy_zap")) {
   return std::make_unique<ArtifactEnergyZapEffect>();
  }
  if (effectId == QStringLiteral("builtin.light_wrap_pro")) {
   return std::make_unique<ArtifactLightWrapProEffect>();
  }
  if (effectId == QStringLiteral("builtin.match_grain")) {
   return std::make_unique<ArtifactMatchGrainEffect>();
  }
  if (effectId == QStringLiteral("builtin.wire_object_remover")) {
   return std::make_unique<ArtifactWireObjectRemoverEffect>();
  }
  if (effectId == QStringLiteral("builtin.depth_relight")) {
   return std::make_unique<ArtifactDepthRelightEffect>();
  }
  if (effectId == QStringLiteral("builtin.matte_refine")) {
   return std::make_unique<ArtifactMatteRefineEffect>();
  }
  if (effectId == QStringLiteral("builtin.spill_killer_pro")) {
   return std::make_unique<ArtifactSpillKillerProEffect>();
  }
  if (effectId == QStringLiteral("builtin.pixel_dust_fixer")) {
   return std::make_unique<ArtifactPixelDustFixerEffect>();
  }
  if (effectId == QStringLiteral("builtin.reflection_composer")) {
   return std::make_unique<ArtifactReflectionComposerEffect>();
  }
  if (effectId == QStringLiteral("builtin.lens_profile_matcher")) {
   return std::make_unique<ArtifactLensProfileMatcherEffect>();
  }
  if (effectId == QStringLiteral("builtin.atmospheric_depth")) {
   return std::make_unique<ArtifactAtmosphericDepthEffect>();
  }
  if (effectId == QStringLiteral("builtin.edge_color_composite")) {
   return std::make_unique<ArtifactEdgeColorCompositeEffect>();
  }
  if (effectId == QStringLiteral("builtin.edge_echo")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "EdgeEcho", "builtin.edge_echo", "Edge Echo");
  }
  if (effectId == QStringLiteral("builtin.pigment_separation")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "PigmentSeparation", "builtin.pigment_separation", "Pigment Separation");
  }
  if (effectId == QStringLiteral("builtin.light_pressure")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "LightPressure", "builtin.light_pressure", "Light Pressure");
  }
  if (effectId == QStringLiteral("builtin.surface_memory")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "SurfaceMemory", "builtin.surface_memory", "Surface Memory");
  }
  if (effectId == QStringLiteral("builtin.depth_melt")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "DepthMelt", "builtin.depth_melt", "Depth Melt");
  }
  if (effectId == QStringLiteral("sharpen")) {
   auto effect = std::make_unique<SharpenEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Sharpen"));
   return effect;
  }
  if (effectId == QStringLiteral("find_edges")) {
   auto effect = std::make_unique<FindEdgesEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Find Edges"));
   return effect;
  }
  if (effectId == QStringLiteral("radial_blur")) {
   auto effect = std::make_unique<RadialBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Radial Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("add_noise")) {
   auto effect = std::make_unique<AddNoiseEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Add Noise"));
   return effect;
  }
  if (effectId == QStringLiteral("radial_shadow")) {
   auto effect = std::make_unique<RadialShadowEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Radial Shadow"));
   return effect;
  }
  if (effectId == QStringLiteral("optics_compensation")) {
   auto effect = std::make_unique<OpticsCompensationEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Optics Compensation"));
   return effect;
  }
  if (effectId == QStringLiteral("mosaic")) {
   auto effect = std::make_unique<MosaicEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Mosaic"));
   return effect;
  }
  if (effectId == QStringLiteral("auto_mosaic") ||
      effectId == QStringLiteral("Effect.AutoMosaic")) {
   auto effect = std::make_unique<AutoMosaicEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Auto Mosaic"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.corner_pin") ||
      effectId == QStringLiteral("corner_pin")) {
   auto effect = std::make_unique<ArtifactCornerPinEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Corner Pin"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.pixelate") ||
      effectId == QStringLiteral("pixelate")) {
   auto effect = std::make_unique<MosaicEffect>();
   effect->setEffectID(UniString::fromQString(QStringLiteral("builtin.pixelate")));
   effect->setDisplayName(QStringLiteral("Pixelate"));
   return effect;
  }
  if (effectId == QStringLiteral("builtin.posterize") ||
      effectId == QStringLiteral("posterize")) {
   auto effect = std::make_unique<CurvesEffect>();
   effect->setPreset(5);
   effect->setEffectID(UniString::fromQString(QStringLiteral("builtin.posterize")));
   effect->setDisplayName(QStringLiteral("Posterize"));
   return effect;
  }
  if (effectId == QStringLiteral("turbulent_displace")) {
   auto effect = std::make_unique<TurbulentDisplaceEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Turbulent Displace"));
   return effect;
  }
  if (effectId == QStringLiteral("bevel")) {
   auto effect = std::make_unique<BevelEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Bevel"));
   return effect;
  }
  if (effectId == QStringLiteral("linear_wipe")) {
   auto effect = std::make_unique<LinearWipeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Linear Wipe"));
   return effect;
  }
  if (effectId == QStringLiteral("displacement_map") ||
      effectId == QStringLiteral("effect.distort.displacementmap")) {
   auto effect = std::make_unique<DisplacementMapEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Displacement Map"));
   return effect;
  }
  if (effectId == QStringLiteral("image_morph") ||
      effectId == QStringLiteral("effect.distort.imagemorph")) {
   auto effect = std::make_unique<ImageMorphEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Image Morph"));
   return effect;
  }
  if (effectId == QStringLiteral("screen_shake") ||
      effectId == QStringLiteral("effect.distort.screenshake")) {
   auto effect = std::make_unique<ScreenShakeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Screen Shake"));
   return effect;
  }
  if (effectId == QStringLiteral("rim_light") ||
      effectId == QStringLiteral("effect.stylize.rimlight")) {
   auto effect = std::make_unique<RimLightEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Rim Light / Edge Light"));
   return effect;
  }
  if (effectId == QStringLiteral("time_displacement") ||
      effectId == QStringLiteral("effect.distort.timedisplacement")) {
   auto effect = std::make_unique<TimeDisplacementEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Time Displacement"));
   return effect;
  }
  if (effectId == QStringLiteral("radio_waves") ||
      effectId == QStringLiteral("effect.generate.radiowaves")) {
   auto effect = std::make_unique<RadioWavesEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Radio Waves"));
   return effect;
  }
  if (effectId == QStringLiteral("liquify") ||
      effectId == QStringLiteral("effect.distort.liquify")) {
   auto effect = std::make_unique<LiquifyEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Liquify"));
   return effect;
  }
  if (effectId == QStringLiteral("kaleidoscope") ||
      effectId == QStringLiteral("effect.stylize.kaleidoscope")) {
   auto effect = std::make_unique<KaleidoscopeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Kaleidoscope"));
   return effect;
  }
  if (effectId == QStringLiteral("mirror") ||
      effectId == QStringLiteral("effect.stylize.mirror")) {
   return std::make_unique<ArtifactCoreCreativeEffect>(
       "Mirror", "mirror", "Mirror");
  }
  if (effectId == QStringLiteral("dithering") ||
      effectId == QStringLiteral("effect.stylize.dithering")) {
   auto effect = std::make_unique<DitheringEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Dithering"));
   return effect;
  }
  if (effectId == QStringLiteral("kuwahara") ||
      effectId == QStringLiteral("effect.stylize.kuwahara")) {
   auto effect = std::make_unique<KuwaharaEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Kuwahara"));
   return effect;
  }
  if (effectId == QStringLiteral("anisotropic_flow_blur") ||
      effectId == QStringLiteral("effect.blur.anisotropicflow")) {
   auto effect = std::make_unique<AnisotropicFlowBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Anisotropic Flow Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("vector_flow_glitch") ||
      effectId == QStringLiteral("effect.stylize.vectorflowglitch")) {
   auto effect = std::make_unique<VectorFlowGlitchEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Vector Flow Glitch"));
   return effect;
  }
  if (effectId == QStringLiteral("physical_halation") ||
      effectId == QStringLiteral("effect.glow.physicalhalation")) {
   auto effect = std::make_unique<PhysicalHalationEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Physical Halation"));
   return effect;
  }
  if (effectId == QStringLiteral("reaction_diffusion_blur") ||
      effectId == QStringLiteral("effect.blur.reactiondiffusion")) {
   auto effect = std::make_unique<ReactionDiffusionBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Reaction Diffusion Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("luminescence_caustics") ||
      effectId == QStringLiteral("effect.glow.luminescencecaustics")) {
   auto effect = std::make_unique<LuminescenceCausticsEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Luminescence Caustics"));
   return effect;
  }
  if (effectId == QStringLiteral("aperture_shape_blur") ||
      effectId == QStringLiteral("effect.blur.apertureshape")) {
   auto effect = std::make_unique<ApertureShapeBlurEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Aperture Shape Blur"));
   return effect;
  }
  if (effectId == QStringLiteral("simple_rain") ||
      effectId == QStringLiteral("effect.generate.simplerain")) {
   auto effect = std::make_unique<SimpleRainEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Simple Rain"));
   return effect;
  }
  if (effectId == QStringLiteral("chromatic_relief") ||
      effectId == QStringLiteral("effect.stylize.chromaticrelief")) {
   auto effect = std::make_unique<ChromaticReliefEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Chromatic Relief"));
   return effect;
  }
  if (effectId == QStringLiteral("film_damage") ||
      effectId == QStringLiteral("effect.stylize.filmdamage")) {
   auto effect = std::make_unique<FilmDamageEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Film Damage"));
   return effect;
  }
  if (effectId == QStringLiteral("vignette")) {
   auto effect = std::make_unique<VignetteEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Vignette"));
   return effect;
  }
  if (effectId == QStringLiteral("stripes")) {
   auto effect = std::make_unique<StripesEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Stripes"));
   return effect;
  }
  if (effectId == QStringLiteral("voronoi")) {
   auto effect = std::make_unique<VoronoiEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Voronoi"));
   return effect;
  }
  if (effectId == QStringLiteral("bricks")) {
   auto effect = std::make_unique<BricksEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Bricks"));
   return effect;
  }
  if (effectId == QStringLiteral("hex_grid")) {
   auto effect = std::make_unique<HexGridEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Hex Grid"));
   return effect;
  }
  if (effectId == QStringLiteral("edge")) {
   auto effect = std::make_unique<EdgeEffect>();
   effect->setEffectID(UniString::fromQString(effectId));
   effect->setDisplayName(QStringLiteral("Edge"));
   return effect;
  }
  if (effectId == QStringLiteral("invert") ||
      effectId == QStringLiteral("effect.colorcorrection.invert")) {
   auto effect = std::make_unique<InvertEffect>();
   effect->setEffectID(UniString::fromQString(QStringLiteral("effect.colorcorrection.invert")));
   effect->setDisplayName(QStringLiteral("Invert"));
   return effect;
  }
  if (effectId.startsWith(QStringLiteral("ofx."))) {
   const QString pluginId = effectId.mid(QStringLiteral("ofx.").size());
   Artifact::Ofx::ArtifactOfxHost::instance().initialize();
   for (const auto& plugin : Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins()) {
    if (plugin.identifier.toQString().compare(pluginId, Qt::CaseInsensitive) != 0) {
     continue;
    }
    return Artifact::Ofx::makeOfxEffect(plugin);
   }

   auto effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(effectId);
   effect->setDisplayName(QStringLiteral("OFX: %1").arg(pluginId));
   FallbackTracker::instance()->record(
       FallbackCategory::Effect,
       FallbackAction::Fallback,
       effectId,
       QStringLiteral("generic-ofx"),
       QStringLiteral("OFX plugin is not loaded; preserving a generic effect envelope"));
   return effect;
  }

  if (!impl_->effectManager_) {
   FallbackTracker::instance()->record(
       FallbackCategory::Effect,
       FallbackAction::Bypass,
       effectId,
       QStringLiteral("none"),
       QStringLiteral("Effect manager is unavailable; effect creation skipped"));
   return nullptr;
  }
  // Delegate to the global effect manager's factory
  if (auto pluginEffect = impl_->effectManager_->factoryByID(id)) {
    return pluginEffect;
  }

  // The legacy global manager does not expose a concrete factory result yet.
  // Keep the requested plugin effect in the layer/effect graph instead of
  // dropping it: the generic effect preserves its ID and property envelope,
  // and can be replaced by a plugin implementation when one is available.
  auto fallback = std::make_unique<ArtifactAbstractEffect>();
  fallback->setEffectID(UniString::fromQString(effectId));
  fallback->setDisplayName(QStringLiteral("Plugin Effect: %1").arg(effectId));
  FallbackTracker::instance()->record(
      FallbackCategory::Effect,
      FallbackAction::Fallback,
      effectId,
      QStringLiteral("generic-effect"),
      QStringLiteral("Effect factory is unavailable; preserving a generic effect envelope"));
  return fallback;
 }

 std::vector<EffectInfo> ArtifactEffectService::availableEffects() const
 {
  // Return known effect types
 std::vector<EffectInfo> effects;
  effects.push_back({EffectID("effect.colorcorrection.brightness"), "Brightness / Contrast"});
  effects.push_back({EffectID("effect.colorcorrection.hsl"), "Hue / Saturation"});
  effects.push_back({EffectID("effect.colorcorrection.exposure"), "Exposure"});
  effects.push_back({EffectID("effect.colorcorrection.grayscale"), "Grayscale"});
  effects.push_back({EffectID("effect.colorcorrection.colorwheels"), "Color Wheels"});
  effects.push_back({EffectID("effect.colorcorrection.curves"), "Curves"});
  effects.push_back({EffectID("effect.colorcorrection.tint"), "Tint"});
  effects.push_back({EffectID("effect.colorcorrection.photofilter"), "Photo Filter"});
  effects.push_back({EffectID("effect.colorcorrection.gradientramp"), "Gradient Ramp"});
  effects.push_back({EffectID("effect.colorcorrection.fill"), "Fill"});
  effects.push_back({EffectID("effect.layerstyle.coloroverlay"), "Color Overlay"});
  effects.push_back({EffectID("effect.layerstyle.gradientoverlay"), "Gradient Overlay"});
  effects.push_back({EffectID("effect.layerstyle.patternoverlay"), "Pattern Overlay"});
  effects.push_back({EffectID("effect.colorcorrection.tritone"), "Tritone"});
  effects.push_back({EffectID("effect.colorcorrection.colorama"), "Colorama"});
  effects.push_back({EffectID("effect.colorcorrection.colorbalance"), "Color Balance"});
  effects.push_back({EffectID("effect.colorcorrection.levels"), "Levels"});
  effects.push_back({EffectID("effect.colorcorrection.channelmixer"), "Channel Mixer"});
  effects.push_back({EffectID("effect.colorcorrection.selectivecolor"), "Selective Color"});
  effects.push_back({EffectID("chroma_key"), "Chroma Key"});
  effects.push_back({EffectID("luma_key"), "Luma Key"});
  effects.push_back({EffectID("difference_key"), "Difference Key"});
  effects.push_back({EffectID("difference_matte"), "Difference Matte"});
  effects.push_back({EffectID("drop_shadow"), "Drop Shadow"});
  effects.push_back({EffectID("inner_shadow"), "Inner Shadow"});
  effects.push_back({EffectID("stroke"), "Stroke"});
  effects.push_back({EffectID("satin"), "Satin"});
  effects.push_back({EffectID("echo"), "Echo / Afterimage"});
  effects.push_back({EffectID("ghost"), "Ghost"});
  effects.push_back({EffectID("feedback"), "Feedback"});
  effects.push_back({EffectID("frame_accumulation"), "Frame Accumulation"});
  effects.push_back({EffectID("frame_blend"), "Frame Blend"});
  effects.push_back({EffectID("frame_average"), "Frame Average"});
  effects.push_back({EffectID("freeze_frame"), "Freeze Frame"});
  effects.push_back({EffectID("temporal_denoise"), "Temporal Denoise"});
  effects.push_back({EffectID("temporal_median"), "Temporal Median"});
  effects.push_back({EffectID("temporal_smear"), "Temporal Smear"});
  effects.push_back({EffectID("time_blur"), "Time Blur"});
  effects.push_back({EffectID("trail_fade"), "Trail Fade"});
  effects.push_back({EffectID("chromatic_aberration"), "Chromatic Aberration"});
  effects.push_back({EffectID("data_mosh"), "Data Mosh"});
  effects.push_back({EffectID("deflicker"), "Deflicker"});
  effects.push_back({EffectID("light_trails"), "Light Trails"});
  effects.push_back({EffectID("motion_trail"), "Motion Trail"});
  effects.push_back({EffectID("optical_flow_blur"), "Optical Flow Blur"});
  effects.push_back({EffectID("pixel_sort"), "Pixel Sort"});
  effects.push_back({EffectID("builtin.pixel_sort_pro"), "Pixel Sort Pro"});
  effects.push_back({EffectID("posterize_time"), "Posterize Time"});
  effects.push_back({EffectID("slit_scan"), "Slit Scan"});
  effects.push_back({EffectID("strobe"), "Strobe"});
  effects.push_back({EffectID("time_warp"), "Time Warp"});
  effects.push_back({EffectID("vector_blur"), "Vector Blur"});
  effects.push_back({EffectID("directional_glow"), "Directional Glow / Streaks"});
  effects.push_back({EffectID("glow"), "Glow"});
  effects.push_back({EffectID("builtin.optical_glow"), "Optical Glow"});
  effects.push_back({EffectID("builtin.film_grunge"), "Film Grunge"});
  effects.push_back({EffectID("builtin.heatwave"), "Heatwave"});
  effects.push_back({EffectID("builtin.cinematic_lens_flare"), "Cinematic Lens Flare"});
  effects.push_back({EffectID("builtin.volumetric_shine"), "Volumetric Shine"});
  effects.push_back({EffectID("builtin.glint_star_filter"), "Glint / Star Filter"});
  effects.push_back({EffectID("builtin.magic_sharp"), "Magic Sharp"});
  effects.push_back({EffectID("builtin.depth_bokeh"), "Depth Bokeh / Rack Defocus"});
  effects.push_back({EffectID("builtin.texturize_motion"), "Texturize Motion"});
  effects.push_back({EffectID("builtin.deband"), "Deband"});
  effects.push_back({EffectID("builtin.deblock"), "Deblock"});
  effects.push_back({EffectID("builtin.beauty_studio"), "Beauty Studio"});
  effects.push_back({EffectID("builtin.energy_zap"), "Energy Zap / Lightning"});
  effects.push_back({EffectID("builtin.light_wrap_pro"), "Light Wrap Pro"});
  effects.push_back({EffectID("builtin.match_grain"), "Match Grain"});
  effects.push_back({EffectID("builtin.wire_object_remover"), "Wire / Object Remover"});
  effects.push_back({EffectID("builtin.depth_relight"), "Depth Relight"});
  effects.push_back({EffectID("builtin.matte_refine"), "Matte Refine"});
  effects.push_back({EffectID("builtin.spill_killer_pro"), "Spill Killer Pro"});
  effects.push_back({EffectID("builtin.pixel_dust_fixer"), "Pixel / Dust Fixer"});
  effects.push_back({EffectID("builtin.reflection_composer"), "Reflection Composer"});
  effects.push_back({EffectID("builtin.lens_profile_matcher"), "Lens Profile Matcher"});
  effects.push_back({EffectID("builtin.atmospheric_depth"), "Atmospheric Depth"});
  effects.push_back({EffectID("builtin.edge_color_composite"), "Edge Color Composite"});
  effects.push_back({EffectID("edge_bloom"), "Edge Bloom"});
  effects.push_back({EffectID("chromatic_glow"), "Chromatic Glow"});
  effects.push_back({EffectID("reactive_glow"), "Reactive Glow"});
  effects.push_back({EffectID("liquid_glow"), "Liquid Glow"});
  effects.push_back({EffectID("residual_glow"), "Residual Glow"});
  effects.push_back({EffectID("effect.blur.gaussian"), "Gaussian Blur"});
  effects.push_back({EffectID("blur"), "Blur"});
  effects.push_back({EffectID("lift_gamma_gain"), "Lift / Gamma / Gain"});
  effects.push_back({EffectID("lens_distortion"), "Lens Distortion"});
  effects.push_back({EffectID("wave"), "Wave"});
  effects.push_back({EffectID("spherize"), "Spherize"});
  effects.push_back({EffectID("twist"), "Twist"});
  effects.push_back({EffectID("bend"), "Bend"});
  effects.push_back({EffectID("pbr_material"), "PBR Material"});
  effects.push_back({EffectID("builtin.glitch"), "Glitch"});
  effects.push_back({EffectID("halftone"), "Halftone"});
  effects.push_back({EffectID("builtin.old_tv"), "Old TV"});
  effects.push_back({EffectID("builtin.edge_echo"), "Edge Echo"});
  effects.push_back({EffectID("builtin.pigment_separation"), "Pigment Separation"});
  effects.push_back({EffectID("builtin.light_pressure"), "Light Pressure"});
  effects.push_back({EffectID("builtin.surface_memory"), "Surface Memory"});
  effects.push_back({EffectID("builtin.depth_melt"), "Depth Melt"});
  effects.push_back({EffectID("sharpen"), "Sharpen"});
  effects.push_back({EffectID("find_edges"), "Find Edges"});
  effects.push_back({EffectID("rim_light"), "Rim Light / Edge Light"});
  effects.push_back({EffectID("radial_blur"), "Radial Blur"});
  effects.push_back({EffectID("add_noise"), "Add Noise"});
  effects.push_back({EffectID("radial_shadow"), "Radial Shadow"});
  effects.push_back({EffectID("optics_compensation"), "Optics Compensation"});
  effects.push_back({EffectID("mosaic"), "Mosaic"});
  effects.push_back({EffectID("auto_mosaic"), "Auto Mosaic"});
  effects.push_back({EffectID("builtin.corner_pin"), "Corner Pin"});
  effects.push_back({EffectID("builtin.pixelate"), "Pixelate"});
  effects.push_back({EffectID("builtin.posterize"), "Posterize"});
  effects.push_back({EffectID("turbulent_displace"), "Turbulent Displace"});
  effects.push_back({EffectID("bevel"), "Bevel"});
  effects.push_back({EffectID("linear_wipe"), "Linear Wipe"});
  effects.push_back({EffectID("displacement_map"), "Displacement Map"});
  effects.push_back({EffectID("image_morph"), "Image Morph"});
  effects.push_back({EffectID("screen_shake"), "Screen Shake"});
  effects.push_back({EffectID("time_displacement"), "Time Displacement"});
  effects.push_back({EffectID("liquify"), "Liquify"});
  effects.push_back({EffectID("radio_waves"), "Radio Waves"});
  effects.push_back({EffectID("kaleidoscope"), "Kaleidoscope"});
  effects.push_back({EffectID("mirror"), "Mirror"});
  effects.push_back({EffectID("dithering"), "Dithering"});
  effects.push_back({EffectID("kuwahara"), "Kuwahara"});
  effects.push_back({EffectID("anisotropic_flow_blur"), "Anisotropic Flow Blur"});
  effects.push_back({EffectID("vector_flow_glitch"), "Vector Flow Glitch"});
  effects.push_back({EffectID("physical_halation"), "Physical Halation"});
  effects.push_back({EffectID("reaction_diffusion_blur"), "Reaction Diffusion Blur"});
  effects.push_back({EffectID("luminescence_caustics"), "Luminescence Caustics"});
  effects.push_back({EffectID("aperture_shape_blur"), "Aperture Shape Blur"});
  effects.push_back({EffectID("simple_rain"), "Simple Rain"});
  effects.push_back({EffectID("chromatic_relief"), "Chromatic Relief"});
  effects.push_back({EffectID("film_damage"), "Film Damage"});
  effects.push_back({EffectID("vignette"), "Vignette"});
  effects.push_back({EffectID("stripes"), "Stripes"});
  effects.push_back({EffectID("voronoi"), "Voronoi"});
  effects.push_back({EffectID("bricks"), "Bricks"});
  effects.push_back({EffectID("hex_grid"), "Hex Grid"});
  effects.push_back({EffectID("edge"), "Edge"});
  effects.push_back({EffectID("effect.colorcorrection.invert"), "Invert"});

  Artifact::Ofx::ArtifactOfxHost::instance().initialize();
  for (const auto& plugin : Artifact::Ofx::ArtifactOfxHost::instance().getLoadedPlugins()) {
   const QString pluginId = plugin.identifier.toQString().trimmed();
   if (pluginId.isEmpty()) {
    continue;
   }

   effects.push_back({
       EffectID(QStringLiteral("ofx.%1").arg(pluginId)),
       QStringLiteral("OFX: %1").arg(pluginId),
   });
  }
  return effects;
 }

 QStringList ArtifactEffectService::availableEffectNames() const
 {
  QStringList names;
  for (const auto& info : availableEffects()) {
   names.append(info.displayName);
  }
  return names;
 }

 EffectServiceResult ArtifactEffectService::addEffectToLayer(const LayerID& layerId, const EffectID& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  // Create the effect through the global manager
  auto effect = createEffect(effectId);
  if (!effect) {
   effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(effectId.toString());
   effect->setDisplayName(effectId.toString());
  }

  auto effectPtr = makeShared(effect.release(), [](ArtifactAbstractEffect* p) { delete p; });
  // Layer effect racks currently display and evaluate Rasterizer effects.
  // Callers such as the Effect menu use this service directly, bypassing the
  // Inspector's normalization; without this, insertion succeeds but the
  // effect immediately disappears from the layer Effects surface.
  effectPtr->setPipelineStage(EffectPipelineStage::Rasterizer);
  if (ps->addEffectToLayerWithUndo(layerId, effectPtr)) {
   const QString actualEffectId = effectPtr ? effectPtr->effectID().toQString() : effectId.toString();
   Q_EMIT effectAdded(layerId, actualEffectId);
   return EffectServiceResult::ok(actualEffectId);
  }
  return EffectServiceResult::fail("Failed to add effect");
 }

 EffectServiceResult ArtifactEffectService::removeEffectFromLayer(const LayerID& layerId, const QString& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  SharedPtr<ArtifactAbstractEffect> capturedEffect;
  if (auto comp = ps->currentComposition().lock()) {
   if (auto layer = comp->layerById(layerId)) {
    for (const auto& e : layer->getEffects()) {
     if (e && e->effectID().toQString() == effectId) {
      capturedEffect = e;
      break;
     }
    }
   }
  }
  if (ps->removeEffectFromLayerWithUndo(layerId, effectId, capturedEffect)) {
   Q_EMIT effectRemoved(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to remove effect");
 }

 EffectServiceResult ArtifactEffectService::setEffectEnabled(const LayerID& layerId, const QString& effectId, bool enabled)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  bool wasEnabled = false;
  if (auto comp = ps->currentComposition().lock()) {
   if (auto layer = comp->layerById(layerId)) {
    for (const auto& e : layer->getEffects()) {
     if (e && e->effectID().toQString() == effectId) {
      wasEnabled = e->isEnabled();
      break;
     }
    }
   }
  }
  if (ps->setEffectEnabledWithUndo(layerId, effectId, enabled, wasEnabled)) {
   Q_EMIT effectChanged(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to set effect enabled state");
 }

 EffectServiceResult ArtifactEffectService::moveEffect(const LayerID& layerId, const QString& effectId, int direction)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->moveEffectWithUndo(layerId, effectId, direction)) {
   Q_EMIT effectOrderChanged(layerId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to move effect");
 }

 EffectServiceResult ArtifactEffectService::duplicateEffect(const LayerID& layerId, const QString& effectId)
 {
  // Find the existing effect and clone its properties
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto comp = ps->currentComposition().lock();
  if (!comp || layerId.isNil()) {
   return EffectServiceResult::fail("Composition not available");
  }
  auto layer = comp->layerById(layerId);
  if (!layer) {
   return EffectServiceResult::fail("Layer not available");
  }

  QString baseId = effectId.trimmed();
  if (baseId.isEmpty()) {
   baseId = QStringLiteral("effect");
  }
  QString sourceDisplayName = effectId;
  bool sourceEnabled = true;
  SharedPtr<ArtifactAbstractEffect> sourceEffect;
  for (const auto &effect : layer->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) {
    continue;
   }
   sourceEffect = effect;
   const QString displayName = effect->displayName().toQString().trimmed();
   if (!displayName.isEmpty()) {
    sourceDisplayName = displayName;
   }
   sourceEnabled = effect->isEnabled();
   break;
  }
  QString sourceTypeId =
      sourceEffect ? stripDuplicateSuffix(sourceEffect->effectID().toQString())
                   : baseId;
  if (sourceTypeId.isEmpty()) {
    sourceTypeId = baseId;
  }
  std::unique_ptr<ArtifactAbstractEffect> effectCopy =
      createEffect(EffectID(sourceTypeId));
  if (!effectCopy) {
    effectCopy = std::make_unique<ArtifactAbstractEffect>();
  }
  const QString copyBaseId = sourceTypeId.isEmpty() ? baseId : sourceTypeId;
  QString copyId = copyBaseId + QStringLiteral("__copy");
  int copyIndex = 2;
  const auto existingEffects = layer->getEffects();
  const auto idExists = [&existingEffects](const QString& candidate) {
   return std::any_of(existingEffects.begin(), existingEffects.end(),
    [&candidate](const SharedPtr<ArtifactAbstractEffect>& effect) {
     return effect && effect->effectID().toQString() == candidate;
    });
  };
  while (idExists(copyId)) {
   copyId = QStringLiteral("%1__copy%2").arg(copyBaseId).arg(copyIndex++);
  }
  effectCopy->setEffectID(copyId);
  effectCopy->setDisplayName(QStringLiteral("%1 (copy)").arg(sourceDisplayName));
  effectCopy->setEnabled(sourceEnabled);
  if (sourceEffect) {
   effectCopy->setMix(sourceEffect->mix());
   effectCopy->setAllowOverscan(sourceEffect->allowOverscan());
   effectCopy->setPipelineStage(sourceEffect->pipelineStage());
   effectCopy->setComputeMode(sourceEffect->computeMode());
  }
  if (sourceEffect) {
   for (const auto &property : sourceEffect->getProperties()) {
    effectCopy->setPropertyValue(
        UniString(property.getName().toStdString()), property.getValue());
   }
  }

  auto copyPtr = makeShared(effectCopy.release(), [](ArtifactAbstractEffect* p) { delete p; });
  if (ps->addEffectToLayerInCurrentComposition(layerId, copyPtr)) {
   const QString newEffectId = copyPtr ? copyPtr->effectID().toQString() : copyId;
   Q_EMIT effectAdded(layerId, newEffectId);
   return EffectServiceResult::ok(newEffectId);
  }
  return EffectServiceResult::fail("Failed to duplicate effect");
 }

 EffectServiceResult ArtifactEffectService::addEffectToCurrentComposition(const EffectID& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto effect = createEffect(effectId);
  if (!effect) {
   effect = std::make_unique<ArtifactAbstractEffect>();
   effect->setEffectID(effectId.toString());
   effect->setDisplayName(effectId.toString());
  }

  auto effectPtr = makeShared(effect.release(), [](ArtifactAbstractEffect* p) { delete p; });
  if (ps->addEffectToCurrentComposition(effectPtr)) {
   const QString actualEffectId = effectPtr ? effectPtr->effectID().toQString() : effectId.toString();
   return EffectServiceResult::ok(actualEffectId);
  }
  return EffectServiceResult::fail("Failed to add composition effect");
 }

 EffectServiceResult ArtifactEffectService::removeEffectFromCurrentComposition(const QString& effectId)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->removeEffectFromCurrentComposition(effectId)) {
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to remove composition effect");
 }

 EffectServiceResult ArtifactEffectService::setCompositionEffectEnabled(const QString& effectId, bool enabled)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->setEffectEnabledInCurrentComposition(effectId, enabled)) {
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to set composition effect enabled state");
 }

 EffectServiceResult ArtifactEffectService::moveCompositionEffect(const QString& effectId, int direction)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  if (ps->moveEffectInCurrentComposition(effectId, direction)) {
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Failed to move composition effect");
 }

 EffectServiceResult ArtifactEffectService::setEffectProperty(const LayerID& layerId,
  const QString& effectId, const QString& propertyName, const QVariant& value)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto comp = ps->currentComposition().lock();
  if (!comp || layerId.isNil()) {
   return EffectServiceResult::fail("Composition not available");
  }
  auto layer = comp->layerById(layerId);
  if (!layer) {
   return EffectServiceResult::fail("Layer not available");
  }
  const QString normalizedPropertyName = propertyName.trimmed();
  if (normalizedPropertyName.isEmpty()) {
   return EffectServiceResult::fail("Property not found");
  }

  for (const auto &effect : layer->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) {
    continue;
   }
   const auto properties = effect->editableProperties();
   const auto propertyExists = std::any_of(
       properties.begin(), properties.end(),
       [&normalizedPropertyName](const auto &property) {
         return property && property->getName().compare(normalizedPropertyName, Qt::CaseInsensitive) == 0;
       });
   if (!propertyExists) {
    return EffectServiceResult::fail("Property not found");
   }
    if (!effect->setCommonPropertyValue(normalizedPropertyName, value)) {
      effect->setPropertyValue(UniString::fromQString(normalizedPropertyName), value);
    }
    ArtifactCore::globalEventBus().post<LayerChangedEvent>(LayerChangedEvent{
        comp->id().toString(), layerId.toString(),
        LayerChangedEvent::ChangeType::Modified});
    if (auto project = ps->getCurrentProjectSharedPtr()) {
      ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
      project->projectChanged();
    }
    Q_EMIT effectChanged(layerId, effectId);
    return EffectServiceResult::ok(effectId);
   }

  return EffectServiceResult::fail("Effect not found");
 }

 EffectServiceResult ArtifactEffectService::setCompositionEffectProperty(const QString& effectId,
  const QString& propertyName, const QVariant& value)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");

  auto comp = ps->currentComposition().lock();
  if (!comp) {
   return EffectServiceResult::fail("Composition not available");
  }
  const QString normalizedPropertyName = propertyName.trimmed();
  if (effectId.trimmed().isEmpty() || normalizedPropertyName.isEmpty()) {
   return EffectServiceResult::fail("Property not found");
  }

  for (const auto &effect : comp->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) {
    continue;
   }
   const auto properties = effect->editableProperties();
   const auto propertyExists = std::any_of(
       properties.begin(), properties.end(),
       [&normalizedPropertyName](const auto &property) {
         return property && property->getName().compare(normalizedPropertyName, Qt::CaseInsensitive) == 0;
       });
   if (!propertyExists) {
    return EffectServiceResult::fail("Property not found");
   }
   if (!effect->setCommonPropertyValue(normalizedPropertyName, value)) {
     effect->setPropertyValue(UniString::fromQString(normalizedPropertyName), value);
   }
   comp->changed();
   if (auto project = ps->getCurrentProjectSharedPtr()) {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
    project->projectChanged();
   }
   return EffectServiceResult::ok(effectId);
  }

  return EffectServiceResult::fail("Effect not found");
 }

 EffectServiceResult ArtifactEffectService::setEffectModulationSnapshot(
  const LayerID& layerId, const QString& effectId,
  const Audio::Modulation::ModulationRouterSnapshot& snapshot)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");
  auto comp = ps->currentComposition().lock();
  if (!comp || layerId.isNil()) return EffectServiceResult::fail("Composition not available");
  auto layer = comp->layerById(layerId);
  if (!layer) return EffectServiceResult::fail("Layer not available");
  for (const auto& effect : layer->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) continue;
   const auto before = effect->modulationRouter().snapshot();
   if (auto* undo = UndoManager::instance()) {
    undo->push(std::make_unique<EffectModulationSnapshotCommand>(
        effect, before, snapshot));
   } else {
    effect->modulationRouter().restoreSnapshot(snapshot);
   }
   ArtifactCore::globalEventBus().post<LayerChangedEvent>(LayerChangedEvent{
       comp->id().toString(), layerId.toString(),
       LayerChangedEvent::ChangeType::Modified});
   if (auto project = ps->getCurrentProjectSharedPtr()) {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
    project->projectChanged();
   }
   Q_EMIT effectChanged(layerId, effectId);
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Effect not found");
 }

 EffectServiceResult ArtifactEffectService::setCompositionEffectModulationSnapshot(
  const QString& effectId,
  const Audio::Modulation::ModulationRouterSnapshot& snapshot)
 {
  auto* ps = ArtifactProjectService::instance();
  if (!ps) return EffectServiceResult::fail("Project service not available");
  auto comp = ps->currentComposition().lock();
  if (!comp || effectId.trimmed().isEmpty()) {
   return EffectServiceResult::fail("Composition not available");
  }
  for (const auto& effect : comp->getEffects()) {
   if (!effect || effect->effectID().toQString() != effectId) continue;
   const auto before = effect->modulationRouter().snapshot();
   if (auto* undo = UndoManager::instance()) {
    undo->push(std::make_unique<EffectModulationSnapshotCommand>(
        effect, before, snapshot, QStringLiteral("Edit Composition Effect Modulation")));
   } else {
    effect->modulationRouter().restoreSnapshot(snapshot);
   }
   comp->changed();
   if (auto project = ps->getCurrentProjectSharedPtr()) {
    ArtifactCore::globalEventBus().publish<ProjectChangedEvent>({QString(), QString()});
    project->projectChanged();
   }
   return EffectServiceResult::ok(effectId);
  }
  return EffectServiceResult::fail("Effect not found");
 }

 bool ArtifactEffectService::saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath) const
 {
  return ArtifactPresetManager::saveEffectPreset(effect, filePath);
 }

 bool ArtifactEffectService::loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) const
 {
  return ArtifactPresetManager::loadEffectPreset(effect, filePath);
 }

}; // namespace Artifact
