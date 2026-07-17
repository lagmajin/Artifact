module;
#include <QAbstractButton>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QFocusEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QModelIndex>
#include <QMouseEvent>
#include <QFileInfo>
#include <QObject>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QKeyEvent>
#include <QRadioButton>
#include <QShortcut>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>
#include <QUrl>
#include <QWidget>
#include <cstdlib>
#include <wobjectimpl.h>

#ifdef READ
#undef READ
#endif
#ifdef WRITE
#undef WRITE
#endif
#ifdef APPEND
#undef APPEND
#endif

#include <opencv2/opencv.hpp>


#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

module Widgets.Inspector;

import Utils.Id;
import Utils.String.UniString;
import Widgets.Utils.CSS;
import Artifact.Widgets.Inspector.EffectTabSurface;
import Artifact.Widgets.Inspector.ComponentTabSurface;

import Artifact.Service.Project;
import Artifact.Service.Effect;
import Artifact.Project.PresetManager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Component.System;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Artifact.Mask.LayerMask;
import Image.ImageF32x4_RGBA;
import Artifact.Widgets.ObjectPicker;
import Artifact.Layer.Matte;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Switch;
import Artifact.Event.Types;
import Event.Bus;
import Undo.UndoManager;
import Input.Operator;
import Generator.Effector;
import Artifact.Effect.Generator.FractalNoise;
import Artifact.Effect.Generator.ProceduralTexture;
import Artifact.Effect.Transform.Twist;
import Artifact.Effect.Transform.Bend;
import Artifact.Effect.Render.PBRMaterial;
import Artifact.Effect.LayerTransform.Transform2D;
import Artifact.Effect.Rasterizer.Blur;
import Artifact.Effect.Rasterizer.DropShadow;
import Artifact.Effect.DirectionalGlow;
import BrightnessEffect;
import ExposureEffect;
import HueAndSaturation;
import ColorWheelsEffect;
import CurvesEffect;
import Artifact.Effect.WhiteBalance;
import Artifact.Effect.Distort.DisplacementMap;
import Artifact.Effect.Distort.TimeDisplacement;
import PhotoFilterEffect;
import GradientRampEffect;
import FillEffect;
import TritoneEffect;
import ColoramaEffect;
import ColorBalanceEffect;
import LevelsEffect;
import ChannelMixerEffect;
import SelectiveColorEffect;
import Artifact.Effect.Glow;
import Artifact.Effect.Glow.EdgeBloom;
import Artifact.Effect.Glow.ChromaticGlow;
import Artifact.Effect.Glow.ReactiveGlow;
import Artifact.Effect.Glow.LiquidGlow;
import Artifact.Effect.Glow.ResidualGlow;
import Artifact.Effect.GauusianBlur;
import Artifact.Effect.LiftGammaGain;
import Artifact.Effect.LensDistortion;
import Artifact.Effect.Keying.ChromaKey;
import Artifact.Effect.Wave;
import Artifact.Effect.Spherize;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.AppDialogs;

namespace Artifact {

using namespace ArtifactCore;

// using namespace ArtifactWidgets;

namespace {
constexpr int kEffectRackCount = 5;
constexpr int kInspectorSectionMarginL = 8;
constexpr int kInspectorSectionMarginT = 8;
constexpr int kInspectorSectionMarginR = 8;
constexpr int kInspectorSectionMarginB = 8;
constexpr int kInspectorSectionSpacing = 4;
constexpr int kInspectorNoteMargin = 6;
constexpr int kInspectorRackMarginL = 6;
constexpr int kInspectorRackMarginT = 10;
constexpr int kInspectorRackMarginR = 6;
constexpr int kInspectorRackMarginB = 6;
constexpr auto kInspectorContext = "Panel.Inspector";

QColor themeColor(const QString &value, const QColor &fallback) {
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

struct LayerTabComponentState {
  bool hasLayer = false;
  bool canEditComponents = false;
  bool physicsEnabled = false;
  bool scriptEnabled = false;
  bool layoutEnabled = false;
  bool cloneEnabled = false;
  bool collisionEnabled = false;
  bool crowdEnabled = false;
  bool particleEmitterEnabled = false;
  bool fluidEnabled = false;
  int generatorCount = 0;
  int fieldCount = 0;
  int cloneModifierCount = 0;
  int extraCloneModifierCount = 0;
  std::vector<LayerComponentValidationIssue> validationIssues;
};

bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath);

LayerTabComponentState collectLayerTabComponentState(
    const ArtifactAbstractLayerPtr &layer) {
  LayerTabComponentState state;
  state.hasLayer = static_cast<bool>(layer);
  state.canEditComponents = state.hasLayer;
  state.physicsEnabled =
      state.hasLayer && layerBooleanProperty(layer, QStringLiteral("physics.enabled"));
  state.scriptEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.script.enabled"));
  state.layoutEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.layout.enabled"));
  state.cloneEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.cloner.enabled"));
  state.collisionEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.collision.enabled"));
  state.crowdEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.crowd.enabled"));
  state.particleEmitterEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.particleEmitter.enabled"));
  state.fluidEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.fluid.enabled"));
  state.generatorCount =
      state.hasLayer ? static_cast<int>(layer->layerGenerators().size()) : 0;
  state.fieldCount = state.hasLayer ? static_cast<int>(layer->layerFields().size()) : 0;
  state.cloneModifierCount =
      state.hasLayer ? static_cast<int>(layer->layerCloneModifiers().size()) : 0;
  state.extraCloneModifierCount = std::max(0, state.cloneModifierCount - 2);
  state.validationIssues =
      state.hasLayer ? layer->validateLayerComponents()
                     : std::vector<LayerComponentValidationIssue>{};
  return state;
}

QString layerComponentSummaryText(const LayerTabComponentState &state) {
  if (!state.hasLayer) {
    return QStringLiteral("Components: unavailable until a layer is selected");
  }

  QStringList active;
  if (state.physicsEnabled) {
    active.push_back(QStringLiteral("Physics"));
  }
  if (state.scriptEnabled) {
    active.push_back(QStringLiteral("Script"));
  }
  if (state.layoutEnabled) {
    active.push_back(QStringLiteral("Layout"));
  }
  if (state.cloneEnabled) {
    active.push_back(QStringLiteral("Cloner"));
  }
  if (state.collisionEnabled) {
    active.push_back(QStringLiteral("Collision"));
  }
  if (state.crowdEnabled) {
    active.push_back(QStringLiteral("Crowd"));
  }
  if (state.particleEmitterEnabled) {
    active.push_back(QStringLiteral("Particle Emitter"));
  }
  if (state.fluidEnabled) {
    active.push_back(QStringLiteral("Fluid"));
  }
  return active.isEmpty() ? QStringLiteral("Components: none")
                          : QStringLiteral("Components: %1")
                                .arg(active.join(QStringLiteral(", ")));
}

QColor blendColor(const QColor &a, const QColor &b, const qreal t) {
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                          a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                          a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                          a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath);

struct EffectCatalogEntry {
  EffectPipelineStage stage = EffectPipelineStage::Rasterizer;
  QString effectId;
  QString displayName;
  QString category;
  QString description;
  QString keywords;
};

QString stageDisplayName(EffectPipelineStage stage) {
  switch (stage) {
  case EffectPipelineStage::Generator:
    return QStringLiteral("Generator");
  case EffectPipelineStage::GeometryTransform:
    return QStringLiteral("Geo Transform");
  case EffectPipelineStage::MaterialRender:
    return QStringLiteral("Material");
  case EffectPipelineStage::Rasterizer:
    return QStringLiteral("Rasterizer");
  case EffectPipelineStage::LayerTransform:
    return QStringLiteral("Layer Transform");
  }
  return QStringLiteral("Effects");
}

std::vector<EffectCatalogEntry> buildEffectCatalogEntries() {
  return {
      {EffectPipelineStage::Generator, QStringLiteral("fractal_noise"),
       QStringLiteral("Fractal Noise"), QStringLiteral("Generator"),
       QStringLiteral("Procedural texture and noise source."), QStringLiteral("noise texture procedural")},
      {EffectPipelineStage::Generator,
       QStringLiteral("procedural_texture"),
       QStringLiteral("Procedural Texture"), QStringLiteral("Generator"),
       QStringLiteral("Pattern-based texture generator."),
       QStringLiteral("texture procedural pattern")},
      {EffectPipelineStage::GeometryTransform, QStringLiteral("twist"),
       QStringLiteral("Twist"), QStringLiteral("Geometry"),
       QStringLiteral("Twist geometry around an axis."),
       QStringLiteral("deform rotate geometry")},
      {EffectPipelineStage::GeometryTransform, QStringLiteral("bend"),
       QStringLiteral("Bend"), QStringLiteral("Geometry"),
       QStringLiteral("Bend geometry across a domain."),
       QStringLiteral("deform curve geometry")},
      {EffectPipelineStage::MaterialRender, QStringLiteral("pbr_material"),
       QStringLiteral("PBR Material"), QStringLiteral("Material"),
       QStringLiteral("Assign physically-based material controls."),
       QStringLiteral("material metal roughness shader")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("blur"),
       QStringLiteral("Blur"), QStringLiteral("Blur"),
       QStringLiteral("Simple raster blur."), QStringLiteral("soften blur")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.blur.gaussian"),
       QStringLiteral("Gaussian Blur"), QStringLiteral("Blur"),
       QStringLiteral("Classic gaussian image blur."),
       QStringLiteral("soft blur gaussian")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("anisotropic_flow_blur"),
       QStringLiteral("Anisotropic Flow Blur"), QStringLiteral("Blur"),
       QStringLiteral("Edge-preserving blur that follows local image flow."),
       QStringLiteral("flow directional edge anisotropic blur")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("reaction_diffusion_blur"),
       QStringLiteral("Reaction Diffusion Blur"), QStringLiteral("Blur"),
       QStringLiteral("Organic blur with self-organizing cellular patterns."),
       QStringLiteral("reaction diffusion organic cellular blur melt")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("aperture_shape_blur"),
       QStringLiteral("Aperture Shape Blur"), QStringLiteral("Blur"),
       QStringLiteral("FFT lens blur using built-in or custom PSF shapes."),
       QStringLiteral("aperture bokeh lens psf fft heart polygon blur")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("glow"),
       QStringLiteral("Glow"), QStringLiteral("Glow"),
       QStringLiteral("Bloom around bright areas."),
       QStringLiteral("bloom light glow")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("drop_shadow"),
       QStringLiteral("Drop Shadow"), QStringLiteral("Light"),
       QStringLiteral("Shadow cast behind the layer."),
       QStringLiteral("shadow depth offset")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("directional_glow"),
       QStringLiteral("Directional Glow / Streaks"),
       QStringLiteral("Glow"),
       QStringLiteral("Stretch glow into directional streaks."),
       QStringLiteral("streaks anamorphic directional glow")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("edge_bloom"),
       QStringLiteral("Edge Bloom"), QStringLiteral("Glow"),
       QStringLiteral("Bloom driven by edge contrast."),
       QStringLiteral("edges bloom")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("chromatic_glow"),
       QStringLiteral("Chromatic Glow"), QStringLiteral("Glow"),
       QStringLiteral("Color-split glow bloom."),
       QStringLiteral("rgb chromatic aberration glow")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("reactive_glow"),
       QStringLiteral("Reactive Glow"), QStringLiteral("Glow"),
       QStringLiteral("Animated glow response with temporal flavor."),
       QStringLiteral("reactive animated glow")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("liquid_glow"),
       QStringLiteral("Liquid Glow"), QStringLiteral("Glow"),
       QStringLiteral("Fluid-style glow diffusion."),
       QStringLiteral("liquid glow fluid")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("residual_glow"),
       QStringLiteral("Residual Glow"), QStringLiteral("Glow"),
       QStringLiteral("Afterimage-like lingering bloom."),
       QStringLiteral("residual afterimage glow")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("physical_halation"),
       QStringLiteral("Physical Halation"), QStringLiteral("Film"),
       QStringLiteral("Red-shifted highlight scattering inspired by film emulsion."),
       QStringLiteral("film halation red bloom highlight")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("luminescence_caustics"),
       QStringLiteral("Luminescence Caustics"), QStringLiteral("Glow"),
       QStringLiteral("Animated light-ridge patterns driven by highlights and edges."),
       QStringLiteral("caustics light crystal water magic glow")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("simple_rain"),
       QStringLiteral("Simple Rain"), QStringLiteral("Generate"),
       QStringLiteral("Screen-space rain streaks with wind, depth, and splashes."),
       QStringLiteral("rain weather streak wind splash generate")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("vector_flow_glitch"),
       QStringLiteral("Vector Flow Glitch"), QStringLiteral("Stylize"),
       QStringLiteral("Tear pixels along local contours with chromatic splitting."),
       QStringLiteral("glitch vector flow edge tear chromatic")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("chromatic_relief"),
       QStringLiteral("Chromatic Relief"), QStringLiteral("Stylize"),
       QStringLiteral("Embossed luminance relief with directional RGB separation."),
       QStringLiteral("chromatic relief emboss edge rgb split stylize")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("film_damage"),
       QStringLiteral("Film Damage"), QStringLiteral("Film"),
       QStringLiteral("Grain, dust, scratches, gate weave, flicker, and film burn."),
       QStringLiteral("film damage grain dust scratch burn flicker vintage")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.brightness"),
       QStringLiteral("Brightness / Contrast"),
       QStringLiteral("Color"), QStringLiteral("Basic tonal correction."),
       QStringLiteral("brightness contrast tone")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.exposure"),
       QStringLiteral("Exposure"), QStringLiteral("Color"),
       QStringLiteral("Exposure and intensity correction."),
       QStringLiteral("exposure tone")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.tint"),
       QStringLiteral("Tint"), QStringLiteral("Color"),
       QStringLiteral("Shift temperature and tint."),
       QStringLiteral("tint white balance temperature")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.photofilter"),
       QStringLiteral("Photo Filter"), QStringLiteral("Color"),
       QStringLiteral("Camera-style photo filter wash."),
       QStringLiteral("photo filter color")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.gradientramp"),
       QStringLiteral("Gradient Ramp"), QStringLiteral("Color"),
       QStringLiteral("Map tones through a gradient."),
       QStringLiteral("gradient ramp remap")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.fill"),
       QStringLiteral("Fill"), QStringLiteral("Color"),
       QStringLiteral("Solid recolor fill."),
       QStringLiteral("fill solid recolor")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.hsl"),
       QStringLiteral("Hue / Saturation"), QStringLiteral("Color"),
       QStringLiteral("Hue and saturation correction."),
       QStringLiteral("hue saturation color")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.colorwheels"),
       QStringLiteral("Color Wheels"), QStringLiteral("Color"),
       QStringLiteral("Lift/gamma/gain style wheel controls."),
       QStringLiteral("color wheels grade")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.curves"),
       QStringLiteral("Curves"), QStringLiteral("Color"),
       QStringLiteral("Curve-based tonal shaping."),
       QStringLiteral("curves rgb luma")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.tritone"),
       QStringLiteral("Tritone"), QStringLiteral("Color"),
       QStringLiteral("Three-color remap treatment."),
       QStringLiteral("tritone duotone color")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.colorama"),
       QStringLiteral("Colorama"), QStringLiteral("Color"),
       QStringLiteral("Stylized palette remapping."),
       QStringLiteral("palette psychedelic colorama")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.colorbalance"),
       QStringLiteral("Color Balance"), QStringLiteral("Color"),
       QStringLiteral("Balance color channels."),
       QStringLiteral("balance channel color")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.levels"),
       QStringLiteral("Levels"), QStringLiteral("Color"),
       QStringLiteral("Input/output black and white levels."),
       QStringLiteral("levels histogram")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.channelmixer"),
       QStringLiteral("Channel Mixer"), QStringLiteral("Color"),
       QStringLiteral("Mix and rebalance channels."),
       QStringLiteral("channel mixer rgb")},
      {EffectPipelineStage::Rasterizer,
       QStringLiteral("effect.colorcorrection.selectivecolor"),
       QStringLiteral("Selective Color"), QStringLiteral("Color"),
       QStringLiteral("Selective hue-based correction."),
       QStringLiteral("selective color cmyk")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("lift_gamma_gain"),
       QStringLiteral("Lift / Gamma / Gain"), QStringLiteral("Color"),
       QStringLiteral("Three-way color grading controls."),
       QStringLiteral("lift gamma gain grade")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("lens_distortion"),
       QStringLiteral("Lens Distortion"), QStringLiteral("Distort"),
       QStringLiteral("Warp image using lens characteristics."),
       QStringLiteral("distortion lens optics")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("displacement_map"),
       QStringLiteral("Displacement Map"), QStringLiteral("Distort"),
       QStringLiteral("Warp using displacement textures."),
       QStringLiteral("displacement map distort")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("screen_shake"),
       QStringLiteral("Screen Shake"), QStringLiteral("Transform"),
       QStringLiteral("Animated 2D screen shake / offset."),
       QStringLiteral("shake camera offset jitter")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("time_displacement"),
       QStringLiteral("Time Displacement"), QStringLiteral("Time"),
       QStringLiteral("Offset sampling in time."),
       QStringLiteral("time temporal displacement")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("chroma_key"),
       QStringLiteral("Chroma Key"), QStringLiteral("Keying"),
       QStringLiteral("Key out a color range."),
       QStringLiteral("green screen keying")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("wave"),
       QStringLiteral("Wave"), QStringLiteral("Distort"),
       QStringLiteral("Wave deformation effect."),
       QStringLiteral("wave distort ripple")},
      {EffectPipelineStage::Rasterizer, QStringLiteral("spherize"),
       QStringLiteral("Spherize"), QStringLiteral("Distort"),
       QStringLiteral("Project image across a sphere."),
       QStringLiteral("spherize fisheye bulge")},
      {EffectPipelineStage::LayerTransform,
       QStringLiteral("transform_2d"),
       QStringLiteral("Transform 2D"), QStringLiteral("Layer Transform"),
       QStringLiteral("Per-layer transform effect stack control."),
       QStringLiteral("transform position scale rotation layer")},
  };
}

bool effectCatalogEntryMatches(const EffectCatalogEntry &entry,
                               const QString &query) {
  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    return true;
  }
  return entry.displayName.contains(trimmed, Qt::CaseInsensitive) ||
         entry.category.contains(trimmed, Qt::CaseInsensitive) ||
         entry.description.contains(trimmed, Qt::CaseInsensitive) ||
         entry.keywords.contains(trimmed, Qt::CaseInsensitive) ||
         entry.effectId.contains(trimmed, Qt::CaseInsensitive);
}

void applyInspectorPalette(QWidget *widget, const bool elevated = false) {
  if (!widget) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  const QColor mutedText = blendColor(text, background, 0.52);
  const QColor disabledSurface = blendColor(surface, background, 0.58);

  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setAutoFillBackground(true);
  QPalette pal = widget->palette();
  const QColor window =
      elevated ? blendColor(surface, background, 0.16) : background;
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, surface);
  pal.setColor(QPalette::AlternateBase, blendColor(surface, background, 0.12));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, surface);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Light, accent.lighter(120));
  pal.setColor(QPalette::Disabled, QPalette::Window, background);
  pal.setColor(QPalette::Disabled, QPalette::WindowText, mutedText);
  pal.setColor(QPalette::Disabled, QPalette::Base, disabledSurface);
  pal.setColor(QPalette::Disabled, QPalette::AlternateBase, disabledSurface);
  pal.setColor(QPalette::Disabled, QPalette::Text, mutedText);
  pal.setColor(QPalette::Disabled, QPalette::Button, disabledSurface);
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, mutedText);
  pal.setColor(QPalette::Disabled, QPalette::Highlight, border);
  pal.setColor(QPalette::Disabled, QPalette::HighlightedText, mutedText);
  widget->setPalette(pal);
}

void applyInspectorLabelPalette(QLabel *label, const bool prominent = false) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  const QColor heading = blendColor(text, QColor(Qt::white), 0.12);
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? heading : text);
  label->setPalette(pal);
}

void applyInspectorSectionBox(QGroupBox *box) {
  if (!box) {
    return;
  }
  applyInspectorPalette(box, true);
  QFont font = box->font();
  font.setPointSize(10);
  font.setWeight(QFont::DemiBold);
  box->setFont(font);
}

void applyInspectorTextEdit(QPlainTextEdit *edit) {
  if (!edit) {
    return;
  }
  applyInspectorPalette(edit, true);
  edit->setTabChangesFocus(true);
}

void applyInspectorList(QListWidget *list) {
  if (!list) {
    return;
  }
  applyInspectorPalette(list, true);
  list->setAlternatingRowColors(true);
}

void applyInspectorButton(QPushButton *button, const bool accent = false) {
  if (!button) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor fill =
      accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")))
             : surface;
  const QColor contrast = accent ? QColor(Qt::white) : text;
  const QColor disabledText = blendColor(text, background, 0.58);
  const QColor disabledButton = blendColor(surface, background, 0.62);
  const QColor disabledWindow = background;

  button->setAttribute(Qt::WA_StyledBackground, true);
  button->setAutoFillBackground(true);
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, fill);
  pal.setColor(QPalette::ButtonText, contrast);
  pal.setColor(QPalette::Window, surface);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Disabled, QPalette::Button, disabledButton);
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
  pal.setColor(QPalette::Disabled, QPalette::Window, disabledWindow);
  pal.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
  pal.setColor(QPalette::Disabled, QPalette::Text, disabledText);
  pal.setColor(QPalette::Disabled, QPalette::Mid, border.darker(120));
  button->setPalette(pal);
}

void applyInspectorComponentStateButton(QPushButton *button,
                                        const bool active) {
  if (!button) {
    return;
  }
  applyInspectorButton(button, false);
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, active ? selection : surface);
  pal.setColor(QPalette::ButtonText, active ? QColor(Qt::white) : text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  button->setPalette(pal);
}

enum class RasterizerInitialSettingsMode {
  KeepDefaults,
  FitToSource,
  AskWhenAdding,
};

constexpr auto kRasterizerInitialSettingsKey =
    "Effects/RasterizerInitialSettingsMode";

RasterizerInitialSettingsMode rasterizerInitialSettingsModeFromValue(
    const QString &value) {
  if (value == QStringLiteral("fit")) {
    return RasterizerInitialSettingsMode::FitToSource;
  }
  if (value == QStringLiteral("defaults")) {
    return RasterizerInitialSettingsMode::KeepDefaults;
  }
  return RasterizerInitialSettingsMode::AskWhenAdding;
}

QString rasterizerInitialSettingsModeValue(
    const RasterizerInitialSettingsMode mode) {
  switch (mode) {
  case RasterizerInitialSettingsMode::KeepDefaults:
    return QStringLiteral("defaults");
  case RasterizerInitialSettingsMode::FitToSource:
    return QStringLiteral("fit");
  case RasterizerInitialSettingsMode::AskWhenAdding:
    return QStringLiteral("ask");
  }
  return QStringLiteral("ask");
}

RasterizerInitialSettingsMode rasterizerInitialSettingsModeFromSettings() {
  QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
  return rasterizerInitialSettingsModeFromValue(
      settings.value(QString::fromLatin1(kRasterizerInitialSettingsKey),
                     QStringLiteral("ask"))
          .toString());
}

class RasterizerInitialSettingsActionButton final : public QPushButton {
public:
  RasterizerInitialSettingsActionButton(const QString &text, QDialog *dialog,
                                        const int dialogResult)
      : QPushButton(text, dialog), dialog_(dialog), dialogResult_(dialogResult) {}

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate = event && event->button() == Qt::LeftButton &&
                          rect().contains(event->position().toPoint());
    QPushButton::mouseReleaseEvent(event);
    if (activate && dialog_) {
      dialog_->done(dialogResult_);
    }
  }

private:
  QDialog *dialog_ = nullptr;
  int dialogResult_ = QDialog::Rejected;
};

class RasterizerInitialSettingsDialog final : public QDialog {
public:
  RasterizerInitialSettingsDialog(const QSize &sourceSize, QWidget *parent)
      : QDialog(parent) {
    setWindowTitle(QStringLiteral("Rasterizer Effect Initial Settings"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    auto *description = new QLabel(
        QStringLiteral("Choose how to initialize pixel-based settings for this effect. "
                       "Auto Fit uses the source size (%1 x %2) only once; it will not "
                       "change the effect after it is added.")
            .arg(sourceSize.width())
            .arg(sourceSize.height()),
        this);
    description->setWordWrap(true);
    applyInspectorLabelPalette(description, false);
    layout->addWidget(description);

    defaultsButton_ = new QRadioButton(QStringLiteral("Keep effect defaults"), this);
    fitButton_ = new QRadioButton(QStringLiteral("Auto Fit to source size"), this);
    defaultsButton_->setChecked(true);
    layout->addWidget(defaultsButton_);
    layout->addWidget(fitButton_);

    rememberChoice_ = new QCheckBox(
        QStringLiteral("Use this choice automatically for future rasterizer effects"),
        this);
    layout->addWidget(rememberChoice_);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancelButton = new RasterizerInitialSettingsActionButton(
        QStringLiteral("Cancel"), this, QDialog::Rejected);
    auto *addButton = new RasterizerInitialSettingsActionButton(
        QStringLiteral("Add Effect"), this, QDialog::Accepted);
    applyInspectorButton(cancelButton, false);
    applyInspectorButton(addButton, true);
    buttons->addWidget(cancelButton);
    buttons->addWidget(addButton);
    layout->addLayout(buttons);
  }

  RasterizerInitialSettingsMode selectedMode() const {
    return fitButton_ && fitButton_->isChecked()
               ? RasterizerInitialSettingsMode::FitToSource
               : RasterizerInitialSettingsMode::KeepDefaults;
  }

  bool rememberChoice() const {
    return rememberChoice_ && rememberChoice_->isChecked();
  }

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (event && (event->key() == Qt::Key_Return ||
                  event->key() == Qt::Key_Enter)) {
      done(QDialog::Accepted);
      return;
    }
    QDialog::keyPressEvent(event);
  }

private:
  QRadioButton *defaultsButton_ = nullptr;
  QRadioButton *fitButton_ = nullptr;
  QCheckBox *rememberChoice_ = nullptr;
};

bool isSourceScaledRasterizerProperty(const QString &name) {
  const QString key = name.toLower();
  return key.contains(QStringLiteral("radius")) ||
         key.contains(QStringLiteral("blur")) ||
         key.contains(QStringLiteral("distance")) ||
         key.contains(QStringLiteral("offset")) ||
         key.contains(QStringLiteral("shift")) ||
         key.contains(QStringLiteral("width")) ||
         key.contains(QStringLiteral("height")) ||
         key.contains(QStringLiteral("size")) ||
         key.contains(QStringLiteral("thickness"));
}

int applyRasterizerSourceFit(ArtifactAbstractEffect *effect,
                             const QSize &sourceSize) {
  if (!effect || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
    return 0;
  }
  const double scale = std::clamp(
      std::sqrt((static_cast<double>(sourceSize.width()) * sourceSize.height()) /
                (1920.0 * 1080.0)),
      0.25, 4.0);
  int adjustedCount = 0;
  for (const auto &property : effect->getProperties()) {
    const QString name = property.getName();
    const QVariant value = property.getValue();
    if (!isSourceScaledRasterizerProperty(name) ||
        (property.getType() != PropertyType::Float &&
         property.getType() != PropertyType::Integer) ||
        !value.isValid()) {
      continue;
    }
    const double scaled = std::clamp(
        value.toDouble() * scale, property.getMinValue().isValid()
                                      ? property.getMinValue().toDouble()
                                      : -std::numeric_limits<double>::max(),
        property.getMaxValue().isValid()
            ? property.getMaxValue().toDouble()
            : std::numeric_limits<double>::max());
    effect->setPropertyValue(UniString::fromQString(name),
                             property.getType() == PropertyType::Integer
                                 ? QVariant(qRound(scaled))
                                 : QVariant(scaled));
    ++adjustedCount;
  }
  return adjustedCount;
}

class EffectPickerDialog final : public QDialog {
public:
  EffectPickerDialog(const std::vector<EffectCatalogEntry> &entries,
                     const EffectPipelineStage stageFilter,
                     const QString &targetLabel, QWidget *parent = nullptr)
      : QDialog(parent), entries_(entries), stageFilter_(stageFilter) {
    setWindowTitle(QStringLiteral("Add Effect"));
    setModal(true);
    resize(760, 540);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *header = new QLabel(
        QStringLiteral("Add to %1  |  Stage: %2")
            .arg(targetLabel, stageDisplayName(stageFilter_)),
        this);
    applyInspectorLabelPalette(header, true);
    layout->addWidget(header);

    auto *subHeader = new QLabel(
        QStringLiteral("Search by name, category, or keyword. Double click or press Add to insert and focus the effect."),
        this);
    subHeader->setWordWrap(true);
    applyInspectorLabelPalette(subHeader, false);
    layout->addWidget(subHeader);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("inspectorSearchEdit"));
    searchEdit_->setPlaceholderText(
        QStringLiteral("Search effects for this stage"));
    applyInspectorPalette(searchEdit_, true);
    layout->addWidget(searchEdit_);

    auto *contentFrame = new QFrame(this);
    contentFrame->setObjectName(QStringLiteral("inspectorContentFrame"));
    applyInspectorPalette(contentFrame, true);
    auto *contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(8);

    resultSummaryLabel_ = new QLabel(contentFrame);
    resultSummaryLabel_->setWordWrap(true);
    applyInspectorLabelPalette(resultSummaryLabel_, false);
    contentLayout->addWidget(resultSummaryLabel_);

    listWidget_ = new QListWidget(contentFrame);
    listWidget_->setUniformItemSizes(false);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    applyInspectorList(listWidget_);
    contentLayout->addWidget(listWidget_, 1);

    layout->addWidget(contentFrame, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         Qt::Horizontal, this);
    addButton_ = buttons->button(QDialogButtonBox::Ok);
    if (addButton_) {
      addButton_->setText(QStringLiteral("Add Effect"));
      applyInspectorButton(addButton_, true);
    }
    if (auto *cancelButton = buttons->button(QDialogButtonBox::Cancel)) {
      cancelButton->setText(QStringLiteral("Cancel"));
      applyInspectorButton(cancelButton, false);
    }
    layout->addWidget(buttons);

    QObject::connect(searchEdit_, &QLineEdit::textChanged, this,
                     [this](const QString &) { rebuildList(); });
    QObject::connect(listWidget_, &QListWidget::currentItemChanged, this,
                     [this](QListWidgetItem *, QListWidgetItem *) {
                       syncButtonState();
                     });
    QObject::connect(listWidget_, &QListWidget::itemDoubleClicked, this,
                     [this](QListWidgetItem *item) {
                       if (!item || item->data(Qt::UserRole).toString().trimmed().isEmpty()) {
                         return;
                       }
                       accept();
                     });
    QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                     &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                     &QDialog::reject);

    rebuildList();
  }

  QString selectedEffectId() const {
    if (!listWidget_ || !listWidget_->currentItem()) {
      return {};
    }
    return listWidget_->currentItem()->data(Qt::UserRole).toString().trimmed();
  }

  QString selectedDisplayName() const {
    if (!listWidget_ || !listWidget_->currentItem()) {
      return {};
    }
    return listWidget_->currentItem()
        ->data(Qt::UserRole + 1)
        .toString()
        .trimmed();
  }

private:
  void rebuildList() {
    if (!listWidget_) {
      return;
    }

    const QString query = searchEdit_ ? searchEdit_->text() : QString();
    const QSignalBlocker blocker(listWidget_);
    listWidget_->clear();

    int matchCount = 0;
    for (const auto &entry : entries_) {
      if (entry.stage != stageFilter_ || !effectCatalogEntryMatches(entry, query)) {
        continue;
      }
      ++matchCount;
      auto *item = new QListWidgetItem(
          QStringLiteral("%1  |  %2").arg(entry.displayName, entry.category),
          listWidget_);
      item->setData(Qt::UserRole, entry.effectId);
      item->setData(Qt::UserRole + 1, entry.displayName);
      item->setToolTip(entry.description);
    }

    if (matchCount == 0) {
      auto *item =
          new QListWidgetItem(QStringLiteral("No effects match this search."),
                              listWidget_);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    } else {
      listWidget_->setCurrentRow(0);
    }

    if (resultSummaryLabel_) {
      resultSummaryLabel_->setText(
          matchCount > 0
              ? QStringLiteral("%1 effect(s) available in %2.")
                    .arg(matchCount)
                    .arg(stageDisplayName(stageFilter_))
              : QStringLiteral("No matching effects in %1.")
                    .arg(stageDisplayName(stageFilter_)));
    }
    syncButtonState();
  }

  void syncButtonState() {
    if (!addButton_) {
      return;
    }
    const bool hasSelection = !selectedEffectId().isEmpty();
    addButton_->setEnabled(hasSelection);
  }

  std::vector<EffectCatalogEntry> entries_;
  EffectPipelineStage stageFilter_;
  QLineEdit *searchEdit_ = nullptr;
  QListWidget *listWidget_ = nullptr;
  QLabel *resultSummaryLabel_ = nullptr;
  QPushButton *addButton_ = nullptr;
};

QColor toneColor(LayerPresentationBadgeTone tone, const QColor &base,
                 const QColor &accent) {
  switch (tone) {
  case LayerPresentationBadgeTone::Container:
    return blendColor(base, accent, 0.18);
  case LayerPresentationBadgeTone::Media:
    return blendColor(base, accent, 0.10);
  case LayerPresentationBadgeTone::Motion:
    return blendColor(base, accent.lighter(108), 0.16);
  case LayerPresentationBadgeTone::Special:
    return blendColor(base, accent.darker(108), 0.14);
  case LayerPresentationBadgeTone::Neutral:
  default:
    return base;
  }
}

int rackIndexFromStage(EffectPipelineStage stage) {
  const int stageIndex = static_cast<int>(stage);
  if (stageIndex <= static_cast<int>(EffectPipelineStage::PreProcess)) {
    return -1;
  }
  const int rackIndex = stageIndex - 1;
  return (rackIndex >= 0 && rackIndex < kEffectRackCount) ? rackIndex : -1;
}

EffectPipelineStage stageFromRackIndex(int rackIndex) {
  return static_cast<EffectPipelineStage>(rackIndex + 1);
}

LayerPresentationBadgeTone toneFromRackIndex(int rackIndex) {
  switch (stageFromRackIndex(rackIndex)) {
  case EffectPipelineStage::Generator:
  case EffectPipelineStage::GeometryTransform:
    return LayerPresentationBadgeTone::Motion;
  case EffectPipelineStage::MaterialRender:
    return LayerPresentationBadgeTone::Media;
  case EffectPipelineStage::Rasterizer:
    return LayerPresentationBadgeTone::Special;
  case EffectPipelineStage::LayerTransform:
    return LayerPresentationBadgeTone::Container;
  default:
    return LayerPresentationBadgeTone::Neutral;
  }
}

constexpr int kEffectRackEnabledRole = Qt::UserRole + 1;
constexpr int kEffectRackHasMaskRole = Qt::UserRole + 2;
constexpr int kEffectRackNameRole = Qt::UserRole + 3;
constexpr int kEffectRackMaskCountRole = Qt::UserRole + 4;

class EffectRackItemDelegate final : public QStyledItemDelegate {
public:
  explicit EffectRackItemDelegate(int rackIndex, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), rackIndex_(rackIndex) {}

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(0, 34);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }

    const auto &theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(
        theme.backgroundColor, QColor(QStringLiteral("#20242A")));
    const QColor surface = themeColor(
        theme.secondaryBackgroundColor, QColor(QStringLiteral("#2B3038")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    const QColor accent = themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
    const QColor selection = themeColor(
        theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor rackColor = toneColor(toneFromRackIndex(rackIndex_), text, accent);
    const QColor muted = blendColor(rackColor, background, 0.58);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool enabled = index.data(kEffectRackEnabledRole).toBool();
    const bool hasMask = index.data(kEffectRackHasMaskRole).toBool();
    const int maskCount = index.data(kEffectRackMaskCountRole).toInt();
    const QString effectId = index.data(Qt::UserRole).toString().trimmed();
    const QString effectName = index.data(kEffectRackNameRole).toString().trimmed();

    painter->save();
    const QRect rowRect = option.rect.adjusted(2, 2, -2, -2);
    painter->fillRect(rowRect, selected ? selection : surface);

    if (effectId.isEmpty()) {
      painter->setPen(blendColor(text, background, 0.52));
      painter->drawText(rowRect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
      painter->restore();
      return;
    }

    const QPoint indicator(rowRect.left() + 10, rowRect.center().y());
    painter->setPen(Qt::NoPen);
    painter->setBrush(enabled ? rackColor : muted);
    painter->drawEllipse(indicator, 4, 4);

    QRect textRect = rowRect.adjusted(22, 0, -6, 0);
    QString maskLabel;
    if (hasMask) {
      maskLabel = maskCount > 0 ? QStringLiteral("Mask %1").arg(maskCount)
                                : QStringLiteral("Mask");
      QFontMetrics metrics(option.font);
      const int chipWidth = metrics.horizontalAdvance(maskLabel) + 12;
      const QRect chipRect(rowRect.right() - chipWidth, rowRect.center().y() - 9,
                           chipWidth, 18);
      painter->setBrush(blendColor(rackColor, background, selected ? 0.28 : 0.16));
      painter->drawRoundedRect(chipRect, 4, 4);
      painter->setPen(enabled ? text : muted);
      painter->drawText(chipRect, Qt::AlignCenter, maskLabel);
      textRect.setRight(chipRect.left() - 6);
    }

    QFont nameFont = option.font;
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(enabled ? text : muted);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      effectName.isEmpty() ? index.data(Qt::DisplayRole).toString()
                                           : effectName);
    painter->restore();
  }

private:
  int rackIndex_ = 0;
};

constexpr int rasterizerRackIndex() {
  return static_cast<int>(EffectPipelineStage::Rasterizer) - 1;
}

bool isLayerVisibleEffectStage(EffectPipelineStage stage) {
  return stage == EffectPipelineStage::Rasterizer;
}

QString effectStackStateSignature(
    const std::vector<ArtifactAbstractEffectPtr> &effects) {
  QString signature;
  signature.reserve(static_cast<int>(effects.size()) * 48);
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    signature += effect->effectID().toQString();
    signature += QLatin1Char('|');
    signature += effect->displayName().toQString();
    signature += QLatin1Char('|');
    signature += effect->isEnabled() ? QLatin1Char('1') : QLatin1Char('0');
    signature += QLatin1Char('|');
  }
  return signature;
}

QString matteTypeToText(MatteType type) {
  switch (type) {
  case MatteType::Alpha:
    return QStringLiteral("Alpha");
  case MatteType::Luma:
    return QStringLiteral("Luma");
  case MatteType::InverseAlpha:
    return QStringLiteral("Inverted Alpha");
  case MatteType::InverseLuma:
    return QStringLiteral("Inverted Luma");
  }
  return QStringLiteral("Matte");
}

QString matteReferenceSummary(const ArtifactCompositionPtr &comp,
                              const ArtifactAbstractLayerPtr &layer,
                              bool *hasInvalid = nullptr) {
  if (hasInvalid) {
    *hasInvalid = false;
  }
  if (!layer) {
    return QStringLiteral("Matte: none");
  }

  const auto refs = layer->matteReferences();
  if (refs.empty()) {
    return QStringLiteral("Matte: none");
  }

  QStringList parts;
  parts.reserve(static_cast<int>(refs.size()));
  for (const auto &ref : refs) {
    QString sourceName = QStringLiteral("<missing>");
    if (comp && !ref.sourceLayerId.isNil()) {
      if (auto source = comp->layerById(ref.sourceLayerId)) {
        sourceName = source->layerName().trimmed().isEmpty()
                         ? ref.sourceLayerId.toString()
                         : source->layerName();
      } else if (hasInvalid) {
        *hasInvalid = true;
      }
    } else if (hasInvalid) {
      *hasInvalid = true;
    }

    if (ref.sourceLayerId == layer->id() || ref.sourceLayerId.isNil()) {
      if (hasInvalid) {
        *hasInvalid = true;
      }
    }

    QString entry = QStringLiteral("%1 (%2)").arg(sourceName, matteTypeToText(ref.type));
    if (!ref.enabled) {
      entry += QStringLiteral(" off");
    }
    if (ref.invert) {
      entry += QStringLiteral(" inverted");
    }
    parts.push_back(entry);
  }

  return QStringLiteral("Matte: %1").arg(parts.join(QStringLiteral(", ")));
}

QString proxySummary(const ArtifactAbstractLayerPtr &layer,
                     QString *proxyPathOut = nullptr,
                     bool *hasProxyOut = nullptr) {
  if (proxyPathOut) {
    proxyPathOut->clear();
  }
  if (hasProxyOut) {
    *hasProxyOut = false;
  }
  const auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
  if (!videoLayer) {
    return QStringLiteral("Proxy: not available");
  }

  if (proxyPathOut) {
    *proxyPathOut = videoLayer->proxyPath();
  }
  const bool hasProxy = videoLayer->hasProxy();
  if (hasProxyOut) {
    *hasProxyOut = hasProxy;
  }

  const QString qualityText = [&]() -> QString {
    switch (videoLayer->proxyQuality()) {
    case ProxyQuality::Quarter:
      return QStringLiteral("1/4");
    case ProxyQuality::Half:
      return QStringLiteral("1/2");
    case ProxyQuality::Full:
      return QStringLiteral("Full");
    case ProxyQuality::None:
    default:
      return QStringLiteral("None");
    }
  }();

  if (!hasProxy) {
    return QStringLiteral("Proxy: none");
  }

  const QString path = videoLayer->proxyPath();
  const QString fileName = path.isEmpty() ? QStringLiteral("<unknown>") : QFileInfo(path).fileName();
  return QStringLiteral("Proxy: %1 | %2").arg(qualityText, fileName);
}

ArtifactCompositionPtr resolveCompositionForId(const CompositionID &compositionId) {
  auto *service = ArtifactProjectService::instance();
  if (!service || compositionId.isNil()) {
    return {};
  }
  auto result = service->findComposition(compositionId);
  if (!result.success) {
    return {};
  }
  return result.ptr.lock();
}

bool applyMatteTypeToLayer(const CompositionID &compositionId,
                           const LayerID &layerId,
                           int matteIndex,
                           MatteType matteType) {
  auto comp = resolveCompositionForId(compositionId);
  if (!comp || layerId.isNil() || matteIndex < 0) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  if (!layer) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  if (matteIndex >= static_cast<int>(beforeRefs.size())) {
    return false;
  }

  auto afterRefs = beforeRefs;
  auto &ref = afterRefs[matteIndex];
  const MatteType previousType = ref.type;
  const bool previousInvert = ref.invert;
  ref.type = matteType;
  ref.invert = false;
  if (ref.type == previousType && ref.invert == previousInvert) {
    return false;
  }

  auto *cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                    std::move(beforeRefs),
                                                    std::move(afterRefs));
  UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
  return true;
}

bool setMatteSourceToLayer(const CompositionID &compositionId,
                           const LayerID &layerId,
                           int matteIndex,
                           const LayerID &sourceLayerId) {
  auto comp = resolveCompositionForId(compositionId);
  if (!comp || layerId.isNil() || sourceLayerId.isNil() || matteIndex < 0 ||
      layerId == sourceLayerId) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  if (!layer || !comp->containsLayerById(sourceLayerId)) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  if (matteIndex >= static_cast<int>(beforeRefs.size())) {
    return false;
  }

  auto afterRefs = beforeRefs;
  auto &ref = afterRefs[matteIndex];
  if (ref.sourceLayerId == sourceLayerId) {
    return false;
  }

  ref.sourceLayerId = sourceLayerId;
  ref.enabled = true;

  auto *cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                    std::move(beforeRefs),
                                                    std::move(afterRefs));
  UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
  return true;
}

bool addMatteSourceToLayer(const CompositionID &compositionId,
                           const LayerID &layerId,
                           const LayerID &sourceLayerId) {
  auto comp = resolveCompositionForId(compositionId);
  if (!comp || layerId.isNil() || sourceLayerId.isNil() ||
      layerId == sourceLayerId) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  if (!layer || !comp->containsLayerById(sourceLayerId)) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  auto afterRefs = beforeRefs;

  LayerMatteReference ref;
  ref.sourceLayerId = sourceLayerId;
  ref.enabled = true;
  ref.type = MatteType::Alpha;
  ref.blendMode = MatteBlendMode::Add;
  ref.fitMode = MatteFitMode::Stretch;
  ref.opacity = 1.0f;
  ref.invert = false;
  afterRefs.push_back(ref);

  auto *cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                    std::move(beforeRefs),
                                                    std::move(afterRefs));
  UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
  return true;
}

bool clearMatteReferenceFromLayer(const CompositionID &compositionId,
                                  const LayerID &layerId,
                                  int matteIndex) {
  auto comp = resolveCompositionForId(compositionId);
  if (!comp || layerId.isNil() || matteIndex < 0) {
    return false;
  }

  auto layer = comp->layerById(layerId);
  if (!layer) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  if (matteIndex >= static_cast<int>(beforeRefs.size())) {
    return false;
  }

  auto afterRefs = beforeRefs;
  afterRefs.erase(afterRefs.begin() + matteIndex);

  auto *cmd = new ChangeLayerMatteReferencesCommand(layer,
                                                    std::move(beforeRefs),
                                                    std::move(afterRefs));
  UndoManager::instance()->push(std::unique_ptr<ChangeLayerMatteReferencesCommand>(cmd));
  return true;
}

class MatteInfoLabel final : public QLabel {
public:
  explicit MatteInfoLabel(QWidget *parent = nullptr)
      : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Left click: focus matte source or create one from the selected layer. Right click: change matte type, replace source, or clear it."));
  }

  void setMatteContext(const CompositionID &compositionId,
                       const ArtifactAbstractLayerPtr &layer,
                       const ArtifactCompositionPtr &composition) {
    compositionId_ = compositionId;
    layerId_ = layer ? layer->id() : LayerID();
    composition_ = composition;
    const bool hasMatteRefs = layer && !layer->matteReferences().empty();
    const ArtifactAbstractLayerPtr selectedLayer =
        ArtifactLayerSelectionManager::instance()
            ? ArtifactLayerSelectionManager::instance()->currentLayer()
            : ArtifactAbstractLayerPtr{};
    const bool canCreateFromSelection =
        composition && layer && selectedLayer && selectedLayer->id() != layerId_ &&
        composition->containsLayerById(selectedLayer->id());
    setCursor((hasMatteRefs || canCreateFromSelection) ? Qt::PointingHandCursor
                                                       : Qt::ArrowCursor);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!event) {
      QLabel::mousePressEvent(event);
      return;
    }

    auto composition = composition_ ? composition_ : resolveCompositionForId(compositionId_);
    if (!composition || layerId_.isNil()) {
      QLabel::mousePressEvent(event);
      return;
    }

    auto layer = composition->layerById(layerId_);
    if (!layer) {
      QLabel::mousePressEvent(event);
      return;
    }

    const auto refs = layer->matteReferences();
    const ArtifactAbstractLayerPtr selectedLayer =
        ArtifactLayerSelectionManager::instance()
            ? ArtifactLayerSelectionManager::instance()->currentLayer()
            : ArtifactAbstractLayerPtr{};
    if (refs.empty()) {
      if (event->button() == Qt::LeftButton && selectedLayer &&
          selectedLayer->id() != layerId_ &&
          composition->containsLayerById(selectedLayer->id())) {
        if (addMatteSourceToLayer(compositionId_, layerId_, selectedLayer->id())) {
          event->accept();
          return;
        }
      }
      QLabel::mousePressEvent(event);
      return;
    }

    if (event->button() == Qt::LeftButton) {
      const auto &ref = refs.front();
      if (!ref.sourceLayerId.isNil()) {
        if (auto *service = ArtifactProjectService::instance()) {
          service->selectLayer(ref.sourceLayerId);
        }
        event->accept();
        return;
      }
    }

    if (event->button() == Qt::RightButton) {
      QMenu menu(this);
      const QStringList typeLabels = {
          QStringLiteral("Alpha"),
          QStringLiteral("Luma"),
          QStringLiteral("Inverted Alpha"),
          QStringLiteral("Inverted Luma")};

      if (refs.empty()) {
        if (selectedLayer && selectedLayer->id() != layerId_ &&
            composition && composition->containsLayerById(selectedLayer->id())) {
          QAction *addAction =
              menu.addAction(QStringLiteral("Use selected layer as source"));
          addAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("add_selected")},
                                         {QStringLiteral("selectedLayerId"), selectedLayer->id().toString()}});
        }
      }

      for (int i = 0; i < refs.size(); ++i) {
        const auto &ref = refs[i];
        QString sourceName = QStringLiteral("<missing>");
        if (!ref.sourceLayerId.isNil()) {
          if (auto source = composition->layerById(ref.sourceLayerId)) {
            const QString name = source->layerName().trimmed();
            sourceName = name.isEmpty() ? ref.sourceLayerId.toString() : name;
          } else {
            sourceName = ref.sourceLayerId.toString();
          }
        }

        QMenu *refMenu = menu.addMenu(QStringLiteral("Matte %1: %2").arg(i + 1).arg(sourceName));
        if (ref.sourceLayerId.isNil()) {
          QAction *disabled = refMenu->addAction(QStringLiteral("Missing source"));
          disabled->setEnabled(false);
          continue;
        }

        QAction *focusAction = refMenu->addAction(QStringLiteral("Focus source"));
        focusAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("focus")},
                                         {QStringLiteral("index"), i}});

        if (auto *selMgr = ArtifactLayerSelectionManager::instance()) {
          const auto selected = selMgr->currentLayer();
          if (selected && selected->id() != layerId_ &&
              composition && composition->containsLayerById(selected->id())) {
            QAction *useSelectedAction =
                refMenu->addAction(QStringLiteral("Use selected layer as source"));
            useSelectedAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("use_selected")},
                                                   {QStringLiteral("index"), i},
                                                   {QStringLiteral("selectedLayerId"), selected->id().toString()}});
          }
        }

        QAction *clearAction = refMenu->addAction(QStringLiteral("Clear source"));
        clearAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("clear")},
                                         {QStringLiteral("index"), i}});

        QMenu *typeMenu = refMenu->addMenu(QStringLiteral("Set matte type"));
        for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
          QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
          typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("type")},
                                          {QStringLiteral("index"), i},
                                          {QStringLiteral("type"), typeIndex}});
        }
      }

      if (QAction *chosen = menu.exec(QCursor::pos())) {
        const QVariantMap data = chosen->data().toMap();
        const QString kind = data.value(QStringLiteral("kind")).toString();
        bool indexOk = false;
        const int index = data.value(QStringLiteral("index")).toInt(&indexOk);
        if (!indexOk) {
          return;
        }
        if (kind == QStringLiteral("add_selected")) {
          const auto selectedLayerId = LayerID(data.value(QStringLiteral("selectedLayerId")).toString());
          addMatteSourceToLayer(compositionId_, layerId_, selectedLayerId);
          event->accept();
          return;
        }
        if (kind == QStringLiteral("focus")) {
          if (index >= 0 && index < static_cast<int>(refs.size())) {
            const auto &ref = refs[index];
            if (!ref.sourceLayerId.isNil()) {
              if (auto *service = ArtifactProjectService::instance()) {
                service->selectLayer(ref.sourceLayerId);
              }
            }
          }
        } else if (kind == QStringLiteral("use_selected")) {
          const auto selectedLayerId = LayerID(data.value(QStringLiteral("selectedLayerId")).toString());
          if (index >= 0 && index < static_cast<int>(refs.size())) {
            setMatteSourceToLayer(compositionId_, layerId_, index, selectedLayerId);
          }
        } else if (kind == QStringLiteral("clear")) {
          if (index >= 0 && index < static_cast<int>(refs.size())) {
            clearMatteReferenceFromLayer(compositionId_, layerId_, index);
          }
        } else if (kind == QStringLiteral("type")) {
          bool typeOk = false;
          const int typeValue = data.value(QStringLiteral("type")).toInt(&typeOk);
          if (!typeOk) {
            return;
          }
          if (index >= 0 && index < static_cast<int>(refs.size()) &&
              typeValue >= 0 && typeValue <= static_cast<int>(MatteType::InverseLuma)) {
            applyMatteTypeToLayer(compositionId_, layerId_, index,
                                  static_cast<MatteType>(typeValue));
          }
        }
      }
      event->accept();
      return;
    }

    QLabel::mousePressEvent(event);
  }

private:
  CompositionID compositionId_;
  LayerID layerId_;
  ArtifactCompositionPtr composition_;
};

class ProxyInfoLabel final : public QLabel {
public:
  explicit ProxyInfoLabel(QWidget *parent = nullptr)
      : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Left click: open proxy folder."));
  }

  void setProxyContext(const ArtifactAbstractLayerPtr &layer) {
    layer_ = layer;
    const bool hasProxy = layer && std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) &&
                          std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)->hasProxy();
    setCursor(hasProxy ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!event) {
      QLabel::mousePressEvent(event);
      return;
    }
    if (event->button() == Qt::LeftButton) {
      const auto videoLayer = layer_ ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer_) : nullptr;
      if (videoLayer && videoLayer->hasProxy()) {
        const QString proxyPath = videoLayer->proxyPath();
        if (!proxyPath.isEmpty() && QFileInfo::exists(proxyPath)) {
          QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(proxyPath).absolutePath()));
          event->accept();
          return;
        }
      }
    }
    QLabel::mousePressEvent(event);
  }

private:
  ArtifactAbstractLayerPtr layer_;
};

class InspectorActionButton final : public QPushButton {
public:
  using QPushButton::QPushButton;

  void setOwnerDrawn(bool enabled) {
    ownerDrawn_ = enabled;
    setAttribute(Qt::WA_Hover, enabled);
    update();
  }

  void setAction(std::function<void()> action) {
    action_ = std::move(action);
  }

  void triggerAction() {
    if (action_) {
      action_();
    }
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    if (!ownerDrawn_) {
      QPushButton::paintEvent(event);
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    const bool hovered = underMouse();
    const bool pressed = isDown();
    const bool active = isCheckable() && isChecked();
    const QColor base = pal.color(isEnabled() ? QPalette::Active
                                              : QPalette::Disabled,
                                  QPalette::Button);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor fill = pressed
                            ? blendColor(base, accent, 0.46)
                            : active
                                  ? blendColor(base, accent, 0.34)
                                  : hovered
                                        ? blendColor(base, accent, 0.14)
                                        : base;
    const QRectF surface = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(fill);
    painter.drawRoundedRect(surface, 4.0, 4.0);

    if (active) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(accent);
      painter.drawRoundedRect(QRectF(3.0, 7.0, 3.0,
                                     qMax(4, height() - 14)),
                              1.5, 1.5);
    }

    QFont textFont = font();
    textFont.setWeight(active ? QFont::DemiBold : QFont::Normal);
    painter.setFont(textFont);
    painter.setPen(pal.color(isEnabled() ? QPalette::Active
                                         : QPalette::Disabled,
                             QPalette::ButtonText));
    const QRect textRect = rect().adjusted(active ? 12 : 8, 0, -8, 0);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());

    if (hasFocus()) {
      QPen focusPen(accent);
      focusPen.setStyle(Qt::DashLine);
      painter.setPen(focusPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(surface.adjusted(2, 2, -2, -2), 3.0, 3.0);
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate =
        event && event->button() == Qt::LeftButton && isEnabled() &&
        isDown() && rect().contains(event->position().toPoint());
    QPushButton::mouseReleaseEvent(event);
    if (activate && action_) {
      action_();
    }
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    const bool activate =
        event && isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter);
    QPushButton::keyReleaseEvent(event);
    if (activate && action_) {
      action_();
    }
  }

private:
  std::function<void()> action_;
  bool ownerDrawn_ = false;
};

class ComponentStackItemDelegate final : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(0, 30);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }
    const QPalette pal = option.palette;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const QRectF row = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
    const QColor base = pal.color(QPalette::Base);
    const QColor accent = pal.color(QPalette::Highlight);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pal.color(QPalette::Mid));
    painter->setBrush(selected ? blendColor(base, accent, 0.42)
                               : hovered ? blendColor(base, accent, 0.12)
                                         : pal.color(QPalette::AlternateBase));
    painter->drawRoundedRect(row, 3.0, 3.0);
    painter->setPen(selected ? pal.color(QPalette::HighlightedText)
                             : pal.color(QPalette::Text));
    QFont itemFont = option.font;
    itemFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
    painter->setFont(itemFont);
    painter->drawText(option.rect.adjusted(10, 0, -8, 0),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
  }
};

class ComponentDivider final : public QFrame {
 public:
  using QFrame::QFrame;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(rect().left(), rect().center().y(),
                     rect().right(), rect().center().y());
  }
};

class InspectorSelectionList final : public QListWidget {
public:
  using QListWidget::QListWidget;

  void setSelectionAction(
      std::function<void(QListWidgetItem *)> action) {
    action_ = std::move(action);
  }

  void setSelectionActionEnabled(bool enabled) {
    selectionActionEnabled_ = enabled;
  }

protected:
  void currentChanged(const QModelIndex &current,
                      const QModelIndex &previous) override {
    QListWidget::currentChanged(current, previous);
    if (selectionActionEnabled_ && action_) {
      action_(currentItem());
    }
  }

private:
  bool selectionActionEnabled_ = true;
  std::function<void(QListWidgetItem *)> action_;
};

class SelectionActionBlocker final {
public:
  explicit SelectionActionBlocker(InspectorSelectionList *list)
      : list_(list) {
    if (list_) {
      list_->setSelectionActionEnabled(false);
    }
  }

  ~SelectionActionBlocker() {
    if (list_) {
      list_->setSelectionActionEnabled(true);
    }
  }

private:
  InspectorSelectionList *list_ = nullptr;
};
} // namespace

W_OBJECT_IMPL(ArtifactInspectorWidget)

class ArtifactInspectorWidget::Impl {
private:
public:
  Impl();
  ~Impl();
  QWidget *containerWidget = nullptr;
  QTabWidget *tabWidget = nullptr;

  // Layer Info Tab
  QGroupBox *compositionNoteGroup = nullptr;
  QPlainTextEdit *compositionNoteEdit = nullptr;
  QGroupBox *layerNoteGroup = nullptr;
  QPlainTextEdit *layerNoteEdit = nullptr;
  QLabel *layerNameLabel = nullptr;
  QLabel *layerTypeLabel = nullptr;
  MatteInfoLabel *matteInfoLabel = nullptr;
  ProxyInfoLabel *proxyInfoLabel = nullptr;
  QWidget *componentsGroup = nullptr;
  QLabel *componentsSummaryLabel = nullptr;
  QLabel *activeComponentLabel = nullptr;
  QString focusedComponentName_;
  LayerID focusedComponentLayerId_;
  InspectorActionButton *addComponentButton = nullptr;
  InspectorActionButton *physicsComponentButton = nullptr;
  InspectorActionButton *scriptComponentButton = nullptr;
  InspectorActionButton *layoutComponentButton = nullptr;
  InspectorActionButton *cloneComponentButton = nullptr;
  InspectorActionButton *fluidComponentButton = nullptr;
  InspectorActionButton *generatorComponentButton = nullptr;
  InspectorActionButton *removeGeneratorComponentButton = nullptr;
  InspectorActionButton *generatorMoveUpButton = nullptr;
  InspectorActionButton *generatorMoveDownButton = nullptr;
  InspectorSelectionList *generatorListWidget = nullptr;
  InspectorActionButton *fieldComponentButton = nullptr;
  InspectorActionButton *removeFieldComponentButton = nullptr;
  InspectorActionButton *fieldMoveUpButton = nullptr;
  InspectorActionButton *fieldMoveDownButton = nullptr;
  InspectorSelectionList *fieldListWidget = nullptr;
  InspectorActionButton *cloneModifierButton = nullptr;
  InspectorActionButton *removeCloneModifierButton = nullptr;
  InspectorActionButton *cloneModifierMoveUpButton = nullptr;
  InspectorActionButton *cloneModifierMoveDownButton = nullptr;
  InspectorSelectionList *cloneModifierListWidget = nullptr;
  QWidget *clonerStructureWidget = nullptr;
  QLabel *componentUtilitiesLabel = nullptr;
  InspectorActionButton *openScriptButton = nullptr;
  InspectorActionButton *applyLipSyncButton = nullptr;
  ArtifactPropertyWidget *componentPropertyWidget = nullptr;
  QString lastComponentPropertyStateSignature_;
  QLabel *statusLabel = nullptr;

  // Effects Pipeline Tab
  QScrollArea *effectsScrollArea = nullptr;
  QWidget *effectsTabWidget = nullptr;
  QLabel *effectsStateLabel = nullptr;
  QLabel *effectsTargetLabel = nullptr;
  QLabel *effectsStackSummaryLabel = nullptr;
  QTabWidget *effectsModeTabs = nullptr;
  QLabel *effectEditorTitleLabel = nullptr;
  QLabel *effectParametersHintLabel = nullptr;
  InspectorActionButton *effectEnableButton = nullptr;
  ArtifactPropertyWidget *effectPropertyWidget = nullptr;
  QPushButton *effectsQuickAddButton = nullptr;
  QString focusedEffectId_;
  ArtifactAbstractLayerPtr lastSyncedLayer_;
  QString lastSyncedFocusedEffectId_;
  QString lastEffectPropertyStateSignature_;

  struct EffectRack {
    QGroupBox *groupBox = nullptr;
    QListWidget *listWidget = nullptr;
    QPushButton *addButton = nullptr;
    QPushButton *removeButton = nullptr;
    QPushButton *moveUpButton = nullptr;
    QPushButton *moveDownButton = nullptr;
  };
  EffectRack racks[kEffectRackCount];
  QMenu *inspectorMenu_ = nullptr;

  CompositionID currentCompositionId_;
  LayerID currentLayerId_;
  QMetaObject::Connection compositionNoteConnection_;
  QMetaObject::Connection layerNoteConnection_;
  ArtifactCore::EventBus::Subscription compositionNoteSubscription_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  QString lastLayerInfoSignature_;
  QString lastMatteInfoSignature_;
  std::array<QString, kEffectRackCount> lastRackSignatures_{};
  QString lastCompositionNoteText_;
  QString lastLayerNoteText_;
  int refreshMask_ = 0;
  bool refreshQueued_ = false;
  bool suppressRackSelectionSync_ = false;
  bool syncingEffectPropertyWidget_ = false;

  enum RefreshReason {
    CompositionNoteDirty = 1 << 0,
    LayerNoteDirty = 1 << 1,
    LayerInfoDirty = 1 << 2,
    EffectsDirty = 1 << 3
  };

  void rebuildMenu();
  void defaultHandleKeyPressEvent(QKeyEvent *event);
  void defaultHandleMousePressEvent(QMouseEvent *event);

  void showContextMenu();
  void showContextMenu(const QPoint &globalPos);
  void showRackContextMenu(int rackIndex, QListWidgetItem *item,
                           const QPoint &globalPos);
  bool removeEffectById(const QString &effectId);
  bool setEffectEnabledById(const QString &effectId, bool enabled);
  bool moveEffectById(const QString &effectId, int direction);
  bool editingCompositionEffects() const;
  ArtifactCompositionPtr currentEffectComposition() const;
  std::vector<ArtifactAbstractEffectPtr> currentEffectStack() const;
  ArtifactAbstractEffectPtr currentEffectById(const QString &effectId) const;
  void handleProjectCreated();
  void handleProjectClosed();
  void handleCompositionCreated(const CompositionID &id);
  void handleCompositionChanged(const CompositionID &id);
  void handleLayerSelected(const LayerSelectionChangedEvent &event);
  void updateCompositionNote();
  void updateLayerNote();
  void updateLayerInfo();
  void updateComponentControls(const ArtifactAbstractLayerPtr &layer);
  void focusComponentProperties(const ArtifactAbstractLayerPtr &layer,
                                const QString &filterText);
  void syncComponentPropertyWidget(const ArtifactAbstractLayerPtr &layer,
                                   const QString &filterText);
  void updateEffectsList();
  void addSelectedEffectToCurrentTarget(const QString &effectId);
  void updateEffectRackItemEnabled(const QString &effectId, bool enabled);
  void updatePropertiesForEffect(const QString &effectId);
  QString currentSelectedEffectIdFromRacks() const;
  void syncFocusedEffectFromRackSelection();
  void syncEffectPropertyWidget();
  void handleApplyLipSyncToSwitchLayer();
  void handleAddEffectClicked(int rackIndex);
  void handleRemoveEffectClicked(int rackIndex);
  void refreshRackButtons();
  void setEffectRackEnabled(bool enabled);
  void updateEffectRackVisibility();
  void setEffectsStateText(const QString &text, bool visible);
  void setNoProjectState();
  void setNoLayerState();
  void scheduleRefresh(int reasonMask = CompositionNoteDirty | LayerNoteDirty |
                                        LayerInfoDirty | EffectsDirty);
  void refreshNow();
  QString
  computeLayerInfoSignature(const ArtifactAbstractLayerPtr &layer) const;
  QString computeRackSignature(
      int rackIndex,
      const std::vector<ArtifactAbstractEffectPtr> &effects) const;
};

ArtifactInspectorWidget::Impl::Impl() {}

void ArtifactInspectorWidget::Impl::scheduleRefresh(int reasonMask) {
  QObject *context = containerWidget ? static_cast<QObject *>(containerWidget)
                                     : static_cast<QObject *>(tabWidget);
  if (!context) {
    refreshNow();
    return;
  }
  refreshMask_ |= reasonMask;
  if (refreshQueued_) {
    return;
  }
  refreshQueued_ = true;
  QTimer::singleShot(0, context, [this]() {
    if (!refreshQueued_) {
      return;
    }
    refreshNow();
  });
}

void ArtifactInspectorWidget::Impl::refreshNow() {
  const int mask = refreshMask_;
  refreshMask_ = 0;
  refreshQueued_ = false;
  if (mask & CompositionNoteDirty) {
    updateCompositionNote();
  }
  if (mask & LayerNoteDirty) {
    updateLayerNote();
  }
  if (mask & LayerInfoDirty) {
    updateLayerInfo();
  }
  if (mask & EffectsDirty) {
    updateEffectsList();
  }
}

void ArtifactInspectorWidget::Impl::updatePropertiesForEffect(
    const QString &effectId) {
  const QString normalized = effectId.trimmed();
  if (focusedEffectId_ == normalized) {
    return;
  }
  focusedEffectId_ = normalized;
  if (!normalized.isEmpty() && effectsModeTabs) {
    effectsModeTabs->setCurrentIndex(1);
  }
  syncEffectPropertyWidget();
}

QString ArtifactInspectorWidget::Impl::currentSelectedEffectIdFromRacks() const {
  for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
    auto *list = racks[rackIndex].listWidget;
    if (!list) {
      continue;
    }
    auto *item = list->currentItem();
    if (!item) {
      continue;
    }
    const QString id = item->data(Qt::UserRole).toString().trimmed();
    if (!id.isEmpty()) {
      return id;
    }
  }
  return {};
}

void ArtifactInspectorWidget::Impl::syncFocusedEffectFromRackSelection() {
  if (suppressRackSelectionSync_) {
    return;
  }
  updatePropertiesForEffect(currentSelectedEffectIdFromRacks());
  refreshRackButtons();
}

bool ArtifactInspectorWidget::Impl::editingCompositionEffects() const {
  return !currentCompositionId_.isNil() && currentLayerId_.isNil();
}

ArtifactCompositionPtr ArtifactInspectorWidget::Impl::currentEffectComposition()
    const {
  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil()) {
    return {};
  }
  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    return {};
  }
  return findResult.ptr.lock();
}

std::vector<ArtifactAbstractEffectPtr>
ArtifactInspectorWidget::Impl::currentEffectStack() const {
  auto comp = currentEffectComposition();
  if (!comp) {
    return {};
  }
  if (editingCompositionEffects()) {
    return comp->getEffects();
  }
  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    return {};
  }
  std::vector<ArtifactAbstractEffectPtr> visibleEffects;
  for (const auto &effect : layer->getEffects()) {
    if (effect && isLayerVisibleEffectStage(effect->pipelineStage())) {
      visibleEffects.push_back(effect);
    }
  }
  return visibleEffects;
}

ArtifactAbstractEffectPtr
ArtifactInspectorWidget::Impl::currentEffectById(const QString &effectId) const {
  const QString trimmedId = effectId.trimmed();
  if (trimmedId.isEmpty()) {
    return {};
  }
  for (const auto &effect : currentEffectStack()) {
    if (effect && effect->effectID().toQString() == trimmedId) {
      return effect;
    }
  }
  return {};
}

void ArtifactInspectorWidget::Impl::syncEffectPropertyWidget() {
  if (!effectPropertyWidget) {
    return;
  }
  if (syncingEffectPropertyWidget_) {
    return;
  }
  syncingEffectPropertyWidget_ = true;
  const auto clearSyncing = qScopeGuard([this]() {
    syncingEffectPropertyWidget_ = false;
  });

  const auto showEffectGuidance = [this](const QString &text,
                                         const bool showPropertyWidget) {
    effectPropertyWidget->setVisible(showPropertyWidget);
    if (effectEnableButton) {
      effectEnableButton->setVisible(showPropertyWidget);
      effectEnableButton->setEnabled(showPropertyWidget);
    }
    if (effectEditorTitleLabel) {
      effectEditorTitleLabel->setText(showPropertyWidget
                                          ? QStringLiteral("Effect Editor")
                                          : QStringLiteral("No effect selected"));
    }
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(text);
      effectParametersHintLabel->setVisible(true);
    }
  };

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil()) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer or composition effect."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  if (editingCompositionEffects()) {
    const auto effect = currentEffectById(focusedEffectId_);
    if (!effect) {
      focusedEffectId_.clear();
    }
    const QString resolvedFocusedEffectId = focusedEffectId_.trimmed();
    const QString stateSignature =
        QStringLiteral("composition:%1|%2")
            .arg(comp->id().toString(), resolvedFocusedEffectId);
    if (!lastSyncedLayer_ &&
        resolvedFocusedEffectId == lastSyncedFocusedEffectId_ &&
        stateSignature == lastEffectPropertyStateSignature_) {
      return;
    }
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_ = resolvedFocusedEffectId;
    lastEffectPropertyStateSignature_ = stateSignature;
    effectPropertyWidget->setCompositionEffects(comp->getEffects());
    effectPropertyWidget->setFocusedEffectId(resolvedFocusedEffectId);
    const bool hasFocus = !resolvedFocusedEffectId.isEmpty();
    showEffectGuidance(
        hasFocus
            ? QStringLiteral("Edit the selected composition effect below.")
            : QStringLiteral("Select a composition effect in any rack."),
        hasFocus);
    if (hasFocus && effectEditorTitleLabel) {
      effectEditorTitleLabel->setText(
          effect
              ? QStringLiteral("Composition Effect  |  %1  |  %2")
                    .arg(effect->displayName().toQString(),
                         stageDisplayName(effect->pipelineStage()))
              : QStringLiteral("Composition Effect  |  %1")
                    .arg(resolvedFocusedEffectId));
    }
    if (effectEnableButton && hasFocus && effect) {
      effectEnableButton->setChecked(effect->isEnabled());
      effectEnableButton->setText(effect->isEnabled()
                                      ? QStringLiteral("Enabled")
                                      : QStringLiteral("Disabled"));
      applyInspectorComponentStateButton(effectEnableButton,
                                         effect->isEnabled());
    }
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    effectPropertyWidget->clear();
    showEffectGuidance(
        QStringLiteral("Open a composition, then select a layer and effect. The selected effect's parameters appear below."),
        false);
    lastSyncedLayer_.reset();
    lastSyncedFocusedEffectId_.clear();
    lastEffectPropertyStateSignature_.clear();
    return;
  }

  const auto visibleEffects = currentEffectStack();
  bool effectExists = false;
  QString focusedEffectName;
  QString resolvedFocusedEffectId = focusedEffectId_.trimmed();
  if (!focusedEffectId_.trimmed().isEmpty()) {
    for (const auto &effect : visibleEffects) {
      if (effect && effect->effectID().toQString() == focusedEffectId_) {
        effectExists = true;
        focusedEffectName = effect->displayName().toQString();
        break;
      }
    }
  }

  if (!effectExists) {
    focusedEffectId_.clear();
    resolvedFocusedEffectId.clear();
  }

  const QString stateSignature = QStringLiteral("%1|%2|%3")
                                     .arg(layer->id().toString(),
                                          resolvedFocusedEffectId,
                                          effectStackStateSignature(
                                              visibleEffects));
  if (layer == lastSyncedLayer_ &&
      resolvedFocusedEffectId == lastSyncedFocusedEffectId_ &&
      stateSignature == lastEffectPropertyStateSignature_) {
    return;
  }

  const bool layerChanged = layer != lastSyncedLayer_;
  lastSyncedLayer_ = layer;
  lastSyncedFocusedEffectId_ = resolvedFocusedEffectId;
  lastEffectPropertyStateSignature_ = stateSignature;

  if (layerChanged) {
    effectPropertyWidget->setLayer(layer);
  }
  effectPropertyWidget->setFocusedEffectId(resolvedFocusedEffectId);

  const bool hasFocus = !resolvedFocusedEffectId.trimmed().isEmpty();
  showEffectGuidance(
      hasFocus
          ? QStringLiteral("Edit the selected layer effect below.")
          : QStringLiteral("Select an effect in any rack."),
      hasFocus);
  if (hasFocus && effectEditorTitleLabel) {
    const auto focusedEffect = currentEffectById(resolvedFocusedEffectId);
    effectEditorTitleLabel->setText(
        focusedEffect
            ? QStringLiteral("Layer Effect  |  %1  |  %2")
                  .arg(focusedEffectName.isEmpty() ? resolvedFocusedEffectId
                                                   : focusedEffectName,
                       stageDisplayName(focusedEffect->pipelineStage()))
            : QStringLiteral("Layer Effect  |  %1")
                  .arg(focusedEffectName.isEmpty() ? resolvedFocusedEffectId
                                                   : focusedEffectName));
  }
  if (effectEnableButton && hasFocus) {
    const auto effect = currentEffectById(resolvedFocusedEffectId);
    if (effect) {
      effectEnableButton->setChecked(effect->isEnabled());
      effectEnableButton->setText(effect->isEnabled()
                                      ? QStringLiteral("Enabled")
                                      : QStringLiteral("Disabled"));
      applyInspectorComponentStateButton(effectEnableButton,
                                         effect->isEnabled());
    }
  }
}

namespace {
bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath);
}

QString defaultComponentInspectorFilter(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return QStringLiteral(
        "physics.enabled|component.script.enabled|"
        "component.layout.enabled|component.cloner.enabled|"
        "component.collision.enabled|component.crowd.enabled|"
        "component.particleEmitter.enabled|component.fluid.enabled");
  }
  QStringList filters = {
      QStringLiteral("physics.enabled"),
      QStringLiteral("component.script.enabled"),
      QStringLiteral("component.layout.enabled"),
      QStringLiteral("component.cloner.enabled"),
      QStringLiteral("component.collision.enabled"),
      QStringLiteral("component.crowd.enabled"),
      QStringLiteral("component.particleEmitter.enabled"),
      QStringLiteral("component.fluid.enabled"),
  };

  if (layerBooleanProperty(layer, QStringLiteral("physics.enabled"))) {
    filters.push_back(QStringLiteral("physics."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.script.enabled"))) {
    filters.push_back(QStringLiteral("component.script."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.layout.enabled"))) {
    filters.push_back(QStringLiteral("component.layout."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.cloner.enabled"))) {
    filters.push_back(QStringLiteral("component.cloner."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.collision.enabled"))) {
    filters.push_back(QStringLiteral("component.collision."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.crowd.enabled"))) {
    filters.push_back(QStringLiteral("component.crowd."));
  }
  if (layerBooleanProperty(
          layer, QStringLiteral("component.particleEmitter.enabled"))) {
    filters.push_back(QStringLiteral("component.particleEmitter."));
  }
  if (layerBooleanProperty(layer, QStringLiteral("component.fluid.enabled"))) {
    filters.push_back(QStringLiteral("component.fluid."));
  }

  filters.removeDuplicates();
  return filters.join(QStringLiteral("|"));
}

QString componentInspectorFilter(const QString &componentName) {
  if (componentName == QStringLiteral("Physics")) {
    return QStringLiteral("physics.");
  }
  if (componentName == QStringLiteral("Script")) {
    return QStringLiteral("component.script.");
  }
  if (componentName == QStringLiteral("Layout")) {
    return QStringLiteral("component.layout.");
  }
  if (componentName == QStringLiteral("Cloner")) {
    return QStringLiteral("component.cloner.");
  }
  if (componentName == QStringLiteral("Fluid")) {
    return QStringLiteral("component.fluid.");
  }
  return {};
}

QString generatorItemFilterText(const QListWidgetItem *item) {
  if (!item) {
    return {};
  }
  return item->data(Qt::UserRole + 1).toString().trimmed();
}

bool generatorItemSupportsReorder(const QListWidgetItem *item) {
  return item && item->data(Qt::UserRole + 2).toBool();
}

QString fieldItemFilterText(const QListWidgetItem *item) {
  if (!item) {
    return {};
  }
  return item->data(Qt::UserRole + 1).toString().trimmed();
}

QString cloneModifierItemFilterText(const QListWidgetItem *item) {
  if (!item) {
    return {};
  }
  return item->data(Qt::UserRole + 1).toString().trimmed();
}

QString componentTypeDisplayName(const QString &typeId) {
  QString name = typeId.section(QLatin1Char('.'), -1);
  name.replace(QLatin1Char('-'), QLatin1Char(' '));
  const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  QStringList displayWords;
  displayWords.reserve(words.size());
  for (QString word : words) {
    if (!word.isEmpty()) {
      word[0] = word[0].toUpper();
    }
    displayWords.push_back(word);
  }
  return displayWords.join(QLatin1Char(' '));
}

void ArtifactInspectorWidget::Impl::setEffectsStateText(const QString &text,
                                                        bool visible) {
  if (!effectsStateLabel)
    return;
  if (effectsStateLabel->text() == text &&
      effectsStateLabel->isVisible() == visible) {
    return;
  }
  effectsStateLabel->setText(text);
  effectsStateLabel->setVisible(visible);
}

namespace {
bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath) {
  if (!layer) {
    return false;
  }
  const auto groups = layer->getLayerPropertyGroups();
  Q_UNUSED(groups);
  const auto property = layer->getProperty(propertyPath);
  return property ? property->getValue().toBool() : false;
}

QString resolveScriptBindingPath(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return {};
  }
  const QJsonObject binding = layer->scriptBinding();
  const QStringList keys = {
      QStringLiteral("path"),
      QStringLiteral("file"),
      QStringLiteral("scriptPath"),
      QStringLiteral("scriptFile"),
  };
  for (const auto &key : keys) {
    const QString value = binding.value(key).toString().trimmed();
    if (!value.isEmpty()) {
      return value;
    }
  }
  return {};
}
} // namespace

void ArtifactInspectorWidget::Impl::updateComponentControls(
    const ArtifactAbstractLayerPtr &layer) {
  const LayerTabComponentState state = collectLayerTabComponentState(layer);
  const bool hasLayer = state.hasLayer;
  const bool canEditComponents = state.canEditComponents;
  QString activeName;

  if (!hasLayer ||
      (!focusedComponentLayerId_.isNil() &&
       focusedComponentLayerId_ != currentLayerId_)) {
    focusedComponentName_.clear();
    focusedComponentLayerId_ = LayerID{};
  }
  if (focusedComponentName_ == QStringLiteral("Generator") &&
      state.generatorCount > 0) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Field") &&
             state.fieldCount > 0) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Clone Modifier") &&
             state.cloneModifierCount > 0) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Cloner") &&
             state.cloneEnabled) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Layout") &&
             state.layoutEnabled) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Physics") &&
             state.physicsEnabled) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Fluid") &&
             state.fluidEnabled) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Script") &&
             state.scriptEnabled) {
    activeName = focusedComponentName_;
  } else if (state.cloneEnabled) {
    activeName = QStringLiteral("Cloner");
  } else if (state.layoutEnabled) {
    activeName = QStringLiteral("Layout");
  } else if (state.physicsEnabled) {
    activeName = QStringLiteral("Physics");
  } else if (state.fluidEnabled) {
    activeName = QStringLiteral("Fluid");
  } else if (state.scriptEnabled) {
    activeName = QStringLiteral("Script");
  }

  if (componentsGroup) {
    componentsGroup->setEnabled(canEditComponents);
  }
  if (clonerStructureWidget) {
    const bool showsClonerStructure =
        activeName == QStringLiteral("Cloner") ||
        activeName == QStringLiteral("Generator") ||
        activeName == QStringLiteral("Field") ||
        activeName == QStringLiteral("Clone Modifier");
    clonerStructureWidget->setVisible(
        hasLayer && state.cloneEnabled && showsClonerStructure);
  }
  if (activeComponentLabel) {
    activeComponentLabel->setText(
        activeName.isEmpty()
            ? QStringLiteral("Active Component  |  None")
            : QStringLiteral("Active Component  |  %1").arg(activeName));
    activeComponentLabel->setVisible(hasLayer);
  }
  if (physicsComponentButton) {
    physicsComponentButton->setEnabled(canEditComponents);
    physicsComponentButton->setChecked(activeName == QStringLiteral("Physics"));
    physicsComponentButton->setProperty("artifactComponentEnabled",
                                        state.physicsEnabled);
    physicsComponentButton->setText(QStringLiteral("Physics"));
    applyInspectorComponentStateButton(physicsComponentButton,
                                       activeName == QStringLiteral("Physics"));
    physicsComponentButton->setVisible(state.physicsEnabled);
    physicsComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Physics component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Physics."));
  }
  if (scriptComponentButton) {
    scriptComponentButton->setEnabled(canEditComponents);
    scriptComponentButton->setChecked(activeName == QStringLiteral("Script"));
    scriptComponentButton->setProperty("artifactComponentEnabled",
                                       state.scriptEnabled);
    scriptComponentButton->setText(QStringLiteral("Script"));
    applyInspectorComponentStateButton(scriptComponentButton,
                                       activeName == QStringLiteral("Script"));
    scriptComponentButton->setVisible(state.scriptEnabled);
    scriptComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Script component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Script."));
  }
  if (layoutComponentButton) {
    layoutComponentButton->setEnabled(canEditComponents);
    layoutComponentButton->setChecked(activeName == QStringLiteral("Layout"));
    layoutComponentButton->setProperty("artifactComponentEnabled",
                                       state.layoutEnabled);
    layoutComponentButton->setText(QStringLiteral("Layout"));
    applyInspectorComponentStateButton(layoutComponentButton,
                                       activeName == QStringLiteral("Layout"));
    layoutComponentButton->setVisible(state.layoutEnabled);
    layoutComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Layout component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Layout."));
  }
  if (cloneComponentButton) {
    cloneComponentButton->setEnabled(canEditComponents);
    cloneComponentButton->setChecked(activeName == QStringLiteral("Cloner"));
    cloneComponentButton->setProperty("artifactComponentEnabled",
                                      state.cloneEnabled);
    cloneComponentButton->setText(QStringLiteral("Cloner"));
    applyInspectorComponentStateButton(cloneComponentButton,
                                       activeName == QStringLiteral("Cloner"));
    cloneComponentButton->setVisible(state.cloneEnabled);
    cloneComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Cloner component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Cloner."));
  }
  if (fluidComponentButton) {
    fluidComponentButton->setEnabled(canEditComponents);
    fluidComponentButton->setChecked(activeName == QStringLiteral("Fluid"));
    fluidComponentButton->setProperty("artifactComponentEnabled",
                                      state.fluidEnabled);
    fluidComponentButton->setText(QStringLiteral("Fluid"));
    applyInspectorComponentStateButton(fluidComponentButton,
                                       activeName == QStringLiteral("Fluid"));
    fluidComponentButton->setVisible(state.fluidEnabled);
    fluidComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Fluid component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Fluid."));
  }
  if (generatorComponentButton) {
    generatorComponentButton->setEnabled(canEditComponents);
    generatorComponentButton->setText(
        state.generatorCount > 1 ? QStringLiteral("+ Generator (%1)").arg(state.generatorCount)
                            : QStringLiteral("+ Generator"));
    generatorComponentButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Add an extra generator to this layer.")
            : QStringLiteral("Select a layer inside a composition to add Generators."));
  }
  if (fieldComponentButton) {
    fieldComponentButton->setEnabled(canEditComponents);
    fieldComponentButton->setText(
        state.fieldCount > 0 ? QStringLiteral("+ Field (%1)").arg(state.fieldCount)
                        : QStringLiteral("+ Field"));
    fieldComponentButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Add a field to this layer.")
            : QStringLiteral("Select a layer inside a composition to add Fields."));
  }
  if (cloneModifierButton) {
    cloneModifierButton->setEnabled(canEditComponents);
    cloneModifierButton->setText(
        state.extraCloneModifierCount > 0
            ? QStringLiteral("+ Clone Mod (%1)").arg(state.extraCloneModifierCount)
            : QStringLiteral("+ Clone Mod"));
    cloneModifierButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Add a clone modifier to this layer.")
            : QStringLiteral("Select a layer inside a composition to add Clone Modifiers."));
  }
  if (removeGeneratorComponentButton) {
    const bool hasExtraGenerators = state.generatorCount > 1;
    removeGeneratorComponentButton->setEnabled(
        canEditComponents && hasExtraGenerators);
    removeGeneratorComponentButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Remove the selected extra generator.")
            : QStringLiteral("Select a layer inside a composition to remove Generators."));
  }
  if (generatorMoveUpButton) {
    generatorMoveUpButton->setEnabled(false);
  }
  if (generatorMoveDownButton) {
    generatorMoveDownButton->setEnabled(false);
  }
  if (removeFieldComponentButton) {
    removeFieldComponentButton->setEnabled(false);
    removeFieldComponentButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Remove the selected field.")
            : QStringLiteral("Select a layer inside a composition to remove Fields."));
  }
  if (fieldMoveUpButton) {
    fieldMoveUpButton->setEnabled(false);
  }
  if (fieldMoveDownButton) {
    fieldMoveDownButton->setEnabled(false);
  }
  if (removeCloneModifierButton) {
    removeCloneModifierButton->setEnabled(false);
    removeCloneModifierButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Remove the selected extra clone modifier.")
            : QStringLiteral("Select a layer inside a composition to remove Clone Modifiers."));
  }
  if (cloneModifierMoveUpButton) {
    cloneModifierMoveUpButton->setEnabled(false);
  }
  if (cloneModifierMoveDownButton) {
    cloneModifierMoveDownButton->setEnabled(false);
  }
  if (generatorListWidget) {
    SelectionActionBlocker blocker(generatorListWidget);
    const QString selectedGeneratorId =
        generatorListWidget->currentItem()
            ? generatorListWidget->currentItem()->data(Qt::UserRole).toString()
            : QString();
    generatorListWidget->clear();
    if (hasLayer) {
      const auto generators = layer->layerGenerators();
      int restoredRow = -1;
      int fallbackRow = -1;
      int extraGeneratorIndex = 0;
      for (int row = 0; row < static_cast<int>(generators.size()); ++row) {
        const auto& generator = generators[static_cast<std::size_t>(row)];
        auto* item = new QListWidgetItem(
            componentTypeDisplayName(generator.typeId),
            generatorListWidget);
        item->setData(Qt::UserRole, generator.generatorId);
        item->setToolTip(
            QStringLiteral("%1\n%2").arg(generator.generatorId, generator.typeId));
        if (generator.generatorId ==
            QStringLiteral("generator.compat.cloner.0")) {
          item->setData(Qt::UserRole + 1, QStringLiteral("component.cloner."));
          item->setData(Qt::UserRole + 2, false);
          item->setToolTip(QStringLiteral("Compatibility generator from the legacy single cloner."));
        } else {
          item->setData(
              Qt::UserRole + 1,
              QStringLiteral("component.generators.%1.")
                  .arg(extraGeneratorIndex));
          item->setData(Qt::UserRole + 2, true);
          ++extraGeneratorIndex;
          fallbackRow = row;
        }
        if (!selectedGeneratorId.isEmpty() &&
            generator.generatorId == selectedGeneratorId) {
          restoredRow = row;
        }
      }
      if (generatorListWidget->count() > 0) {
        if (restoredRow >= 0) {
          generatorListWidget->setCurrentRow(restoredRow);
        } else if (fallbackRow >= 0) {
          generatorListWidget->setCurrentRow(fallbackRow);
        } else {
          generatorListWidget->setCurrentRow(0);
        }
      }
    }
    generatorListWidget->setVisible(generatorListWidget->count() > 0);
    const auto* currentItem = generatorListWidget->currentItem();
    const bool currentIsExtra =
        canEditComponents && generatorItemSupportsReorder(currentItem);
    if (removeGeneratorComponentButton) {
      removeGeneratorComponentButton->setEnabled(currentIsExtra);
    }
    if (generatorMoveUpButton) {
      generatorMoveUpButton->setEnabled(
          currentIsExtra && generatorListWidget->currentRow() > 1);
    }
    if (generatorMoveDownButton) {
      generatorMoveDownButton->setEnabled(
          currentIsExtra &&
          generatorListWidget->currentRow() >= 1 &&
          generatorListWidget->currentRow() < generatorListWidget->count() - 1);
    }
  }
  if (fieldListWidget) {
    SelectionActionBlocker blocker(fieldListWidget);
    const QString selectedFieldId =
        fieldListWidget->currentItem()
            ? fieldListWidget->currentItem()->data(Qt::UserRole).toString()
            : QString();
    fieldListWidget->clear();
    if (hasLayer) {
      const auto fields = layer->layerFields();
      int restoredRow = -1;
      for (int row = 0; row < static_cast<int>(fields.size()); ++row) {
        const auto& field = fields[static_cast<std::size_t>(row)];
        auto* item = new QListWidgetItem(
            componentTypeDisplayName(field.typeId),
            fieldListWidget);
        item->setData(Qt::UserRole, field.fieldId);
        item->setToolTip(
            QStringLiteral("%1\n%2").arg(field.fieldId, field.typeId));
        item->setData(
            Qt::UserRole + 1,
            QStringLiteral("component.fields.%1.").arg(row));
        if (!selectedFieldId.isEmpty() && field.fieldId == selectedFieldId) {
          restoredRow = row;
        }
      }
      if (fieldListWidget->count() > 0) {
        fieldListWidget->setCurrentRow(restoredRow >= 0 ? restoredRow
                                                        : fieldListWidget->count() - 1);
      }
    }
    fieldListWidget->setVisible(fieldListWidget->count() > 0);
    const bool hasSelectedField = canEditComponents && fieldListWidget->currentItem();
    if (removeFieldComponentButton) {
      removeFieldComponentButton->setEnabled(hasSelectedField);
    }
    if (fieldMoveUpButton) {
      fieldMoveUpButton->setEnabled(
          hasSelectedField && fieldListWidget->currentRow() > 0);
    }
    if (fieldMoveDownButton) {
      fieldMoveDownButton->setEnabled(
          hasSelectedField &&
          fieldListWidget->currentRow() >= 0 &&
          fieldListWidget->currentRow() < fieldListWidget->count() - 1);
    }
  }
  if (cloneModifierListWidget) {
    SelectionActionBlocker blocker(cloneModifierListWidget);
    const QString selectedModifierId =
        cloneModifierListWidget->currentItem()
            ? cloneModifierListWidget->currentItem()->data(Qt::UserRole).toString()
            : QString();
    cloneModifierListWidget->clear();
    if (hasLayer) {
      const auto modifiers = layer->layerCloneModifiers();
      int restoredRow = -1;
      int fallbackRow = -1;
      int extraModifierIndex = 0;
      for (int row = 0; row < static_cast<int>(modifiers.size()); ++row) {
        const auto& modifier = modifiers[static_cast<std::size_t>(row)];
        auto* item = new QListWidgetItem(
            componentTypeDisplayName(modifier.typeId),
            cloneModifierListWidget);
        item->setData(Qt::UserRole, modifier.modifierId);
        item->setToolTip(
            QStringLiteral("%1\n%2").arg(modifier.modifierId, modifier.typeId));
        if (modifier.modifierId.startsWith(QStringLiteral("modifier.compat."))) {
          if (modifier.typeId == QStringLiteral("artifact.modifier.time-offset")) {
            item->setData(
                Qt::UserRole + 1,
                QStringLiteral("component.cloner.modifiers.compat.timeOffset."));
          } else {
            item->setData(
                Qt::UserRole + 1,
                QStringLiteral("component.cloner.modifiers.compat.sequence."));
          }
          item->setData(Qt::UserRole + 2, false);
        } else {
          item->setData(
              Qt::UserRole + 1,
              QStringLiteral("component.cloneModifiers.%1.").arg(extraModifierIndex));
          item->setData(Qt::UserRole + 2, true);
          ++extraModifierIndex;
          fallbackRow = row;
        }
        if (!selectedModifierId.isEmpty() &&
            modifier.modifierId == selectedModifierId) {
          restoredRow = row;
        }
      }
      if (cloneModifierListWidget->count() > 0) {
        if (restoredRow >= 0) {
          cloneModifierListWidget->setCurrentRow(restoredRow);
        } else if (fallbackRow >= 0) {
          cloneModifierListWidget->setCurrentRow(fallbackRow);
        } else {
          cloneModifierListWidget->setCurrentRow(0);
        }
      }
    }
    cloneModifierListWidget->setVisible(
        cloneModifierListWidget->count() > 0);
    const auto* currentItem = cloneModifierListWidget->currentItem();
    const bool currentIsExtra =
        canEditComponents && currentItem &&
        currentItem->data(Qt::UserRole + 2).toBool();
    if (removeCloneModifierButton) {
      removeCloneModifierButton->setEnabled(currentIsExtra);
    }
    if (cloneModifierMoveUpButton) {
      cloneModifierMoveUpButton->setEnabled(
          currentIsExtra && cloneModifierListWidget->currentRow() > 2);
    }
    if (cloneModifierMoveDownButton) {
      cloneModifierMoveDownButton->setEnabled(
          currentIsExtra &&
          cloneModifierListWidget->currentRow() >= 2 &&
          cloneModifierListWidget->currentRow() < cloneModifierListWidget->count() - 1);
    }
  }
  if (componentsSummaryLabel) {
    QString summaryText = layerComponentSummaryText(state);
    if (hasLayer && !state.validationIssues.empty()) {
      summaryText += QStringLiteral(" | issues: %1")
                         .arg(static_cast<int>(state.validationIssues.size()));
    }
    componentsSummaryLabel->setText(summaryText);
    componentsSummaryLabel->setVisible(
        !hasLayer || !state.validationIssues.empty());
    const bool mutedSummary = !hasLayer || (summaryText == QStringLiteral("Components: none"));
    applyInspectorLabelPalette(componentsSummaryLabel, !mutedSummary);
    if (hasLayer && !state.validationIssues.empty()) {
      QStringList issueLines;
      issueLines.reserve(static_cast<int>(std::min<std::size_t>(
          state.validationIssues.size(), static_cast<std::size_t>(4))));
      for (const auto &issue : state.validationIssues) {
        if (issueLines.size() >= 4) {
          break;
        }
        const QString componentLabel = issue.componentId.trimmed().isEmpty()
            ? QStringLiteral("(unnamed component)")
            : issue.componentId;
        issueLines.push_back(QStringLiteral("%1: %2")
                                 .arg(componentLabel, issue.message));
      }
      componentsSummaryLabel->setToolTip(issueLines.join(QStringLiteral("\n")));
    } else if (hasLayer &&
               (state.generatorCount > 0 || state.fieldCount > 0 || state.cloneModifierCount > 0)) {
      QStringList generatorLines;
      const auto generators = layer->layerGenerators();
      for (const auto& generator : generators) {
        generatorLines.push_back(
            QStringLiteral("%1: %2")
                .arg(generator.generatorId, generator.typeId));
      }
      const auto fields = layer->layerFields();
      for (const auto& field : fields) {
        generatorLines.push_back(
            QStringLiteral("%1: %2").arg(field.fieldId, field.typeId));
      }
      const auto modifiers = layer->layerCloneModifiers();
      for (const auto& modifier : modifiers) {
        generatorLines.push_back(
            QStringLiteral("%1: %2")
                .arg(modifier.modifierId, modifier.typeId));
      }
      componentsSummaryLabel->setToolTip(generatorLines.join(QStringLiteral("\n")));
    } else {
      componentsSummaryLabel->setToolTip({});
    }
  }

  QString desiredComponentFilter = componentInspectorFilter(activeName);
  if (activeName == QStringLiteral("Generator")) {
    desiredComponentFilter = generatorItemFilterText(
        generatorListWidget ? generatorListWidget->currentItem() : nullptr);
  } else if (activeName == QStringLiteral("Field")) {
    desiredComponentFilter = fieldItemFilterText(
        fieldListWidget ? fieldListWidget->currentItem() : nullptr);
  } else if (activeName == QStringLiteral("Clone Modifier")) {
    desiredComponentFilter = cloneModifierItemFilterText(
        cloneModifierListWidget ? cloneModifierListWidget->currentItem()
                                : nullptr);
  }
  syncComponentPropertyWidget(
      hasLayer && !activeName.isEmpty() ? layer : ArtifactAbstractLayerPtr{},
      desiredComponentFilter);

  if (openScriptButton) {
    const QString scriptPath = resolveScriptBindingPath(layer);
    const bool canOpen = hasLayer && !scriptPath.trimmed().isEmpty();
    openScriptButton->setEnabled(canOpen);
    openScriptButton->setVisible(
        canOpen && activeName == QStringLiteral("Script"));
    openScriptButton->setText(canOpen ? QStringLiteral("Open Script")
                                      : QStringLiteral("Open Script"));
    openScriptButton->setToolTip(
        canOpen ? QStringLiteral("Open the script file linked to this layer.")
                : (hasLayer ? QStringLiteral("No script file is linked to this layer yet.")
                            : QStringLiteral("Select a layer inside a composition to open its script.")));
  }

  if (applyLipSyncButton) {
    const auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer);
    const bool canShow = static_cast<bool>(audioLayer);
    bool canApply = false;
    std::shared_ptr<ArtifactSwitchLayer> switchTarget;
    if (audioLayer) {
      auto *projectService = ArtifactProjectService::instance();
      auto *selMgr = ArtifactLayerSelectionManager::instance();
      const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
      for (const auto &selectedLayer : selected) {
        if (!selectedLayer || selectedLayer == layer) {
          continue;
        }
        switchTarget = std::dynamic_pointer_cast<ArtifactSwitchLayer>(selectedLayer);
        if (switchTarget) {
          break;
        }
      }
      auto currentComposition = projectService ? projectService->currentComposition().lock()
                                               : ArtifactCompositionPtr{};
      if (!switchTarget && currentComposition) {
        for (const auto &candidate : currentComposition->allLayer()) {
          if (!candidate || candidate == layer) {
            continue;
          }
          switchTarget = std::dynamic_pointer_cast<ArtifactSwitchLayer>(candidate);
          if (switchTarget) {
            break;
          }
        }
      }
      canApply = static_cast<bool>(switchTarget);
    }
    applyLipSyncButton->setVisible(canShow);
    applyLipSyncButton->setEnabled(canApply);
    applyLipSyncButton->setToolTip(
        canApply ? QStringLiteral("Build a lip sync track from this audio layer and apply it to the selected Switch Layer.")
                 : (canShow ? QStringLiteral("Select a Switch Layer in the same composition to enable Lip Sync.")
                             : QStringLiteral("Select an audio layer to enable Lip Sync.")));
  }
  if (componentUtilitiesLabel) {
    const bool hasVisibleUtility =
        (openScriptButton && !openScriptButton->isHidden()) ||
        (applyLipSyncButton && !applyLipSyncButton->isHidden());
    componentUtilitiesLabel->setVisible(hasVisibleUtility);
  }
}

void ArtifactInspectorWidget::Impl::syncComponentPropertyWidget(
    const ArtifactAbstractLayerPtr &layer, const QString &filterText) {
  if (!componentPropertyWidget) {
    return;
  }
  if (!layer) {
    lastComponentPropertyStateSignature_.clear();
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
    return;
  }

  const QString normalizedFilter = filterText.trimmed().isEmpty()
      ? defaultComponentInspectorFilter(layer)
      : filterText;
  auto *selMgr = ArtifactLayerSelectionManager::instance();
  const auto selected =
      selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
  QStringList selectedLayerIds;
  selectedLayerIds.reserve(selected.size());
  for (const auto &selectedLayer : selected) {
    if (selectedLayer) {
      selectedLayerIds.push_back(selectedLayer->id().toString());
    }
  }
  std::sort(selectedLayerIds.begin(), selectedLayerIds.end());
  const QString stateSignature = QStringLiteral("%1|%2|%3")
      .arg(layer->id().toString(), selectedLayerIds.join(QLatin1Char(',')),
           normalizedFilter);
  componentPropertyWidget->setVisible(true);
  if (stateSignature == lastComponentPropertyStateSignature_) {
    return;
  }
  lastComponentPropertyStateSignature_ = stateSignature;
  if (selected.size() > 1) {
    componentPropertyWidget->setLayers(selected);
  } else {
    componentPropertyWidget->setLayer(layer);
  }
  componentPropertyWidget->setFilterText(normalizedFilter);
}

void ArtifactInspectorWidget::Impl::focusComponentProperties(
    const ArtifactAbstractLayerPtr &layer, const QString &filterText) {
  syncComponentPropertyWidget(layer, filterText);
}

QString ArtifactInspectorWidget::Impl::computeLayerInfoSignature(
    const ArtifactAbstractLayerPtr &layer) const {
  if (!layer) {
    return QStringLiteral("<no-layer>");
  }

  QString signature;
  signature.reserve(256);
  signature += currentCompositionId_.toString();
  signature += QLatin1Char('|');
  signature += layer->id().toString();
  signature += QLatin1Char('|');
  signature += layer->layerName();
  signature += QLatin1Char('|');
  signature += describeLayerPresentation(layer).typeText;
  signature += QLatin1Char('|');
  signature += QString::number(layer->maskCount());
  signature += QLatin1Char('|');
  signature += layerBooleanProperty(layer, QStringLiteral("physics.enabled"))
                   ? QLatin1Char('1')
                   : QLatin1Char('0');
  signature +=
      layerBooleanProperty(layer, QStringLiteral("component.script.enabled"))
          ? QLatin1Char('1')
          : QLatin1Char('0');
  signature +=
      layerBooleanProperty(layer, QStringLiteral("component.cloner.enabled"))
          ? QLatin1Char('1')
          : QLatin1Char('0');
  signature += QLatin1Char('|');
  signature += layer->layerNote();
  signature += QLatin1Char('|');
  const auto mattes = layer->matteReferences();
  for (const auto &ref : mattes) {
    signature += ref.id.toString();
    signature += QLatin1Char(':');
    signature += ref.sourceLayerId.toString();
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.type));
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.blendMode));
    signature += QLatin1Char(':');
    signature += QString::number(static_cast<int>(ref.fitMode));
    signature += QLatin1Char(':');
    signature += ref.enabled ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char(':');
    signature += ref.invert ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char('|');
  }
  return signature;
}

QString ArtifactInspectorWidget::Impl::computeRackSignature(
    int rackIndex,
    const std::vector<ArtifactAbstractEffectPtr> &effects) const {
  QString signature;
  signature.reserve(512);
  signature += currentCompositionId_.toString();
  signature += QLatin1Char('|');
  signature += currentLayerId_.toString();
  signature += QLatin1Char('|');
  signature += QString::number(rackIndex);
  signature += QLatin1Char('|');
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    signature += effect->effectID().toQString();
    signature += QLatin1Char('|');
    signature += effect->displayName().toQString();
    signature += QLatin1Char('|');
    signature +=
        effect->isEnabled() ? QStringLiteral("1") : QStringLiteral("0");
    signature += QLatin1Char('|');
  }
  return signature;
}

ArtifactInspectorWidget::Impl::~Impl() {}

void ArtifactInspectorWidget::Impl::rebuildMenu() {}

void ArtifactInspectorWidget::Impl::defaultHandleKeyPressEvent(
    QKeyEvent *event) {}

void ArtifactInspectorWidget::Impl::showContextMenu() {
  showContextMenu(QCursor::pos());
}

void ArtifactInspectorWidget::Impl::showContextMenu(const QPoint &globalPos) {
  QMenu menu;
  menu.addAction("Refresh Inspector", [this]() {
    updateLayerInfo();
    updateEffectsList();
  });
  QAction *saveMaskAction = menu.addAction("Save Mask Preset...");
  QAction *loadMaskAction = menu.addAction("Load Mask Preset...");
  menu.addSeparator();
  menu.addAction("Show Layer Info Tab", [this]() {
    if (tabWidget)
      tabWidget->setCurrentIndex(0);
  });
  menu.addAction("Show Effects Tab", [this]() {
    if (tabWidget)
      tabWidget->setCurrentIndex(2);
  });
  menu.addSeparator();
  menu.addAction("Expand All Racks", [this]() {
    for (auto &rack : racks) {
      if (rack.listWidget)
        rack.listWidget->setMaximumHeight(10000);
    }
  });
  menu.addAction("Collapse All Racks", [this]() {
    for (auto &rack : racks) {
      if (rack.listWidget)
        rack.listWidget->setMaximumHeight(100);
    }
  });
  QObject::connect(saveMaskAction, &QAction::triggered, [this]() {
    if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
      return;
    }
    auto *projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return;
    }
    auto findResult = projectService->findComposition(currentCompositionId_);
    if (!findResult.success) {
      return;
    }
    auto comp = findResult.ptr.lock();
    if (!comp) {
      return;
    }
    auto layer = comp->layerById(currentLayerId_);
    if (!layer || !layer->hasMasks()) {
      QMessageBox::information(containerWidget, QStringLiteral("Mask Preset"),
                               QStringLiteral("保存するマスクがありません。"));
      return;
    }
    const QString filePath = QFileDialog::getSaveFileName(
        containerWidget, QStringLiteral("マスクプリセットを保存"), QString(),
        QStringLiteral("Mask Preset (*.mask.json *.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
      return;
    }
    QString resolvedPath = filePath;
    if (!resolvedPath.endsWith(QStringLiteral(".mask.json"), Qt::CaseInsensitive) &&
        !resolvedPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
      resolvedPath += QStringLiteral(".mask.json");
    }
    LayerMask mask;
    for (int i = 0; i < layer->maskCount(); ++i) {
      const LayerMask sourceMask = layer->mask(i);
      for (int p = 0; p < sourceMask.maskPathCount(); ++p) {
        mask.addMaskPath(sourceMask.maskPath(p));
      }
    }
    if (!ArtifactPresetManager::saveMaskPreset(mask, resolvedPath)) {
      QMessageBox::warning(containerWidget, QStringLiteral("Mask Preset"),
                           QStringLiteral("マスクプリセットを保存できませんでした。"));
    }
  });
  QObject::connect(loadMaskAction, &QAction::triggered, [this]() {
    if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
      return;
    }
    auto *projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return;
    }
    auto findResult = projectService->findComposition(currentCompositionId_);
    if (!findResult.success) {
      return;
    }
    auto comp = findResult.ptr.lock();
    if (!comp) {
      return;
    }
    auto layer = comp->layerById(currentLayerId_);
    if (!layer) {
      return;
    }
    const QString filePath = QFileDialog::getOpenFileName(
        containerWidget, QStringLiteral("マスクプリセットを適用"), QString(),
        QStringLiteral("Mask Preset (*.mask.json *.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
      return;
    }
    LayerMask mask;
    if (!ArtifactPresetManager::loadMaskPreset(mask, filePath)) {
      QMessageBox::warning(containerWidget, QStringLiteral("Mask Preset"),
                           QStringLiteral("マスクプリセットを読み込めませんでした。"));
      return;
    }
    const auto choice = QMessageBox::question(
        containerWidget, QStringLiteral("Mask Preset"),
        QStringLiteral("マスクを置換しますか？\n\n"
                       "Yes: 置換\nNo: 追加"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice == QMessageBox::Yes) {
      layer->clearMasks();
    }
    layer->addMask(mask);
    layer->changed();
  });
  menu.exec(globalPos);
}

void ArtifactInspectorWidget::Impl::showRackContextMenu(
    int rackIndex, QListWidgetItem *item, const QPoint &globalPos) {
  QMenu menu;

  if (!item) {
    menu.addSeparator();
    menu.addAction("Refresh Inspector", [this]() {
      updateLayerInfo();
      updateEffectsList();
    });
    menu.exec(globalPos);
    return;
  }

  const QString effectId = item->data(Qt::UserRole).toString();
  if (effectId.isEmpty()) {
    menu.exec(globalPos);
    return;
  }

  bool isEnabled = false;
  bool found = false;
  if (const auto effect = currentEffectById(effectId)) {
    isEnabled = effect->isEnabled();
    found = true;
  }

  if (found) {
    QAction *toggleAction =
        menu.addAction(isEnabled ? "Disable Effect" : "Enable Effect");
    QObject::connect(toggleAction, &QAction::triggered,
                     [this, effectId, isEnabled]() {
                       if (setEffectEnabledById(effectId, !isEnabled)) {
                         updateEffectRackItemEnabled(effectId, !isEnabled);
                         if (statusLabel) {
                           statusLabel->setText(
                               QStringLiteral("Status: Effect %1")
                                   .arg(!isEnabled ? "enabled" : "disabled"));
                         }
                      }
      });
  }

  const auto effect = currentEffectById(effectId);

  QString layerMaskActionLabel = QStringLiteral("Use Current Layer Mask(s) as Effect Mask...");
  if (!currentCompositionId_.isNil() && !currentLayerId_.isNil()) {
    if (auto *projectService = ArtifactProjectService::instance()) {
      auto findResult = projectService->findComposition(currentCompositionId_);
      if (findResult.success) {
        if (auto comp = findResult.ptr.lock()) {
          if (auto layer = comp->layerById(currentLayerId_)) {
            layerMaskActionLabel =
                QStringLiteral("Use \"%1\" Mask(s) as Effect Mask...")
                    .arg(layer->layerName());
          }
        }
      }
    }
  }

  auto buildEffectMaskImageFromLayer =
      [](const ArtifactAbstractLayerPtr &sourceLayer)
          -> std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> {
    if (!sourceLayer || !sourceLayer->hasMasks()) {
      return {};
    }

    const auto sourceSize = sourceLayer->sourceSize();
    const int maskW = std::max(1, sourceSize.width);
    const int maskH = std::max(1, sourceSize.height);
    if (maskW <= 0 || maskH <= 0) {
      return {};
    }

    cv::Mat maskMat(maskH, maskW, CV_32FC4,
                    cv::Scalar(1.0f, 1.0f, 1.0f, 1.0f));
    for (int i = 0; i < sourceLayer->maskCount(); ++i) {
      const LayerMask sourceMask = sourceLayer->mask(i);
      if (!sourceMask.isEnabled()) {
        continue;
      }
      sourceMask.applyToImage(maskW, maskH, &maskMat);
    }

    auto maskImage = std::make_shared<ArtifactCore::ImageF32x4_RGBA>();
    maskImage->setFromRGBA32F(maskMat.ptr<float>(), maskW, maskH);
    return maskImage;
  };

  auto captureEffectMaskImages =
      [](const std::shared_ptr<ArtifactAbstractEffect> &effect) {
        std::vector<std::shared_ptr<ArtifactCore::ImageF32x4_RGBA>> masks;
        if (!effect) {
          return masks;
        }
        masks.reserve(static_cast<std::size_t>(std::max(0, effect->effectMaskImageCount())));
        for (int i = 0; i < effect->effectMaskImageCount(); ++i) {
          masks.push_back(effect->effectMaskImage(i));
        }
        return masks;
      };

  QAction *pickLayerMaskAction =
      menu.addAction("Pick Layer Mask Source...");
  QObject::connect(pickLayerMaskAction, &QAction::triggered,
                   [this, effectId, buildEffectMaskImageFromLayer,
                    captureEffectMaskImages]() {
    auto effect = currentEffectById(effectId);
    if (!effect) {
      QMessageBox::information(containerWidget, QStringLiteral("Effect Mask"),
                               QStringLiteral("適用先のエフェクトが見つかりません。"));
      return;
    }

    auto *projectService = ArtifactProjectService::instance();
    if (!projectService || currentCompositionId_.isNil()) {
      return;
    }
    auto findResult = projectService->findComposition(currentCompositionId_);
    if (!findResult.success) {
      return;
    }
    auto comp = findResult.ptr.lock();
    if (!comp) {
      return;
    }

    ArtifactObjectPickerDialog picker(containerWidget);
    picker.setReferenceType(QStringLiteral("Layer"));
    if (!currentLayerId_.isNil()) {
      picker.setCurrentSelectionId(currentLayerId_);
    }
    if (picker.exec() != QDialog::Accepted) {
      return;
    }

    const auto selectedLayer = comp->layerById(LayerID(picker.selectedId()));
    if (!selectedLayer || !selectedLayer->hasMasks()) {
      QMessageBox::information(containerWidget, QStringLiteral("Effect Mask"),
                               QStringLiteral("選択したレイヤーにマスクがありません。"));
      return;
    }

    auto maskImage = buildEffectMaskImageFromLayer(selectedLayer);
    if (!maskImage) {
      QMessageBox::warning(containerWidget, QStringLiteral("Effect Mask"),
                           QStringLiteral("レイヤーのマスク画像を生成できませんでした。"));
      return;
    }

    const auto beforeMasks = captureEffectMaskImages(effect);
    auto afterMasks = beforeMasks;
    afterMasks.clear();
    afterMasks.push_back(maskImage);
    if (auto *mgr = UndoManager::instance()) {
      mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
          effect, beforeMasks, afterMasks, QStringLiteral("Apply Layer Mask To Effect")));
    } else {
      effect->clearEffectMaskImages();
      effect->addEffectMaskImage(maskImage);
    }
    updateEffectsList();
    if (statusLabel) {
      statusLabel->setText(
          QStringLiteral("Status: Layer mask source applied to effect"));
    }
  });

  QAction *moveUpAction = menu.addAction("Move Up");
  QObject::connect(moveUpAction, &QAction::triggered, [this, effectId]() {
    if (moveEffectById(effectId, -1)) {
      updateEffectsList();
      if (statusLabel) {
        statusLabel->setText(QStringLiteral("Status: Effect moved up"));
      }
    }
  });

  QAction *moveDownAction = menu.addAction("Move Down");
  QObject::connect(moveDownAction, &QAction::triggered, [this, effectId]() {
    if (moveEffectById(effectId, 1)) {
      updateEffectsList();
      if (statusLabel) {
        statusLabel->setText(QStringLiteral("Status: Effect moved down"));
      }
    }
  });

  QAction *removeAction = menu.addAction("Remove Effect");
  QObject::connect(removeAction, &QAction::triggered, [this, effectId]() {
    if (removeEffectById(effectId)) {
      updateEffectsList();
    }
  });

  QAction *clearMaskAction = nullptr;
  if (found && effect && effect->effectMaskImageCount() > 0) {
    clearMaskAction = menu.addAction("Clear Effect Mask Images");
    QObject::connect(clearMaskAction, &QAction::triggered,
                     [this, effectId, captureEffectMaskImages]() {
      auto effect = currentEffectById(effectId);
      if (!effect) {
        QMessageBox::information(containerWidget, QStringLiteral("Effect Mask"),
                                 QStringLiteral("適用先のエフェクトが見つかりません。"));
        return;
      }

      const auto beforeMasks = captureEffectMaskImages(effect);
      if (beforeMasks.empty()) {
        return;
      }
      const std::vector<std::shared_ptr<ArtifactCore::ImageF32x4_RGBA>> afterMasks;
      if (auto *mgr = UndoManager::instance()) {
        mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
            effect, beforeMasks, afterMasks, QStringLiteral("Clear Effect Mask Images")));
      } else {
        effect->clearEffectMaskImages();
      }
      updateEffectsList();
      if (statusLabel) {
        statusLabel->setText(QStringLiteral("Status: Effect mask images cleared"));
      }
    });
  }

  QAction *applyLayerMaskAction = menu.addAction(layerMaskActionLabel);
  QObject::connect(applyLayerMaskAction, &QAction::triggered,
                   [this, effectId, buildEffectMaskImageFromLayer,
                    captureEffectMaskImages]() {
                     auto effect = currentEffectById(effectId);
                     if (!effect) {
                       QMessageBox::information(
                           containerWidget, QStringLiteral("Effect Mask"),
                           QStringLiteral("適用先のエフェクトが見つかりません。"));
                       return;
                     }

                     if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
                       QMessageBox::information(
                           containerWidget, QStringLiteral("Effect Mask"),
                           QStringLiteral("マスク元のレイヤーが選択されていません。"));
                       return;
                     }

                     auto *projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(currentLayerId_);
                     if (!layer || !layer->hasMasks()) {
                       QMessageBox::information(
                           containerWidget, QStringLiteral("Effect Mask"),
                           QStringLiteral("適用するレイヤーマスクがありません。"));
                       return;
                     }

                     auto maskImage = buildEffectMaskImageFromLayer(layer);
                     if (!maskImage) {
                       QMessageBox::warning(
                           containerWidget, QStringLiteral("Effect Mask"),
                           QStringLiteral("レイヤーのマスク画像を生成できませんでした。"));
                       return;
                     }

                     const auto applyMode = QMessageBox::question(
                         containerWidget, QStringLiteral("Effect Mask"),
                         QStringLiteral("現在の effect mask を置換しますか？\n\n"
                                        "Yes: 置換\nNo: 追加"),
                         QMessageBox::Yes | QMessageBox::No,
                         QMessageBox::Yes);
                     const auto beforeMasks = captureEffectMaskImages(effect);
                     auto afterMasks = beforeMasks;
                     if (applyMode == QMessageBox::Yes) {
                       afterMasks.clear();
                     }
                     afterMasks.push_back(maskImage);
                     if (auto *mgr = UndoManager::instance()) {
                       mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
                           effect, beforeMasks, afterMasks,
                           applyMode == QMessageBox::Yes
                               ? QStringLiteral("Replace Effect Mask Images")
                               : QStringLiteral("Append Effect Mask Image")));
                     } else {
                       if (applyMode == QMessageBox::Yes) {
                         effect->clearEffectMaskImages();
                       }
                       effect->addEffectMaskImage(maskImage);
                     }
                     updateEffectsList();
                     if (statusLabel) {
                       statusLabel->setText(applyMode == QMessageBox::Yes
                                                ? QStringLiteral("Status: Layer mask applied to effect (replaced)")
                                                : QStringLiteral("Status: Layer mask applied to effect (appended)"));
                     }
                   });

  QAction *savePresetAction = menu.addAction("Save Effect Preset...");
  QObject::connect(savePresetAction, &QAction::triggered,
                   [this, effectId]() {
                     auto effect = currentEffectById(effectId);
                     if (!effect) {
                       QMessageBox::information(
                           containerWidget, QStringLiteral("Effect Preset"),
                           QStringLiteral("保存するエフェクトが見つかりません。"));
                       return;
                     }

                     const QString filePath = QFileDialog::getSaveFileName(
                         containerWidget,
                         QStringLiteral("エフェクトプリセットを保存"),
                         QString(),
                         QStringLiteral("Effect Preset (*.effect.json *.json);;All Files (*.*)"));
                     if (filePath.isEmpty()) {
                       return;
                     }

                     QString resolvedPath = filePath;
                     if (!resolvedPath.endsWith(QStringLiteral(".effect.json"),
                                                Qt::CaseInsensitive) &&
                         !resolvedPath.endsWith(QStringLiteral(".json"),
                                                Qt::CaseInsensitive)) {
                       resolvedPath += QStringLiteral(".effect.json");
                     }

                     if (!ArtifactPresetManager::saveEffectPreset(effect,
                                                                  resolvedPath)) {
                       QMessageBox::warning(
                           containerWidget, QStringLiteral("Effect Preset"),
                           QStringLiteral("エフェクトプリセットを保存できませんでした。"));
                     }
                   });

  QAction *loadPresetAction = menu.addAction("Load Effect Preset...");
  QObject::connect(loadPresetAction, &QAction::triggered,
                   [this, effectId]() {
                     auto effect = currentEffectById(effectId);
                     if (!effect) {
                       QMessageBox::information(
                           containerWidget, QStringLiteral("Effect Preset"),
                           QStringLiteral("適用先のエフェクトが見つかりません。"));
                       return;
                     }

                     const QString filePath = QFileDialog::getOpenFileName(
                         containerWidget,
                         QStringLiteral("エフェクトプリセットを適用"),
                         QString(),
                         QStringLiteral("Effect Preset (*.effect.json *.json);;All Files (*.*)"));
                     if (filePath.isEmpty()) {
                       return;
                     }

                     if (!ArtifactPresetManager::loadEffectPreset(effect,
                                                                   filePath)) {
                       QMessageBox::warning(
                           containerWidget, QStringLiteral("Effect Preset"),
                           QStringLiteral("エフェクトプリセットを読み込めませんでした。"));
                       return;
                     }

                     updateEffectsList();
                     if (statusLabel) {
                       statusLabel->setText(
                           QStringLiteral("Status: Effect preset applied"));
                     }
                   });

  menu.addSeparator();
  QAction *copyIdAction = menu.addAction("Copy Effect ID");
  QObject::connect(copyIdAction, &QAction::triggered, [effectId]() {
    if (auto *cb = QApplication::clipboard()) {
      cb->setText(effectId);
    }
  });

  menu.exec(globalPos);
}

bool ArtifactInspectorWidget::Impl::removeEffectById(const QString &effectId) {
  if (effectId.isEmpty() || currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;

  if (editingCompositionEffects()) {
    return projectService->removeEffectFromCurrentComposition(effectId);
  }

  std::shared_ptr<ArtifactAbstractEffect> capturedEffect;
  if (auto layer = comp->layerById(currentLayerId_)) {
    for (const auto &e : layer->getEffects()) {
      if (e && e->effectID().toQString() == effectId) {
        capturedEffect = e;
        break;
      }
    }
  }
  return projectService->removeEffectFromLayerWithUndo(
      currentLayerId_, effectId, capturedEffect);
}

bool ArtifactInspectorWidget::Impl::setEffectEnabledById(
    const QString &effectId, bool enabled) {
  if (effectId.isEmpty() || currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;

  if (editingCompositionEffects()) {
    return projectService->setEffectEnabledInCurrentComposition(effectId,
                                                                enabled);
  }

  bool wasEnabled = false;
  if (auto layer = comp->layerById(currentLayerId_)) {
    for (const auto &e : layer->getEffects()) {
      if (e && e->effectID().toQString() == effectId) {
        wasEnabled = e->isEnabled();
        break;
      }
    }
  }
  return projectService->setEffectEnabledWithUndo(
      currentLayerId_, effectId, enabled, wasEnabled);
}

bool ArtifactInspectorWidget::Impl::moveEffectById(const QString &effectId,
                                                   int direction) {
  if (effectId.isEmpty() || currentCompositionId_.isNil())
    return false;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return false;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return false;
  auto comp = findResult.ptr.lock();
  if (!comp)
    return false;

  if (editingCompositionEffects()) {
    return projectService->moveEffectInCurrentComposition(effectId, direction);
  }

  return projectService->moveEffectWithUndo(
      currentLayerId_, effectId, direction);
}

void ArtifactInspectorWidget::Impl::handleProjectCreated() {
  qDebug() << "[Inspector] Project created";
  const bool wasEnabled = containerWidget && containerWidget->isEnabled();
  containerWidget->setEnabled(true);
  if (wasEnabled) {
    scheduleRefresh(CompositionNoteDirty);
    return;
  }
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleProjectClosed() {
  qDebug() << "[Inspector] Project closed";
  setNoProjectState();
}

void ArtifactInspectorWidget::Impl::handleCompositionCreated(
    const CompositionID &id) {
  qDebug() << "[Inspector] Composition created:" << id.toString();
  currentCompositionId_ = id;
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleCompositionChanged(
    const CompositionID &id) {
  qDebug() << "[Inspector] Composition changed:" << id.toString();
  currentCompositionId_ = id;
  scheduleRefresh(CompositionNoteDirty | LayerNoteDirty | LayerInfoDirty |
                  EffectsDirty);
}

void ArtifactInspectorWidget::Impl::handleLayerSelected(
    const LayerSelectionChangedEvent &event) {
  const LayerID id(event.layerId);
  qDebug() << "[Inspector] Layer selected:" << id.toString()
           << "reason="
           << layerSelectionChangeReasonToString(event.reason);
  if (id.isNil()) {
    auto projectService = ArtifactProjectService::instance();
    if (projectService && !currentCompositionId_.isNil() &&
        !currentLayerId_.isNil()) {
      auto findResult = projectService->findComposition(currentCompositionId_);
      if (findResult.success) {
        auto comp = findResult.ptr.lock();
        if (comp && comp->containsLayerById(currentLayerId_)) {
          syncEffectPropertyWidget();
          scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
          return;
        }
      }
    }
    qDebug() << "[Inspector] NoLayer reason="
             << layerSelectionChangeReasonToString(event.reason)
             << "composition=" << currentCompositionId_.toString()
             << "layer=" << currentLayerId_.toString()
             << "projectService=" << static_cast<bool>(projectService);
    setNoLayerState();
    scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
    return;
  }
  currentLayerId_ = id;
  focusedEffectId_.clear();
  syncEffectPropertyWidget();
  scheduleRefresh(LayerNoteDirty | LayerInfoDirty | EffectsDirty);
}

void ArtifactInspectorWidget::Impl::updateCompositionNote() {
  auto disconnectNoteConnection = [this]() {
    if (compositionNoteConnection_) {
      QObject::disconnect(compositionNoteConnection_);
      compositionNoteConnection_ = {};
    }
    compositionNoteSubscription_.disconnect();
  };

  if (!compositionNoteEdit) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil()) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
      compositionNoteGroup->hide();
    }
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
      compositionNoteGroup->hide();
    }
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    disconnectNoteConnection();
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(false);
      compositionNoteGroup->hide();
    }
    return;
  }

  disconnectNoteConnection();
  compositionNoteSubscription_ =
      eventBus_.subscribe<CompositionNoteChangedEvent>([this](const CompositionNoteChangedEvent &event) {
        if (!compositionNoteEdit || event.compositionId != currentCompositionId_.toString()) {
          return;
        }
        QSignalBlocker blocker(compositionNoteEdit);
        compositionNoteEdit->setPlainText(event.note);
        compositionNoteEdit->setEnabled(true);
        if (compositionNoteGroup) {
          compositionNoteGroup->setEnabled(true);
          compositionNoteGroup->hide();
        }
      });

  const QString note = comp->compositionNote();
  if (note == lastCompositionNoteText_) {
    compositionNoteEdit->setEnabled(true);
    if (compositionNoteGroup) {
      compositionNoteGroup->setEnabled(true);
      compositionNoteGroup->hide();
    }
    return;
  }
  lastCompositionNoteText_ = note;
  {
    QSignalBlocker blocker(compositionNoteEdit);
    compositionNoteEdit->setPlainText(note);
    compositionNoteEdit->setEnabled(true);
  }
  if (compositionNoteGroup) {
    compositionNoteGroup->setEnabled(true);
    compositionNoteGroup->hide();
  }
}

void ArtifactInspectorWidget::Impl::updateLayerNote() {
  auto disconnectNoteConnection = [this]() {
    if (layerNoteConnection_) {
      QObject::disconnect(layerNoteConnection_);
      layerNoteConnection_ = {};
    }
  };

  if (!layerNoteEdit) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil() ||
      currentLayerId_.isNil()) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
      layerNoteGroup->hide();
    }
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
      layerNoteGroup->hide();
    }
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp || !comp->containsLayerById(currentLayerId_)) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
      layerNoteGroup->hide();
    }
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    disconnectNoteConnection();
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(false);
      layerNoteGroup->hide();
    }
    return;
  }

  disconnectNoteConnection();
  layerNoteConnection_ =
      QObject::connect(layer.get(), &ArtifactAbstractLayer::layerNoteChanged,
                       layerNoteEdit, [this](const QString &note) {
                         if (!layerNoteEdit) {
                           return;
                         }
                         QSignalBlocker blocker(layerNoteEdit);
                         layerNoteEdit->setPlainText(note);
                         layerNoteEdit->setEnabled(true);
                         if (layerNoteGroup) {
                           layerNoteGroup->setEnabled(true);
                           layerNoteGroup->hide();
                         }
                       });

  const QString note = layer->layerNote();
  if (note == lastLayerNoteText_) {
    layerNoteEdit->setEnabled(true);
    if (layerNoteGroup) {
      layerNoteGroup->setEnabled(true);
      layerNoteGroup->hide();
    }
    return;
  }
  lastLayerNoteText_ = note;
  {
    QSignalBlocker blocker(layerNoteEdit);
    layerNoteEdit->setPlainText(note);
    layerNoteEdit->setEnabled(true);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(true);
    layerNoteGroup->hide();
  }
}

void ArtifactInspectorWidget::Impl::updateLayerInfo() {
  if (currentLayerId_.isNil()) {
    setNoLayerState();
    return;
  }

  // レイヤー情報を取得
  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
    setNoProjectState();
    return;
  }

  // コンポジションを取得
  if (currentCompositionId_.isNil()) {
    // イベントで compositionId が届かなかった場合のフォールバック
    if (auto comp = projectService->currentComposition().lock()) {
      currentCompositionId_ = comp->id();
    } else {
      setNoLayerState();
      return;
    }
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    setNoLayerState();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    setNoLayerState();
    return;
  }

  // レイヤーを取得
  if (!comp->containsLayerById(currentLayerId_)) {
    setNoLayerState();
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  if (!layer) {
    setNoLayerState();
    return;
  }

  const QString nextSignature = computeLayerInfoSignature(layer);
  if (nextSignature == lastLayerInfoSignature_) {
    // matte 表示も同じ更新経路で同期するため、ここでは止めない
  }
  lastLayerInfoSignature_ = nextSignature;

  // レイヤー情報を表示
  QString layerName = layer->layerName();
  layerNameLabel->setText(
      QString("Layer: %1").arg(layerName.isEmpty() ? "(Unnamed)" : layerName));
  {
    const auto theme = ArtifactCore::currentDCCTheme();
    QFont nameFont = layerNameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(13);
    layerNameLabel->setFont(nameFont);
    applyInspectorLabelPalette(layerNameLabel, true);

    QFont typeFont = layerTypeLabel->font();
    typeFont.setBold(true);
    layerTypeLabel->setFont(typeFont);
    applyInspectorLabelPalette(layerTypeLabel, false);
  }

  // レイヤータイプを実態に寄せて表示する
  const auto presentation = describeLayerPresentation(layer);
  layerTypeLabel->setText(presentation.inspectorTypeLabel);

  const int maskCount = layer->maskCount();
  const QString maskText = maskCount > 0
                               ? QStringLiteral("Masks: %1").arg(maskCount)
                               : QStringLiteral("Masks: none");
  bool matteHasInvalid = false;
  const QString matteText = matteReferenceSummary(comp, layer, &matteHasInvalid);
  if (matteInfoLabel) {
    matteInfoLabel->setMatteContext(currentCompositionId_, layer, comp);
    if (matteText != lastMatteInfoSignature_) {
      lastMatteInfoSignature_ = matteText;
      matteInfoLabel->setText(matteText);
    }
    matteInfoLabel->setEnabled(true);
    QFont matteFont = matteInfoLabel->font();
    matteFont.setBold(matteHasInvalid);
    matteInfoLabel->setFont(matteFont);
    applyInspectorLabelPalette(matteInfoLabel, matteHasInvalid);
  }
  const QString proxyText = proxySummary(layer);
  if (proxyInfoLabel) {
    proxyInfoLabel->setProxyContext(layer);
    proxyInfoLabel->setText(proxyText);
    proxyInfoLabel->setEnabled(true);
    applyInspectorLabelPalette(proxyInfoLabel, proxyText.contains(QStringLiteral("none"), Qt::CaseInsensitive));
  }
  updateComponentControls(layer);
  const QString capabilityText = presentation.capabilitySummaryText.isEmpty()
                                     ? QString()
                                     : QStringLiteral(" | %1").arg(presentation.capabilitySummaryText);
  statusLabel->setText(QString("Status: Layer selected - ID: %1 | %2%3")
                           .arg(currentLayerId_.toString(), maskText, capabilityText));
  {
    const auto theme = ArtifactCore::currentDCCTheme();
    applyInspectorLabelPalette(statusLabel, true);
  }
      setEffectsStateText(
      maskCount > 0
          ? QStringLiteral("Mask editing is available for this layer. Roto fields are also available where supported.")
          : QStringLiteral(
                "No masks on this layer. Use the Mask tool to create one."),
      true);

  qDebug() << "[Inspector] Updated layer info:" << layerName
           << "Type:" << presentation.typeText;
}

void ArtifactInspectorWidget::Impl::setNoProjectState() {
  containerWidget->setEnabled(false);
  if (compositionNoteConnection_) {
    QObject::disconnect(compositionNoteConnection_);
    compositionNoteConnection_ = {};
  }
  if (layerNoteConnection_) {
    QObject::disconnect(layerNoteConnection_);
    layerNoteConnection_ = {};
  }
  if (compositionNoteEdit) {
    compositionNoteEdit->blockSignals(true);
    compositionNoteEdit->clear();
    compositionNoteEdit->setEnabled(false);
    compositionNoteEdit->blockSignals(false);
  }
  if (compositionNoteGroup) {
    compositionNoteGroup->setEnabled(false);
    compositionNoteGroup->hide();
  }
  if (layerNoteEdit) {
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(false);
    layerNoteGroup->hide();
  }
  layerNameLabel->setText("Layer: Open a project to inspect layers");
  layerTypeLabel->setText("Type: N/A");
  if (matteInfoLabel) {
    matteInfoLabel->setText("Matte: none");
    matteInfoLabel->setEnabled(false);
    matteInfoLabel->setMatteContext(CompositionID(), ArtifactAbstractLayerPtr{}, ArtifactCompositionPtr{});
  }
  if (proxyInfoLabel) {
    proxyInfoLabel->setText("Proxy: not available");
    proxyInfoLabel->setEnabled(false);
    proxyInfoLabel->setProxyContext(ArtifactAbstractLayerPtr{});
  }
  updateComponentControls(ArtifactAbstractLayerPtr{});
  statusLabel->setText("Status: Open a project to inspect layers");
  currentCompositionId_ = CompositionID();
  currentLayerId_ = LayerID();
  lastLayerInfoSignature_.clear();
  lastMatteInfoSignature_.clear();
  lastCompositionNoteText_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  lastSyncedLayer_.reset();
  lastSyncedFocusedEffectId_.clear();
  lastEffectPropertyStateSignature_.clear();
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (componentPropertyWidget) {
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Open a project, then select a composition, layer, and effect to edit parameters here."));
    effectParametersHintLabel->setVisible(true);
  }
  if (effectsTargetLabel) {
    effectsTargetLabel->setText(QStringLiteral("Target: No project open"));
  }
  if (effectsStackSummaryLabel) {
    effectsStackSummaryLabel->setText(
        QStringLiteral("The effect stack appears here once a valid target is selected."));
  }
  setEffectRackEnabled(false);
  setEffectsStateText("Open a project to manage effects.", true);
}

void ArtifactInspectorWidget::Impl::setNoLayerState() {
  layerNameLabel->setText("Layer: Select a layer to continue");
  layerTypeLabel->setText("Type: N/A");
  if (matteInfoLabel) {
    matteInfoLabel->setText("Matte: none");
    matteInfoLabel->setEnabled(false);
    matteInfoLabel->setMatteContext(CompositionID(), ArtifactAbstractLayerPtr{}, ArtifactCompositionPtr{});
  }
  if (proxyInfoLabel) {
    proxyInfoLabel->setText("Proxy: not available");
    proxyInfoLabel->setEnabled(false);
    proxyInfoLabel->setProxyContext(ArtifactAbstractLayerPtr{});
  }
  updateComponentControls(ArtifactAbstractLayerPtr{});
  statusLabel->setText("Status: Select a layer to inspect details");
  currentLayerId_ = LayerID();
  if (layerNoteConnection_) {
    QObject::disconnect(layerNoteConnection_);
    layerNoteConnection_ = {};
  }
  if (layerNoteEdit) {
    layerNoteEdit->blockSignals(true);
    layerNoteEdit->clear();
    layerNoteEdit->setEnabled(false);
    layerNoteEdit->blockSignals(false);
  }
  if (layerNoteGroup) {
    layerNoteGroup->setEnabled(false);
    layerNoteGroup->hide();
  }

  lastLayerInfoSignature_.clear();
  lastMatteInfoSignature_.clear();
  lastLayerNoteText_.clear();
  lastRackSignatures_.fill(QString());
  lastSyncedLayer_.reset();
  lastSyncedFocusedEffectId_.clear();
  lastEffectPropertyStateSignature_.clear();
  refreshMask_ = 0;
  refreshQueued_ = false;
  focusedEffectId_.clear();
  if (effectPropertyWidget) {
    effectPropertyWidget->clear();
    effectPropertyWidget->setVisible(false);
  }
  if (componentPropertyWidget) {
    componentPropertyWidget->clear();
    componentPropertyWidget->setVisible(false);
  }
  if (effectParametersHintLabel) {
    effectParametersHintLabel->setText(
        QStringLiteral("Select an effect on the left to edit parameters here."));
    effectParametersHintLabel->setVisible(true);
  }
  if (currentCompositionId_.isNil()) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    refreshRackButtons();
  } else {
    updateEffectsList();
  }
}

void ArtifactInspectorWidget::Impl::setEffectRackEnabled(bool enabled) {
  if (effectsQuickAddButton) {
    effectsQuickAddButton->setEnabled(enabled);
  }
  for (auto &rack : racks) {
    if (rack.listWidget) {
      rack.listWidget->setEnabled(enabled);
    }
    if (rack.addButton) {
      rack.addButton->setEnabled(enabled);
    }
    if (rack.removeButton) {
      rack.removeButton->setEnabled(false);
    }
    if (rack.moveUpButton) {
      rack.moveUpButton->setEnabled(false);
    }
    if (rack.moveDownButton) {
      rack.moveDownButton->setEnabled(false);
    }
  }
}

void ArtifactInspectorWidget::Impl::updateEffectRackVisibility() {
  const int visibleRackIndex = rasterizerRackIndex();
  const bool showAllCompositionRacks = editingCompositionEffects();
  for (int i = 0; i < kEffectRackCount; ++i) {
    if (racks[i].groupBox) {
      const bool visible = showAllCompositionRacks || i == visibleRackIndex;
      racks[i].groupBox->setVisible(visible);
      if (racks[i].addButton) {
        racks[i].addButton->setVisible(visible);
      }
    }
  }
}

void ArtifactInspectorWidget::Impl::refreshRackButtons() {
  const bool canEdit = !currentCompositionId_.isNil();
  if (effectsQuickAddButton) {
    effectsQuickAddButton->setEnabled(canEdit);
  }
  for (auto &rack : racks) {
    if (rack.addButton) {
      rack.addButton->setEnabled(canEdit);
    }
    if (!rack.removeButton || !rack.listWidget) {
      continue;
    }
    auto *current = rack.listWidget->currentItem();
    const bool hasEffectItem =
        canEdit && current &&
        current->data(Qt::UserRole).toString().trimmed().size() > 0;
    rack.removeButton->setEnabled(hasEffectItem);
    if (rack.moveUpButton) {
      rack.moveUpButton->setEnabled(hasEffectItem);
    }
    if (rack.moveDownButton) {
      rack.moveDownButton->setEnabled(hasEffectItem);
    }
  }
}

void ArtifactInspectorWidget::Impl::updateEffectsList() {
  updateEffectRackVisibility();
  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a project to manage effects.", true);
    if (effectsTargetLabel) {
      effectsTargetLabel->setText(QStringLiteral("Target: No project open"));
    }
    if (effectsStackSummaryLabel) {
      effectsStackSummaryLabel->setText(
          QStringLiteral("Choose a project and composition to browse the effect stack."));
    }
    refreshRackButtons();
    return;
  }

  if (currentCompositionId_.isNil()) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    if (effectsTargetLabel) {
      effectsTargetLabel->setText(
          QStringLiteral("Target: No composition selected"));
    }
    if (effectsStackSummaryLabel) {
      effectsStackSummaryLabel->setText(
          QStringLiteral("The stack appears once a composition is active."));
    }
    refreshRackButtons();
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    if (effectsTargetLabel) {
      effectsTargetLabel->setText(
          QStringLiteral("Target: Composition unavailable"));
    }
    refreshRackButtons();
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    setEffectRackEnabled(false);
    setEffectsStateText("Open a composition to manage effects.", true);
    if (effectsTargetLabel) {
      effectsTargetLabel->setText(
          QStringLiteral("Target: Composition unavailable"));
    }
    refreshRackButtons();
    return;
  }

  if (!editingCompositionEffects() && currentLayerId_.isNil()) {
    setEffectRackEnabled(false);
    setEffectsStateText("Select a layer to manage effects.", true);
    if (effectsTargetLabel) {
      effectsTargetLabel->setText(
          QStringLiteral("Target: Select a layer or switch to composition effects"));
    }
    if (effectsStackSummaryLabel) {
      effectsStackSummaryLabel->setText(
          QStringLiteral("Effects are organized by stage once a target is selected."));
    }
    refreshRackButtons();
    return;
  }

  if (effectsTargetLabel) {
    if (editingCompositionEffects()) {
      effectsTargetLabel->setText(
          QStringLiteral("Target: Composition \"%1\"")
              .arg(comp->settings().compositionName().toQString()));
    } else if (comp->layerById(currentLayerId_)) {
      effectsTargetLabel->setText(QStringLiteral("Target: Layer"));
    } else {
      effectsTargetLabel->setText(QStringLiteral("Target: Layer unavailable"));
    }
  }

  auto effects = currentEffectStack();
  setEffectRackEnabled(true);
  int effectCount = 0;
  int maskedEffectCount = 0;
  std::array<std::vector<ArtifactAbstractEffectPtr>, kEffectRackCount>
      rackEffects;

  for (const auto &effect : effects) {
    if (effect) {
      ++effectCount;
      if (effect->hasMask()) {
        ++maskedEffectCount;
      }
      const int rackIdx = rackIndexFromStage(effect->pipelineStage());
      if (rackIdx >= 0) {
        rackEffects[rackIdx].push_back(effect);
      }
    }
  }

  for (int i = 0; i < kEffectRackCount; ++i) {
    const QString rackSignature = computeRackSignature(i, rackEffects[i]);
    if (rackSignature == lastRackSignatures_[i]) {
      if (racks[i].groupBox) {
        racks[i].groupBox->setTitle(
            QStringLiteral("%1 (%2)")
                .arg(stageDisplayName(stageFromRackIndex(i)))
                .arg(static_cast<int>(rackEffects[i].size())));
      }
      continue;
    }
    lastRackSignatures_[i] = rackSignature;

    if (!racks[i].listWidget) {
      continue;
    }
    if (racks[i].groupBox) {
      racks[i].groupBox->setTitle(
          QStringLiteral("%1 (%2)")
              .arg(stageDisplayName(stageFromRackIndex(i)))
              .arg(static_cast<int>(rackEffects[i].size())));
    }
    const QSignalBlocker blocker(racks[i].listWidget);
    racks[i].listWidget->clear();
    if (rackEffects[i].empty()) {
      racks[i].listWidget->setMinimumHeight(32);
      racks[i].listWidget->setMaximumHeight(44);
      auto item = new QListWidgetItem("(No effects)");
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      racks[i].listWidget->addItem(item);
      continue;
    }
    racks[i].listWidget->setMinimumHeight(56);
    racks[i].listWidget->setMaximumHeight(180);
    for (const auto &effect : rackEffects[i]) {
      if (!effect) {
        continue;
      }
      QString effectName = effect->displayName().toQString();
      QString effectStatus = effect->isEnabled() ? QStringLiteral("Enabled")
                                                 : QStringLiteral("Disabled");
      const bool hasMask = effect->hasMask();
      const int effectMaskCount = effect->effectMaskImageCount();
      const QString maskSuffix = hasMask
                                     ? (effectMaskCount > 0
                                            ? QStringLiteral(" [Mask x%1]").arg(effectMaskCount)
                                            : QStringLiteral(" [Mask]"))
                                     : QString();
      QString itemText = QStringLiteral("%1 %2%3").arg(effectStatus, effectName, maskSuffix);
      auto *item = new QListWidgetItem(itemText);
      item->setData(Qt::UserRole, effect->effectID().toQString());
      item->setData(kEffectRackEnabledRole, effect->isEnabled());
      item->setData(kEffectRackHasMaskRole, hasMask);
      item->setData(kEffectRackNameRole, effectName);
      item->setData(kEffectRackMaskCountRole, effectMaskCount);
      item->setSizeHint(QSize(0, 34));
      item->setToolTip(
          QStringLiteral("%1 on this %2.%3%4 Single click to focus. Double click toggles enable/disable. Right click for effect actions.")
              .arg(effectName,
                   editingCompositionEffects() ? QStringLiteral("composition")
                                               : QStringLiteral("layer"),
                   hasMask ? QStringLiteral(" Mask attached.") : QString(),
                   effectMaskCount > 0
                       ? QStringLiteral(" Effect mask images: %1.").arg(effectMaskCount)
                       : QString()));
      racks[i].listWidget->addItem(item);
    }
  }

  if (effectCount == 0) {
    setEffectsStateText("No effects yet. Use + Add to create an effect.", true);
  } else if (focusedEffectId_.trimmed().isEmpty()) {
    setEffectsStateText("Select an effect to edit its parameters below.", true);
  } else {
    setEffectsStateText(QString(), false);
  }
  if (effectsStackSummaryLabel) {
    effectsStackSummaryLabel->setText(
        editingCompositionEffects()
            ? (effectCount > 0
                   ? QStringLiteral("%1 effect(s) across %2 stages on this composition, %3 with masks.")
                         .arg(effectCount)
                         .arg(kEffectRackCount)
                         .arg(maskedEffectCount)
                   : QStringLiteral("This composition has no effects yet."))
            : (effectCount > 0
                   ? QStringLiteral("%1 raster effect(s) on this layer, %2 with masks.")
                         .arg(effectCount)
                         .arg(maskedEffectCount)
                   : QStringLiteral("This layer has no raster effects yet.")));
  }
  if (!focusedEffectId_.trimmed().isEmpty()) {
    suppressRackSelectionSync_ = true;
    for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
      auto *list = racks[rackIndex].listWidget;
      if (!list) {
        continue;
      }
      for (int row = 0; row < list->count(); ++row) {
        auto *item = list->item(row);
        if (!item) {
          continue;
        }
        if (item->data(Qt::UserRole).toString().trimmed() == focusedEffectId_) {
          const QSignalBlocker blocker(list);
          list->setCurrentItem(item);
          break;
        }
      }
    }
    suppressRackSelectionSync_ = false;
  }

  // The focused item is restored after the lists are rebuilt. Refresh after
  // that restoration so Up/Down is immediately available for a newly added
  // effect instead of remaining disabled until the user reselects it.
  refreshRackButtons();
  syncEffectPropertyWidget();
}

void ArtifactInspectorWidget::Impl::updateEffectRackItemEnabled(
    const QString &effectId, const bool enabled) {
  const QString trimmedId = effectId.trimmed();
  if (trimmedId.isEmpty()) {
    return;
  }

  for (int rackIndex = 0; rackIndex < kEffectRackCount; ++rackIndex) {
    auto *list = racks[rackIndex].listWidget;
    if (!list) {
      continue;
    }
    for (int row = 0; row < list->count(); ++row) {
      auto *item = list->item(row);
      if (!item || item->data(Qt::UserRole).toString().trimmed() != trimmedId) {
        continue;
      }

      const QString effectName = item->data(kEffectRackNameRole).toString();
      const bool hasMask = item->data(kEffectRackHasMaskRole).toBool();
      item->setData(kEffectRackEnabledRole, enabled);
      item->setToolTip(
          QStringLiteral("%1 on this %2.%3 Single click to focus. Double click toggles enable/disable. Right click for effect actions.")
              .arg(effectName,
                   editingCompositionEffects() ? QStringLiteral("composition")
                                               : QStringLiteral("layer"),
                       hasMask ? QStringLiteral(" Mask attached.") : QString()));
      if (list->viewport()) {
        list->viewport()->update();
      }
      refreshRackButtons();
      return;
    }
  }
}

struct EffectTabState {
  bool hasProject = false;
  bool hasComposition = false;
  bool hasResolvedComposition = false;
  bool editingCompositionEffects = false;
  bool hasLayerTarget = false;
  bool hasLayerEffects = false;
  int effectCount = 0;
  QString targetText;
  QString stateText;
  QString stackSummaryText;
};

EffectTabState collectEffectTabState(
    ArtifactProjectService *projectService,
    const CompositionID &currentCompositionId,
    const LayerID &currentLayerId,
    const bool editingCompositionEffects,
    const QString &focusedEffectId) {
  EffectTabState state;
  state.hasProject = static_cast<bool>(projectService);
  state.hasComposition = !currentCompositionId.isNil();
  state.editingCompositionEffects = editingCompositionEffects;

  if (!projectService) {
    state.stateText = QStringLiteral("Open a project to manage effects.");
    state.targetText = QStringLiteral("Target: No project open");
    state.stackSummaryText =
        QStringLiteral("Choose a project and composition to browse the effect stack.");
    return state;
  }

  if (currentCompositionId.isNil()) {
    state.stateText = QStringLiteral("Open a composition to manage effects.");
    state.targetText = QStringLiteral("Target: No composition selected");
    state.stackSummaryText =
        QStringLiteral("The stack appears once a composition is active.");
    return state;
  }

  auto findResult = projectService->findComposition(currentCompositionId);
  if (!findResult.success) {
    state.stateText = QStringLiteral("Open a composition to manage effects.");
    state.targetText = QStringLiteral("Target: Composition unavailable");
    return state;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    state.stateText = QStringLiteral("Open a composition to manage effects.");
    state.targetText = QStringLiteral("Target: Composition unavailable");
    return state;
  }

  state.hasResolvedComposition = true;
  if (editingCompositionEffects) {
    state.hasLayerTarget = true;
    const auto effects = comp->getEffects();
    state.effectCount = static_cast<int>(effects.size());
    state.targetText = QStringLiteral("Target: Composition \"%1\"")
                          .arg(comp->settings().compositionName().toQString());
    state.stackSummaryText =
        state.effectCount > 0
            ? QStringLiteral("%1 effect(s) across %2 pipeline stages. Add into the rack that matches where the effect should run.")
                  .arg(state.effectCount)
                  .arg(kEffectRackCount)
            : QStringLiteral("The stack is empty. Start by adding an effect into the stage where it belongs.");
    state.stateText = focusedEffectId.trimmed().isEmpty()
        ? QStringLiteral("Select a composition effect to edit its parameters below.")
        : QString();
    return state;
  }

  if (currentLayerId.isNil()) {
    state.stateText = QStringLiteral("Select a layer to manage effects.");
    state.targetText =
        QStringLiteral("Target: Select a layer or switch to composition effects");
    state.stackSummaryText =
        QStringLiteral("Effects are organized by stage once a target is selected.");
    return state;
  }

  auto layer = comp->layerById(currentLayerId);
  if (!layer) {
    state.stateText = QStringLiteral("Select a layer to manage effects.");
    state.targetText = QStringLiteral("Target: Layer unavailable");
    return state;
  }

  state.hasLayerTarget = true;
  const auto effects = layer->getEffects();
  state.hasLayerEffects = !effects.empty();
  state.effectCount = static_cast<int>(effects.size());
  state.targetText = QStringLiteral("Target: Layer \"%1\"").arg(layer->layerName());
  state.stackSummaryText =
      state.effectCount > 0
          ? QStringLiteral("%1 effect(s) across %2 pipeline stages. Add into the rack that matches where the effect should run.")
                .arg(state.effectCount)
                .arg(kEffectRackCount)
          : QStringLiteral("The stack is empty. Start by adding an effect into the stage where it belongs.");
  state.stateText = focusedEffectId.trimmed().isEmpty()
      ? QStringLiteral("Select an effect to edit its parameters below.")
      : QString();
  return state;
}

void ArtifactInspectorWidget::Impl::addSelectedEffectToCurrentTarget(
    const QString &effectId) {
  const QString normalizedId = effectId.trimmed();
  if (normalizedId.isEmpty() || currentCompositionId_.isNil()) {
    return;
  }

  auto *projectService = ArtifactProjectService::instance();
  auto *effectService = ArtifactEffectService::instance();
  if (!projectService || !effectService) {
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    return;
  }
  if (!editingCompositionEffects() && !comp->layerById(currentLayerId_)) {
    return;
  }

  std::shared_ptr<ArtifactAbstractEffect> newEffect;
  if (normalizedId == QStringLiteral("fractal_noise")) {
    newEffect = std::make_shared<FractalNoiseGenerator>();
  } else if (normalizedId == QStringLiteral("procedural_texture")) {
    newEffect = std::make_shared<ProceduralTextureGeneratorEffect>();
  } else if (normalizedId == QStringLiteral("transform_2d")) {
    newEffect = std::make_shared<LayerTransform2D>();
  } else {
    auto effect = effectService->createEffect(EffectID(normalizedId));
    if (effect) {
      newEffect = std::shared_ptr<ArtifactAbstractEffect>(std::move(effect));
    }
  }

  if (!newEffect) {
    if (statusLabel) {
      statusLabel->setText(
          QStringLiteral("Status: Failed to create effect for %1")
              .arg(normalizedId));
    }
    return;
  }

  const auto catalogEntries = buildEffectCatalogEntries();
  const auto catalogEntry =
      std::find_if(catalogEntries.begin(), catalogEntries.end(),
                   [&normalizedId](const EffectCatalogEntry &entry) {
                     return entry.effectId == normalizedId;
                   });
  if (catalogEntry != catalogEntries.end()) {
    newEffect->setPipelineStage(catalogEntry->stage);
  }

  // Layer effect stacks intentionally expose only the Rasterizer stage. A
  // catalog entry can carry a composition pipeline stage, so normalize the
  // instance before insertion or the add succeeds but immediately disappears
  // from the layer Effects surface.
  if (!editingCompositionEffects()) {
    newEffect->setPipelineStage(EffectPipelineStage::Rasterizer);
  }

  if (newEffect->pipelineStage() == EffectPipelineStage::Rasterizer) {
    QSize sourceSize = comp->settings().compositionSize();
    if (!editingCompositionEffects()) {
      if (auto targetLayer = comp->layerById(currentLayerId_)) {
        const auto layerSourceSize = targetLayer->sourceSize();
        if (layerSourceSize.width > 0 && layerSourceSize.height > 0) {
          sourceSize = QSize(layerSourceSize.width, layerSourceSize.height);
        }
      }
    }

    RasterizerInitialSettingsMode initialMode =
        rasterizerInitialSettingsModeFromSettings();
    if (initialMode == RasterizerInitialSettingsMode::AskWhenAdding) {
      RasterizerInitialSettingsDialog dialog(
          sourceSize, containerWidget ? containerWidget : tabWidget);
      if (dialog.exec() != QDialog::Accepted) {
        return;
      }
      initialMode = dialog.selectedMode();
      if (dialog.rememberChoice()) {
        QSettings settings(QStringLiteral("ArtifactStudio"),
                           QStringLiteral("Artifact"));
        settings.setValue(QString::fromLatin1(kRasterizerInitialSettingsKey),
                          rasterizerInitialSettingsModeValue(initialMode));
      }
    }
    if (initialMode == RasterizerInitialSettingsMode::FitToSource) {
      applyRasterizerSourceFit(newEffect.get(), sourceSize);
    }
  }

  bool added = editingCompositionEffects()
                   ? projectService->addEffectToCurrentComposition(newEffect)
                   : projectService->addEffectToLayerWithUndo(currentLayerId_,
                                                              newEffect);
  if (!added && !editingCompositionEffects()) {
    // Keep the Effects tab functional when the undo command cannot resolve
    // the current composition snapshot even though the selected layer is
    // still valid in the inspector.
    if (auto targetLayer = comp->layerById(currentLayerId_)) {
      targetLayer->addEffect(newEffect);
      const auto targetEffects = targetLayer->getEffects();
      added = std::any_of(
          targetEffects.begin(), targetEffects.end(),
          [&newEffect](const std::shared_ptr<ArtifactAbstractEffect> &effect) {
            return effect == newEffect;
          });
    }
  }
  if (!added) {
    if (statusLabel) {
      statusLabel->setText(
          QStringLiteral("Status: Failed to add %1")
              .arg(newEffect->displayName().toQString()));
    }
    return;
  }

  focusedEffectId_ = newEffect->effectID().toQString();
  lastRackSignatures_.fill(QString());
  lastEffectPropertyStateSignature_.clear();
  updateEffectsList();
  scheduleRefresh(EffectsDirty);
  if (statusLabel) {
    statusLabel->setText(
        QStringLiteral("Status: %1 effect added - %2.")
            .arg(editingCompositionEffects() ? QStringLiteral("Composition")
                                             : QStringLiteral("Layer"),
                 newEffect->displayName().toQString()));
  }
  if (tabWidget) {
    tabWidget->setCurrentIndex(2);
  }
}

void ArtifactInspectorWidget::Impl::handleAddEffectClicked(int rackIndex) {
  if (currentCompositionId_.isNil())
    return;

  auto *projectService = ArtifactProjectService::instance();
  if (!projectService)
    return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return;

  if (!editingCompositionEffects() && !comp->layerById(currentLayerId_)) {
    return;
  }

  if (!editingCompositionEffects()) {
    rackIndex = rasterizerRackIndex();
  } else if (rackIndex < 0 || rackIndex >= kEffectRackCount) {
    // Default to the rasterizer stage when invoked from the single header button.
    rackIndex = rasterizerRackIndex();
  }

  QString targetLabel = editingCompositionEffects()
                            ? QStringLiteral("Composition")
                            : QStringLiteral("Layer");
  if (!editingCompositionEffects()) {
    if (auto layer = comp->layerById(currentLayerId_)) {
      targetLabel = QStringLiteral("Layer \"%1\"")
                        .arg(layer->layerName());
    }
  } else {
    targetLabel = QStringLiteral("Composition \"%1\"")
                      .arg(comp->settings().compositionName().toQString());
  }

  EffectPickerDialog dialog(buildEffectCatalogEntries(),
                            stageFromRackIndex(rackIndex), targetLabel,
                            containerWidget ? containerWidget : tabWidget);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  addSelectedEffectToCurrentTarget(dialog.selectedEffectId());
}


void ArtifactInspectorWidget::Impl::handleRemoveEffectClicked(int rackIndex) {
  if (rackIndex < 0 || rackIndex >= kEffectRackCount)
    return;
  if (!racks[rackIndex].listWidget)
    return;

  auto selectedItems = racks[rackIndex].listWidget->selectedItems();
  if (selectedItems.isEmpty())
    return;

  if (currentCompositionId_.isNil())
    return;

  auto projectService = ArtifactProjectService::instance();
  if (!projectService)
    return;

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success)
    return;

  auto comp = findResult.ptr.lock();
  if (!comp)
    return;

  if (!editingCompositionEffects() && !comp->layerById(currentLayerId_)) {
    return;
  }

  if (!ArtifactMessageBox::confirmDelete(
          containerWidget, QStringLiteral("Remove Effect"),
          QStringLiteral("選択したエフェクトを削除しますか？"))) {
    return;
  }

  int removedCount = 0;
  for (auto item : selectedItems) {
    UniString effectID(item->data(Qt::UserRole).toString().toStdString());
    if (effectID.length() > 0) {
      const QString eid = effectID.toQString();
      const bool removed = editingCompositionEffects()
                               ? projectService
                                     ->removeEffectFromCurrentComposition(eid)
                               : [&]() {
                                   std::shared_ptr<ArtifactAbstractEffect>
                                       capturedEffect;
                                   if (auto layer =
                                           comp->layerById(currentLayerId_)) {
                                     for (const auto &e :
                                          layer->getEffects()) {
                                       if (e && e->effectID().toQString() ==
                                                    eid) {
                                         capturedEffect = e;
                                         break;
                                       }
                                     }
                                   }
                                   return projectService
                                       ->removeEffectFromLayerWithUndo(
                                           currentLayerId_, eid,
                                           capturedEffect);
                                 }();
      if (removed) {
        qDebug() << "[Inspector] Effect removed:" << eid;
        ++removedCount;
      }
    }
  }

  updateEffectsList();
  if (removedCount > 0 && statusLabel) {
    statusLabel->setText(
        QStringLiteral("Status: Removed %1 effect(s)").arg(removedCount));
  }
}

void ArtifactInspectorWidget::Impl::handleApplyLipSyncToSwitchLayer() {
  if (currentCompositionId_.isNil() || currentLayerId_.isNil()) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService) {
    return;
  }

  auto findResult = projectService->findComposition(currentCompositionId_);
  if (!findResult.success) {
    return;
  }

  auto comp = findResult.ptr.lock();
  if (!comp) {
    return;
  }

  auto layer = comp->layerById(currentLayerId_);
  auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer);
  if (!audio) {
    QMessageBox::warning(containerWidget, QStringLiteral("Lip Sync"),
                         QStringLiteral("Select an audio layer first."));
    return;
  }

  std::shared_ptr<ArtifactSwitchLayer> switchTarget;
  auto *selMgr = ArtifactLayerSelectionManager::instance();
  const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
  for (const auto &selectedLayer : selected) {
    if (!selectedLayer || selectedLayer == layer) {
      continue;
    }
    switchTarget = std::dynamic_pointer_cast<ArtifactSwitchLayer>(selectedLayer);
    if (switchTarget) {
      break;
    }
  }
  if (!switchTarget) {
    for (const auto &candidate : comp->allLayer()) {
      if (!candidate || candidate == layer) {
        continue;
      }
      switchTarget = std::dynamic_pointer_cast<ArtifactSwitchLayer>(candidate);
      if (switchTarget) {
        break;
      }
    }
  }

  if (!switchTarget) {
    QMessageBox::warning(containerWidget, QStringLiteral("Lip Sync"),
                         QStringLiteral("Select a Switch Layer in the same composition."));
    return;
  }

  QMessageBox::information(
      containerWidget, QStringLiteral("Lip Sync"),
      QStringLiteral("Lip Sync の適用は現在のビルドでは無効化されています。"));
  statusLabel->setText(QStringLiteral("Status: Lip Sync applied to Switch Layer"));
  QMessageBox::information(containerWidget, QStringLiteral("Lip Sync"),
                           QStringLiteral("Lip Sync を Switch Layer に適用しました。"));
}

void ArtifactInspectorWidget::update() {}

void ArtifactInspectorWidget::focusInEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kInspectorContext));
  }
  QScrollArea::focusInEvent(event);
}

void ArtifactInspectorWidget::focusOutEvent(QFocusEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    if (input->activeContext() == QString::fromLatin1(kInspectorContext)) {
      input->setActiveContext(QStringLiteral("Global"));
    }
  }
  QScrollArea::focusOutEvent(event);
}

void ArtifactInspectorWidget::keyPressEvent(QKeyEvent* event)
{
  if (auto* input = InputOperator::instance()) {
    input->setActiveContext(QString::fromLatin1(kInspectorContext));
    if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
      event->accept();
      return;
    }
  }
  QScrollArea::keyPressEvent(event);
}

ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget *parent /*= nullptr*/)
    : QScrollArea(parent), impl_(new Impl()) {
  setFocusPolicy(Qt::StrongFocus);
  // メインレイアウト
  auto mainLayout = new QVBoxLayout();
  impl_->containerWidget = new QWidget();
  impl_->containerWidget->setObjectName(QStringLiteral("inspectorContainer"));
  applyInspectorPalette(impl_->containerWidget);

  // タブウィジェットを作成
  impl_->tabWidget = new QTabWidget();
  impl_->tabWidget->setObjectName(QStringLiteral("inspectorTabWidget"));
  applyInspectorPalette(impl_->tabWidget);

  // ================== Layer Info Tab ==================
  auto layerInfoWidget = new QWidget();
  layerInfoWidget->setObjectName(QStringLiteral("inspectorLayerInfoWidget"));
  auto layerInfoLayout = new QVBoxLayout();

  // ステータスラベル
  impl_->statusLabel = new QLabel("Status: Open a project to inspect layers");
  {
    QFont f = impl_->statusLabel->font();
    f.setItalic(true);
    impl_->statusLabel->setFont(f);
    applyInspectorLabelPalette(impl_->statusLabel, false);
  }
  layerInfoLayout->addWidget(impl_->statusLabel);

  // レイヤー名ラベル
  impl_->layerNameLabel = new QLabel("Layer: Open a project to inspect layers");
  {
    QFont f = impl_->layerNameLabel->font();
    f.setBold(true);
    f.setPointSize(13);
    impl_->layerNameLabel->setFont(f);
    applyInspectorLabelPalette(impl_->layerNameLabel, true);
  }
  layerInfoLayout->addWidget(impl_->layerNameLabel);

  // レイヤータイプラベル
  impl_->layerTypeLabel = new QLabel("Type: N/A");
  applyInspectorLabelPalette(impl_->layerTypeLabel, false);
  layerInfoLayout->addWidget(impl_->layerTypeLabel);

  impl_->matteInfoLabel = new MatteInfoLabel();
  impl_->matteInfoLabel->setText("Matte: none");
  impl_->matteInfoLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->matteInfoLabel, false);
  layerInfoLayout->addWidget(impl_->matteInfoLabel);

  impl_->proxyInfoLabel = new ProxyInfoLabel();
  impl_->proxyInfoLabel->setText("Proxy: not available");
  impl_->proxyInfoLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->proxyInfoLabel, false);
  layerInfoLayout->addWidget(impl_->proxyInfoLabel);

  impl_->componentsGroup = new QWidget();
  applyInspectorPalette(impl_->componentsGroup);
  auto componentsLayout = new QVBoxLayout();
  impl_->componentsSummaryLabel =
      new QLabel("Components: select a layer", impl_->componentsGroup);
  impl_->componentsSummaryLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->componentsSummaryLabel, true);

  impl_->physicsComponentButton = new InspectorActionButton("Physics");
  impl_->scriptComponentButton = new InspectorActionButton("Script");
  impl_->layoutComponentButton = new InspectorActionButton("Layout");
  impl_->cloneComponentButton = new InspectorActionButton("Cloner");
  impl_->fluidComponentButton = new InspectorActionButton("Fluid");
  impl_->generatorComponentButton = new InspectorActionButton("+ Generator");
  impl_->removeGeneratorComponentButton = new InspectorActionButton("Remove");
  impl_->generatorMoveUpButton = new InspectorActionButton("Up");
  impl_->generatorMoveDownButton = new InspectorActionButton("Down");
  impl_->fieldComponentButton = new InspectorActionButton("+ Field");
  impl_->removeFieldComponentButton = new InspectorActionButton("Remove Field");
  impl_->fieldMoveUpButton = new InspectorActionButton("Field Up");
  impl_->fieldMoveDownButton = new InspectorActionButton("Field Down");
  impl_->cloneModifierButton = new InspectorActionButton("+ Clone Mod");
  impl_->removeCloneModifierButton = new InspectorActionButton("Remove Mod");
  impl_->cloneModifierMoveUpButton = new InspectorActionButton("Mod Up");
  impl_->cloneModifierMoveDownButton = new InspectorActionButton("Mod Down");
  impl_->openScriptButton = new InspectorActionButton("Open Script");
  impl_->applyLipSyncButton = new InspectorActionButton("Lip Sync");
  for (auto *button : {impl_->physicsComponentButton,
                       impl_->scriptComponentButton,
                       impl_->layoutComponentButton,
                       impl_->cloneComponentButton,
                       impl_->fluidComponentButton,
                       impl_->generatorComponentButton,
                       impl_->removeGeneratorComponentButton,
                       impl_->generatorMoveUpButton,
                       impl_->generatorMoveDownButton,
                       impl_->fieldComponentButton,
                       impl_->removeFieldComponentButton,
                       impl_->fieldMoveUpButton,
                       impl_->fieldMoveDownButton,
                       impl_->cloneModifierButton,
                       impl_->removeCloneModifierButton,
                       impl_->cloneModifierMoveUpButton,
                       impl_->cloneModifierMoveDownButton,
                       impl_->openScriptButton,
                       impl_->applyLipSyncButton}) {
    button->setOwnerDrawn(true);
  }
  impl_->physicsComponentButton->setCheckable(true);
  impl_->scriptComponentButton->setCheckable(true);
  impl_->layoutComponentButton->setCheckable(true);
  impl_->cloneComponentButton->setCheckable(true);
  impl_->fluidComponentButton->setCheckable(true);
  impl_->physicsComponentButton->setMinimumHeight(30);
  impl_->scriptComponentButton->setMinimumHeight(30);
  impl_->layoutComponentButton->setMinimumHeight(30);
  impl_->cloneComponentButton->setMinimumHeight(30);
  impl_->fluidComponentButton->setMinimumHeight(30);
  // Component entries are the primary browse surface.  Give them a stable
  // card-like rhythm so the active component reads separately from utilities.
  for (auto *componentButton : {impl_->physicsComponentButton,
                                impl_->scriptComponentButton,
                                impl_->layoutComponentButton,
                                impl_->cloneComponentButton,
                                impl_->fluidComponentButton}) {
    componentButton->setMinimumHeight(42);
    componentButton->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
  }
  applyInspectorButton(impl_->physicsComponentButton, false);
  applyInspectorButton(impl_->scriptComponentButton, false);
  applyInspectorButton(impl_->layoutComponentButton, false);
  applyInspectorButton(impl_->cloneComponentButton, false);
  applyInspectorButton(impl_->fluidComponentButton, false);
  applyInspectorButton(impl_->generatorComponentButton, false);
  applyInspectorButton(impl_->removeGeneratorComponentButton, false);
  applyInspectorButton(impl_->generatorMoveUpButton, false);
  applyInspectorButton(impl_->generatorMoveDownButton, false);
  applyInspectorButton(impl_->fieldComponentButton, false);
  applyInspectorButton(impl_->removeFieldComponentButton, false);
  applyInspectorButton(impl_->fieldMoveUpButton, false);
  applyInspectorButton(impl_->fieldMoveDownButton, false);
  applyInspectorButton(impl_->cloneModifierButton, false);
  applyInspectorButton(impl_->removeCloneModifierButton, false);
  applyInspectorButton(impl_->cloneModifierMoveUpButton, false);
  applyInspectorButton(impl_->cloneModifierMoveDownButton, false);
  applyInspectorButton(impl_->openScriptButton, false);
  applyInspectorButton(impl_->applyLipSyncButton, false);
  impl_->physicsComponentButton->setToolTip(
      QStringLiteral("Show the Physics component settings."));
  impl_->scriptComponentButton->setToolTip(
      QStringLiteral("Show the Script component settings."));
  impl_->layoutComponentButton->setToolTip(
      QStringLiteral("Show the Layout component settings."));
  impl_->cloneComponentButton->setToolTip(
      QStringLiteral("Show the Cloner component settings."));
  impl_->fluidComponentButton->setToolTip(
      QStringLiteral("Show the Fluid component settings."));
  impl_->generatorComponentButton->setToolTip(
      QStringLiteral("Add an extra generator to the layer."));
  impl_->removeGeneratorComponentButton->setToolTip(
      QStringLiteral("Remove the selected extra generator."));
  impl_->generatorMoveUpButton->setToolTip(
      QStringLiteral("Move the selected extra generator up."));
  impl_->generatorMoveDownButton->setToolTip(
      QStringLiteral("Move the selected extra generator down."));
  impl_->fieldComponentButton->setToolTip(
      QStringLiteral("Add a field to the layer."));
  impl_->removeFieldComponentButton->setToolTip(
      QStringLiteral("Remove the selected field."));
  impl_->fieldMoveUpButton->setToolTip(
      QStringLiteral("Move the selected field up."));
  impl_->fieldMoveDownButton->setToolTip(
      QStringLiteral("Move the selected field down."));
  impl_->cloneModifierButton->setToolTip(
      QStringLiteral("Add a clone modifier to the layer."));
  impl_->removeCloneModifierButton->setToolTip(
      QStringLiteral("Remove the selected clone modifier."));
  impl_->cloneModifierMoveUpButton->setToolTip(
      QStringLiteral("Move the selected clone modifier up."));
  impl_->cloneModifierMoveDownButton->setToolTip(
      QStringLiteral("Move the selected clone modifier down."));
  impl_->openScriptButton->setToolTip(
      QStringLiteral("Open the script file linked to this layer."));
  impl_->applyLipSyncButton->setToolTip(
      QStringLiteral("Build a lip sync track from the audio layer and apply it to a Switch Layer."));
  impl_->addComponentButton =
      new InspectorActionButton(QStringLiteral("+ Add Component"));
  impl_->addComponentButton->setOwnerDrawn(true);
  applyInspectorButton(impl_->addComponentButton, false);
  impl_->addComponentButton->setMinimumHeight(30);
  impl_->addComponentButton->setMaximumWidth(240);
  impl_->addComponentButton->setSizePolicy(QSizePolicy::Preferred,
                                           QSizePolicy::Preferred);
  impl_->addComponentButton->setToolTip(
      QStringLiteral("Add or enable a component on the selected layer."));
  for (auto *componentButton : {impl_->physicsComponentButton,
                                impl_->scriptComponentButton,
                                impl_->layoutComponentButton,
                                impl_->cloneComponentButton,
                                impl_->fluidComponentButton,
                                impl_->openScriptButton,
                                impl_->applyLipSyncButton}) {
    componentButton->setParent(impl_->componentsGroup);
    componentButton->setVisible(false);
  }

  impl_->activeComponentLabel =
      new QLabel(QStringLiteral("Active Component  |  None"),
                 impl_->componentsGroup);
  applyInspectorLabelPalette(impl_->activeComponentLabel, true);
  impl_->activeComponentLabel->setVisible(false);

  impl_->componentPropertyWidget =
      new ArtifactPropertyWidget(impl_->componentsGroup);
  impl_->componentPropertyWidget->setProperty(
      "artifactEmbeddedComponentEditor", true);
  impl_->componentPropertyWidget->setVisible(false);
  impl_->componentPropertyWidget->setMinimumHeight(120);
  // The active component filter is selected from the layer state during refresh.
  impl_->componentPropertyWidget->setFilterText(QString());

  auto *componentStackHeader = new QHBoxLayout();
  auto *componentStackLabel = new QLabel(QStringLiteral("Layer Components"));
  applyInspectorLabelPalette(componentStackLabel, true);
  componentStackHeader->addWidget(componentStackLabel, 1);
  componentsLayout->addLayout(componentStackHeader);
  componentsLayout->addWidget(impl_->componentsSummaryLabel);
  componentsLayout->addWidget(impl_->cloneComponentButton);
  componentsLayout->addWidget(impl_->layoutComponentButton);
  componentsLayout->addWidget(impl_->physicsComponentButton);
  componentsLayout->addWidget(impl_->fluidComponentButton);
  componentsLayout->addWidget(impl_->scriptComponentButton);

  auto *addComponentLayout = new QHBoxLayout();
  addComponentLayout->addStretch(1);
  addComponentLayout->addWidget(impl_->addComponentButton);
  addComponentLayout->addStretch(1);
  componentsLayout->addLayout(addComponentLayout);

  auto *componentDivider = new ComponentDivider(impl_->componentsGroup);
  componentDivider->setObjectName(QStringLiteral("inspectorComponentDivider"));
  componentDivider->setFrameShape(QFrame::HLine);
  componentDivider->setFrameShadow(QFrame::Plain);
  applyInspectorPalette(componentDivider, false);
  componentsLayout->addWidget(componentDivider);
  componentsLayout->addWidget(impl_->activeComponentLabel);
  componentsLayout->addWidget(impl_->componentPropertyWidget, 1);

  impl_->clonerStructureWidget = new QWidget(impl_->componentsGroup);
  auto *clonerStructureLayout = new QVBoxLayout(impl_->clonerStructureWidget);
  clonerStructureLayout->setContentsMargins(0, 4, 0, 0);
  clonerStructureLayout->setSpacing(6);
  auto *clonerStructureLabel = new QLabel(QStringLiteral("Cloner Structure"));
  applyInspectorLabelPalette(clonerStructureLabel, true);
  clonerStructureLayout->addWidget(clonerStructureLabel);

  auto generatorHeaderLayout = new QHBoxLayout();
  auto *generatorHeaderLabel = new QLabel(QStringLiteral("Generators"));
  applyInspectorLabelPalette(generatorHeaderLabel, true);
  generatorHeaderLayout->addWidget(generatorHeaderLabel, 1);
  generatorHeaderLayout->addWidget(impl_->generatorComponentButton);
  generatorHeaderLayout->addWidget(impl_->generatorMoveUpButton);
  generatorHeaderLayout->addWidget(impl_->generatorMoveDownButton);
  generatorHeaderLayout->addWidget(impl_->removeGeneratorComponentButton);
  clonerStructureLayout->addLayout(generatorHeaderLayout);
  impl_->generatorListWidget = new InspectorSelectionList();
  impl_->generatorListWidget->setItemDelegate(
      new ComponentStackItemDelegate(impl_->generatorListWidget));
  impl_->generatorListWidget->setVisible(false);
  impl_->generatorListWidget->setMaximumHeight(96);
  impl_->generatorListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->generatorListWidget);
  impl_->generatorListWidget->setAlternatingRowColors(false);
  impl_->generatorListWidget->setFrameShape(QFrame::NoFrame);
  impl_->generatorListWidget->setSpacing(2);
  clonerStructureLayout->addWidget(impl_->generatorListWidget);

  auto fieldHeaderLayout = new QHBoxLayout();
  auto *fieldHeaderLabel = new QLabel(QStringLiteral("Fields"));
  applyInspectorLabelPalette(fieldHeaderLabel, true);
  fieldHeaderLayout->addWidget(fieldHeaderLabel, 1);
  fieldHeaderLayout->addWidget(impl_->fieldComponentButton);
  fieldHeaderLayout->addWidget(impl_->fieldMoveUpButton);
  fieldHeaderLayout->addWidget(impl_->fieldMoveDownButton);
  fieldHeaderLayout->addWidget(impl_->removeFieldComponentButton);
  clonerStructureLayout->addLayout(fieldHeaderLayout);
  impl_->fieldListWidget = new InspectorSelectionList();
  impl_->fieldListWidget->setItemDelegate(
      new ComponentStackItemDelegate(impl_->fieldListWidget));
  impl_->fieldListWidget->setVisible(false);
  impl_->fieldListWidget->setMaximumHeight(96);
  impl_->fieldListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->fieldListWidget);
  impl_->fieldListWidget->setAlternatingRowColors(false);
  impl_->fieldListWidget->setFrameShape(QFrame::NoFrame);
  impl_->fieldListWidget->setSpacing(2);
  clonerStructureLayout->addWidget(impl_->fieldListWidget);

  auto cloneModifierHeaderLayout = new QHBoxLayout();
  auto *cloneModifierHeaderLabel =
      new QLabel(QStringLiteral("Clone Modifiers"));
  applyInspectorLabelPalette(cloneModifierHeaderLabel, true);
  cloneModifierHeaderLayout->addWidget(cloneModifierHeaderLabel, 1);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierButton);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierMoveUpButton);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierMoveDownButton);
  cloneModifierHeaderLayout->addWidget(impl_->removeCloneModifierButton);
  clonerStructureLayout->addLayout(cloneModifierHeaderLayout);
  impl_->cloneModifierListWidget = new InspectorSelectionList();
  impl_->cloneModifierListWidget->setItemDelegate(
      new ComponentStackItemDelegate(impl_->cloneModifierListWidget));
  impl_->cloneModifierListWidget->setVisible(false);
  impl_->cloneModifierListWidget->setMaximumHeight(96);
  impl_->cloneModifierListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->cloneModifierListWidget);
  impl_->cloneModifierListWidget->setAlternatingRowColors(false);
  impl_->cloneModifierListWidget->setFrameShape(QFrame::NoFrame);
  impl_->cloneModifierListWidget->setSpacing(2);
  clonerStructureLayout->addWidget(impl_->cloneModifierListWidget);
  impl_->clonerStructureWidget->setVisible(false);
  componentsLayout->addWidget(impl_->clonerStructureWidget);

  impl_->componentUtilitiesLabel = new QLabel(QStringLiteral("Layer Utilities"));
  applyInspectorLabelPalette(impl_->componentUtilitiesLabel, true);
  impl_->componentUtilitiesLabel->setVisible(false);
  componentsLayout->addWidget(impl_->componentUtilitiesLabel);
  componentsLayout->addWidget(impl_->openScriptButton);
  componentsLayout->addWidget(impl_->applyLipSyncButton);
  impl_->componentPropertyWidget = new ArtifactPropertyWidget();
  impl_->componentPropertyWidget->setVisible(false);
  impl_->componentPropertyWidget->setMinimumHeight(120);
  componentsLayout->addWidget(impl_->componentPropertyWidget);
  componentsLayout->setContentsMargins(
      kInspectorNoteMargin, kInspectorNoteMargin, kInspectorNoteMargin,
      kInspectorNoteMargin);
  impl_->componentsGroup->setLayout(componentsLayout);
  impl_->componentsGroup->setEnabled(false);
  layerInfoLayout->addWidget(impl_->componentsGroup);

  layerInfoLayout->setAlignment(Qt::AlignTop);
  layerInfoLayout->setContentsMargins(
      kInspectorSectionMarginL, kInspectorSectionMarginT,
      kInspectorSectionMarginR, kInspectorSectionMarginB);
  layerInfoLayout->setSpacing(kInspectorSectionSpacing);

  auto toggleComponent = [this](const QString &propertyPath,
                                const QString &displayName) {
    if (impl_->currentCompositionId_.isNil() ||
        impl_->currentLayerId_.isNil()) {
      return;
    }
    auto projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return;
    }
    auto findResult =
        projectService->findComposition(impl_->currentCompositionId_);
    if (!findResult.success) {
      return;
    }
    auto comp = findResult.ptr.lock();
    if (!comp) {
      return;
    }
    auto layer = comp->layerById(impl_->currentLayerId_);
    if (!layer) {
      return;
    }
    const bool nextEnabled = !layerBooleanProperty(layer, propertyPath);
    if (layer->setLayerPropertyValue(propertyPath, nextEnabled)) {
      impl_->focusedComponentName_ = displayName;
      impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
      impl_->focusComponentProperties(
          layer, nextEnabled ? componentInspectorFilter(displayName) : QString());
      impl_->updateComponentControls(layer);
      impl_->lastLayerInfoSignature_.clear();
      impl_->scheduleRefresh(
          ArtifactInspectorWidget::Impl::LayerInfoDirty |
          ArtifactInspectorWidget::Impl::EffectsDirty);
      if (impl_->statusLabel) {
        impl_->statusLabel->setText(
            QStringLiteral("Status: %1 component %2")
                .arg(displayName, nextEnabled ? QStringLiteral("enabled")
                                              : QStringLiteral("disabled")));
      }
    }
  };
  auto selectComponent = [this](const QString &displayName) {
    if (impl_->currentCompositionId_.isNil() || impl_->currentLayerId_.isNil()) {
      return;
    }
    auto projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return;
    }
    auto findResult =
        projectService->findComposition(impl_->currentCompositionId_);
    auto comp = findResult.success ? findResult.ptr.lock()
                                   : ArtifactCompositionPtr{};
    auto layer = comp ? comp->layerById(impl_->currentLayerId_)
                      : ArtifactAbstractLayerPtr{};
    if (!layer) {
      return;
    }
    impl_->focusedComponentName_ = displayName;
    impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
    impl_->updateComponentControls(layer);
    impl_->focusComponentProperties(layer, componentInspectorFilter(displayName));
  };
  impl_->addComponentButton->setAction([this, toggleComponent]() {
    if (!impl_->addComponentButton || !impl_->componentsGroup ||
        !impl_->componentsGroup->isEnabled()) {
      return;
    }
    QMenu menu;
    auto *physicsAction = menu.addAction(QStringLiteral("Physics"));
    auto *scriptAction = menu.addAction(QStringLiteral("Script"));
    auto *layoutAction = menu.addAction(QStringLiteral("Layout"));
    auto *clonerAction = menu.addAction(QStringLiteral("Cloner"));
    auto *fluidAction = menu.addAction(QStringLiteral("Fluid"));
    const auto markState = [](QAction *action, const QAbstractButton *button) {
      if (!action || !button) {
        return;
      }
      action->setCheckable(true);
      action->setChecked(button->property("artifactComponentEnabled").toBool());
    };
    markState(physicsAction, impl_->physicsComponentButton);
    markState(scriptAction, impl_->scriptComponentButton);
    markState(layoutAction, impl_->layoutComponentButton);
    markState(clonerAction, impl_->cloneComponentButton);
    markState(fluidAction, impl_->fluidComponentButton);
    menu.addSeparator();
    auto *generatorAction = menu.addAction(QStringLiteral("Generator"));
    auto *fieldAction = menu.addAction(QStringLiteral("Field"));
    auto *modifierAction = menu.addAction(QStringLiteral("Clone Modifier"));
    menu.addSeparator();
    auto *openScriptAction = menu.addAction(QStringLiteral("Open Script"));
    openScriptAction->setEnabled(impl_->openScriptButton->isEnabled());
    auto *lipSyncAction = menu.addAction(QStringLiteral("Apply Lip Sync"));
    lipSyncAction->setEnabled(impl_->applyLipSyncButton->isEnabled());
    const auto selected = menu.exec(
        impl_->addComponentButton->mapToGlobal(
            QPoint(0, impl_->addComponentButton->height())));
    if (selected == physicsAction) {
      toggleComponent(QStringLiteral("physics.enabled"),
                      QStringLiteral("Physics"));
    } else if (selected == scriptAction) {
      toggleComponent(QStringLiteral("component.script.enabled"),
                      QStringLiteral("Script"));
    } else if (selected == layoutAction) {
      toggleComponent(QStringLiteral("component.layout.enabled"),
                      QStringLiteral("Layout"));
    } else if (selected == clonerAction) {
      toggleComponent(QStringLiteral("component.cloner.enabled"),
                      QStringLiteral("Cloner"));
    } else if (selected == fluidAction) {
      toggleComponent(QStringLiteral("component.fluid.enabled"),
                      QStringLiteral("Fluid"));
    } else if (selected == generatorAction) {
      impl_->generatorComponentButton->triggerAction();
    } else if (selected == fieldAction) {
      impl_->fieldComponentButton->triggerAction();
    } else if (selected == modifierAction) {
      impl_->cloneModifierButton->triggerAction();
    } else if (selected == openScriptAction) {
      impl_->openScriptButton->triggerAction();
    } else if (selected == lipSyncAction) {
      impl_->applyLipSyncButton->triggerAction();
    }
  });
  impl_->physicsComponentButton->setAction([selectComponent]() {
                     selectComponent(QStringLiteral("Physics"));
                   });
  impl_->scriptComponentButton->setAction([selectComponent]() {
                     selectComponent(QStringLiteral("Script"));
                   });
  impl_->layoutComponentButton->setAction([selectComponent]() {
                     selectComponent(QStringLiteral("Layout"));
                   });
  impl_->cloneComponentButton->setAction([selectComponent]() {
                     selectComponent(QStringLiteral("Cloner"));
                   });
  impl_->fluidComponentButton->setAction([selectComponent]() {
                     selectComponent(QStringLiteral("Fluid"));
                   });
  impl_->generatorComponentButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QStringList generatorChoices = {
                         QStringLiteral("grid"),
                         QStringLiteral("radial"),
                     };
                     bool accepted = false;
                     const QString generatorChoice = QInputDialog::getItem(
                         this, QStringLiteral("Add Generator"),
                         QStringLiteral("Generator Type"),
                         generatorChoices, 0, false, &accepted);
                     if (!accepted || generatorChoice.trimmed().isEmpty()) {
                       return;
                     }
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.generators.add"),
                             generatorChoice)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget
                                   ? impl_->generatorListWidget->currentItem()
                                   : nullptr));
                       impl_->lastLayerInfoSignature_.clear();
                       impl_->scheduleRefresh(
                           ArtifactInspectorWidget::Impl::LayerInfoDirty |
                           ArtifactInspectorWidget::Impl::EffectsDirty);
                       if (impl_->statusLabel) {
                         impl_->statusLabel->setText(
                             QStringLiteral(
                                 "Status: extra generator added"));
                       }
                     }
                   });
  impl_->removeGeneratorComponentButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     QString generatorId;
                     if (impl_->generatorListWidget &&
                         impl_->generatorListWidget->currentItem()) {
                       generatorId = impl_->generatorListWidget->currentItem()
                                         ->data(Qt::UserRole)
                                         .toString();
                     }
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.generators.remove"),
                             generatorId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget
                                   ? impl_->generatorListWidget->currentItem()
                                   : nullptr));
                       impl_->lastLayerInfoSignature_.clear();
                       impl_->scheduleRefresh(
                           ArtifactInspectorWidget::Impl::LayerInfoDirty |
                           ArtifactInspectorWidget::Impl::EffectsDirty);
                       if (impl_->statusLabel) {
                         impl_->statusLabel->setText(
                             QStringLiteral(
                                 "Status: extra generator removed"));
                       }
                     }
                   });
  impl_->generatorMoveUpButton->setAction([this]() {
                     if (!impl_->generatorListWidget ||
                         !impl_->generatorListWidget->currentItem()) {
                       return;
                     }
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString generatorId = impl_->generatorListWidget
                                                     ->currentItem()
                                                     ->data(Qt::UserRole)
                                                     .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.generators.moveUp"),
                             generatorId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget->currentItem()));
                     }
                   });
  impl_->generatorMoveDownButton->setAction([this]() {
                     if (!impl_->generatorListWidget ||
                         !impl_->generatorListWidget->currentItem()) {
                       return;
                     }
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString generatorId = impl_->generatorListWidget
                                                     ->currentItem()
                                                     ->data(Qt::UserRole)
                                                     .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.generators.moveDown"),
                             generatorId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget->currentItem()));
                     }
                   });
  impl_->generatorListWidget->setSelectionAction(
      [this](QListWidgetItem *current) {
        if (impl_->currentCompositionId_.isNil() || impl_->currentLayerId_.isNil()) {
          return;
        }
        auto projectService = ArtifactProjectService::instance();
        if (!projectService) {
          return;
        }
        auto findResult =
            projectService->findComposition(impl_->currentCompositionId_);
        if (!findResult.success) {
          return;
        }
        auto comp = findResult.ptr.lock();
        if (!comp) {
          return;
        }
        auto layer = comp->layerById(impl_->currentLayerId_);
        if (!layer) {
          return;
        }
        const QString filterText = generatorItemFilterText(current);
        impl_->focusedComponentName_ = QStringLiteral("Generator");
        impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
        impl_->updateComponentControls(layer);
        impl_->focusComponentProperties(layer, filterText);
      });
  impl_->fieldComponentButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QStringList fieldChoices = {
                         QStringLiteral("solid"), QStringLiteral("sphere"),
                         QStringLiteral("box"),   QStringLiteral("linear"),
                         QStringLiteral("radial"), QStringLiteral("noise"),
                     };
                     bool accepted = false;
                     const QString fieldChoice = QInputDialog::getItem(
                         this, QStringLiteral("Add Field"),
                         QStringLiteral("Field Type"), fieldChoices, 0, false,
                         &accepted);
                     if (!accepted || fieldChoice.trimmed().isEmpty()) {
                       return;
                     }
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.fields.add"),
                             fieldChoice)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget
                                          ? impl_->fieldListWidget->currentItem()
                                          : nullptr));
                     }
                   });
  impl_->removeFieldComponentButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer || !impl_->fieldListWidget ||
                         !impl_->fieldListWidget->currentItem()) {
                       return;
                     }
                     const QString fieldId =
                         impl_->fieldListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.fields.remove"),
                             fieldId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  impl_->fieldMoveUpButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil() || !impl_->fieldListWidget ||
                         !impl_->fieldListWidget->currentItem()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString fieldId =
                         impl_->fieldListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.fields.moveUp"),
                             fieldId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  impl_->fieldMoveDownButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil() || !impl_->fieldListWidget ||
                         !impl_->fieldListWidget->currentItem()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString fieldId =
                         impl_->fieldListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.fields.moveDown"),
                             fieldId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  impl_->fieldListWidget->setSelectionAction(
      [this](QListWidgetItem *current) {
        if (impl_->currentCompositionId_.isNil() || impl_->currentLayerId_.isNil()) {
          return;
        }
        auto projectService = ArtifactProjectService::instance();
        if (!projectService) {
          return;
        }
        auto findResult =
            projectService->findComposition(impl_->currentCompositionId_);
        if (!findResult.success) {
          return;
        }
        auto comp = findResult.ptr.lock();
        if (!comp) {
          return;
        }
        auto layer = comp->layerById(impl_->currentLayerId_);
        if (!layer) {
          return;
        }
        const QString filterText = fieldItemFilterText(current);
        impl_->focusedComponentName_ = QStringLiteral("Field");
        impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
        impl_->updateComponentControls(layer);
        impl_->focusComponentProperties(layer, filterText);
      });
  impl_->cloneModifierButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QStringList modifierChoices = {
                         QStringLiteral("time-offset"),
                         QStringLiteral("sequence"),
                         QStringLiteral("plain"),
                         QStringLiteral("random"),
                         QStringLiteral("step"),
                     };
                     bool accepted = false;
                     const QString modifierChoice = QInputDialog::getItem(
                         this, QStringLiteral("Add Clone Modifier"),
                         QStringLiteral("Modifier Type"), modifierChoices, 0,
                         false, &accepted);
                     if (!accepted || modifierChoice.trimmed().isEmpty()) {
                       return;
                     }
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.cloneModifiers.add"),
                             modifierChoice)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget
                                   ? impl_->cloneModifierListWidget->currentItem()
                                   : nullptr));
                     }
                   });
  impl_->removeCloneModifierButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil() ||
                         !impl_->cloneModifierListWidget ||
                         !impl_->cloneModifierListWidget->currentItem()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString modifierId =
                         impl_->cloneModifierListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.cloneModifiers.remove"),
                             modifierId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  impl_->cloneModifierMoveUpButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil() ||
                         !impl_->cloneModifierListWidget ||
                         !impl_->cloneModifierListWidget->currentItem()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString modifierId =
                         impl_->cloneModifierListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.cloneModifiers.moveUp"),
                             modifierId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  impl_->cloneModifierMoveDownButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil() ||
                         !impl_->cloneModifierListWidget ||
                         !impl_->cloneModifierListWidget->currentItem()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString modifierId =
                         impl_->cloneModifierListWidget->currentItem()
                             ->data(Qt::UserRole)
                             .toString();
                     if (layer->setLayerPropertyValue(
                             QStringLiteral("component.cloneModifiers.moveDown"),
                             modifierId)) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  impl_->cloneModifierListWidget->setSelectionAction(
      [this](QListWidgetItem *current) {
        if (impl_->currentCompositionId_.isNil() || impl_->currentLayerId_.isNil()) {
          return;
        }
        auto projectService = ArtifactProjectService::instance();
        if (!projectService) {
          return;
        }
        auto findResult =
            projectService->findComposition(impl_->currentCompositionId_);
        if (!findResult.success) {
          return;
        }
        auto comp = findResult.ptr.lock();
        if (!comp) {
          return;
        }
        auto layer = comp->layerById(impl_->currentLayerId_);
        if (!layer) {
          return;
        }
        const QString filterText = cloneModifierItemFilterText(current);
        impl_->focusedComponentName_ = QStringLiteral("Clone Modifier");
        impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
        impl_->updateComponentControls(layer);
        impl_->focusComponentProperties(layer, filterText);
      });
  impl_->openScriptButton->setAction([this]() {
                     if (impl_->currentCompositionId_.isNil() ||
                         impl_->currentLayerId_.isNil()) {
                       return;
                     }
                     auto projectService = ArtifactProjectService::instance();
                     if (!projectService) {
                       return;
                     }
                     auto findResult =
                         projectService->findComposition(impl_->currentCompositionId_);
                     if (!findResult.success) {
                       return;
                     }
                     auto comp = findResult.ptr.lock();
                     if (!comp) {
                       return;
                     }
                     auto layer = comp->layerById(impl_->currentLayerId_);
                     if (!layer) {
                       return;
                     }
                     const QString scriptPath = resolveScriptBindingPath(layer);
                     if (scriptPath.trimmed().isEmpty()) {
                       return;
                     }
                     const QFileInfo info(scriptPath);
                     const QString openPath =
                         info.isDir() ? info.absoluteFilePath()
                                      : info.absoluteFilePath();
                     QDesktopServices::openUrl(
                         QUrl::fromLocalFile(openPath));
                   });
  impl_->applyLipSyncButton->setAction([this]() {
    impl_->handleApplyLipSyncToSwitchLayer();
  });

  layerInfoWidget->setLayout(layerInfoLayout);
  // Keep the component editor as a first-class surface, matching the
  // component-oriented inspector layout while preserving the existing widget
  // and its action wiring.
  layerInfoLayout->removeWidget(impl_->componentsGroup);
  auto *componentsTab = new ArtifactComponentTabSurface(impl_->componentsGroup);
  impl_->tabWidget->addTab(layerInfoWidget, "Layer");
  impl_->tabWidget->addTab(componentsTab, "Components");

  // ================== Effects Pipeline Tab ==================
  impl_->effectsScrollArea = new QScrollArea();
  impl_->effectsScrollArea->setObjectName(QStringLiteral("inspectorEffectsScrollArea"));
  impl_->effectsScrollArea->setWidgetResizable(true);
  impl_->effectsTabWidget = new QWidget();
  impl_->effectsTabWidget->setObjectName(QStringLiteral("inspectorEffectsTabWidget"));
  auto effectsLayout = new QVBoxLayout();
  auto *effectsHeaderFrame = new QFrame();
  effectsHeaderFrame->setObjectName(QStringLiteral("inspectorEffectsHeaderFrame"));
  applyInspectorPalette(effectsHeaderFrame, false);
  auto *effectsHeaderLayout = new QVBoxLayout(effectsHeaderFrame);
  effectsHeaderLayout->setContentsMargins(10, 10, 10, 10);
  effectsHeaderLayout->setSpacing(6);

  impl_->effectsStateLabel =
      new QLabel("Open a composition to manage effects.");
  impl_->effectsStateLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStateLabel, true);
  effectsHeaderLayout->addWidget(impl_->effectsStateLabel);

  impl_->effectsTargetLabel =
      new QLabel("Target: No composition selected");
  impl_->effectsTargetLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsTargetLabel, false);
  effectsHeaderLayout->addWidget(impl_->effectsTargetLabel);

  auto *effectsToolbarLayout = new QHBoxLayout();
  effectsToolbarLayout->setContentsMargins(0, 0, 0, 0);
  effectsToolbarLayout->setSpacing(8);
  impl_->effectsQuickAddButton = new QPushButton("+ Add Effect");
  impl_->effectsQuickAddButton->setObjectName(QStringLiteral("inspectorEffectsQuickAddButton"));
  applyInspectorButton(impl_->effectsQuickAddButton, true);
  impl_->effectsQuickAddButton->setToolTip(
      QStringLiteral("Open a searchable picker and add an effect to the current target. Shortcut: Ctrl+Space."));
  effectsToolbarLayout->addWidget(impl_->effectsQuickAddButton);
  effectsToolbarLayout->addStretch(1);
  effectsHeaderLayout->addLayout(effectsToolbarLayout);
  effectsLayout->addWidget(effectsHeaderFrame);

  // AE-style Effect Controls is one continuous browse-and-edit surface.  Do
  // not insert a second Stack / Editor page switch inside the Effects tab.
  impl_->effectsModeTabs = nullptr;

  auto *stackPanel = new QFrame();
  stackPanel->setObjectName(QStringLiteral("inspectorEffectsStackPanel"));
  applyInspectorPalette(stackPanel, false);
  auto *stackPanelLayout = new QVBoxLayout(stackPanel);
  stackPanelLayout->setContentsMargins(8, 8, 8, 8);
  stackPanelLayout->setSpacing(8);

  impl_->effectsStackSummaryLabel = new QLabel("Effect Controls");
  impl_->effectsStackSummaryLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStackSummaryLabel, false);
  stackPanelLayout->addWidget(impl_->effectsStackSummaryLabel);

  auto *detailPanel = new QFrame();
  detailPanel->setObjectName(QStringLiteral("inspectorEffectsDetailPanel"));
  applyInspectorPalette(detailPanel, false);
  auto *detailPanelLayout = new QVBoxLayout(detailPanel);
  detailPanelLayout->setContentsMargins(8, 8, 8, 8);
  detailPanelLayout->setSpacing(8);

  impl_->effectEditorTitleLabel = new QLabel("Selected Effect Controls");
  applyInspectorLabelPalette(impl_->effectEditorTitleLabel, true);
  detailPanelLayout->addWidget(impl_->effectEditorTitleLabel);

  impl_->effectEnableButton =
      new InspectorActionButton(QStringLiteral("Enabled"));
  impl_->effectEnableButton->setCheckable(true);
  impl_->effectEnableButton->setVisible(false);
  impl_->effectEnableButton->setMinimumHeight(28);
  applyInspectorButton(impl_->effectEnableButton, false);
  impl_->effectEnableButton->setToolTip(
      QStringLiteral("Temporarily bypass the selected effect."));
  detailPanelLayout->addWidget(impl_->effectEnableButton);

  impl_->effectParametersHintLabel =
      new QLabel("Select an effect above to reveal its parameters here.");
  impl_->effectParametersHintLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectParametersHintLabel, false);
  detailPanelLayout->addWidget(impl_->effectParametersHintLabel);

  impl_->effectPropertyWidget = new ArtifactPropertyWidget();
  impl_->effectPropertyWidget->setVisible(false);
  impl_->effectPropertyWidget->setMinimumHeight(220);
  detailPanelLayout->addWidget(impl_->effectPropertyWidget, 1);

  impl_->effectEnableButton->setAction([this]() {
    const QString effectId = impl_->focusedEffectId_.trimmed();
    if (effectId.isEmpty()) {
      return;
    }
    const auto effect = impl_->currentEffectById(effectId);
    if (!effect) {
      return;
    }
    const bool nextEnabled = !effect->isEnabled();
    if (impl_->setEffectEnabledById(effectId, nextEnabled)) {
      impl_->effectEnableButton->setChecked(nextEnabled);
      impl_->effectEnableButton->setText(
          nextEnabled ? QStringLiteral("Enabled")
                      : QStringLiteral("Disabled"));
      applyInspectorComponentStateButton(impl_->effectEnableButton,
                                         nextEnabled);
      impl_->updateEffectRackItemEnabled(effectId, nextEnabled);
      impl_->syncEffectPropertyWidget();
      if (impl_->statusLabel) {
        impl_->statusLabel->setText(
            QStringLiteral("Status: Effect %1")
                .arg(nextEnabled ? QStringLiteral("enabled")
                                 : QStringLiteral("disabled")));
      }
    }
  });

  QString rackNames[5] = {"Generator", "Geo Transform", "Material",
                          "Rasterizer", "Layer Transform"};

  for (int i = 0; i < 5; ++i) {
    auto rackGroup = new QGroupBox(rackNames[i]);
    impl_->racks[i].groupBox = rackGroup;
    applyInspectorSectionBox(rackGroup);
    applyInspectorPalette(rackGroup, false);
    rackGroup->setFlat(true);
    auto rackLayout = new QVBoxLayout();

    impl_->racks[i].listWidget = new QListWidget();
    impl_->racks[i].listWidget->setMinimumHeight(38);
    impl_->racks[i].listWidget->setMaximumHeight(132);
    impl_->racks[i].listWidget->setUniformItemSizes(true);
    impl_->racks[i].listWidget->setFrameShape(QFrame::NoFrame);
    impl_->racks[i].listWidget->setSpacing(5);
    impl_->racks[i].listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    impl_->racks[i].listWidget->setItemDelegate(
        new EffectRackItemDelegate(i, impl_->racks[i].listWidget));
    impl_->racks[i].listWidget->setToolTip(
        QStringLiteral("Single click an effect to edit its parameters below. Double click toggles enable/disable. Right click opens effect actions."));
    applyInspectorList(impl_->racks[i].listWidget);
    impl_->racks[i].listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    if (impl_->racks[i].listWidget->viewport()) {
      impl_->racks[i].listWidget->viewport()->setContextMenuPolicy(
          Qt::CustomContextMenu);
    }

    auto btnLayout = new QHBoxLayout();
  impl_->racks[i].addButton = new QPushButton("+ Add");
  impl_->racks[i].addButton->setObjectName(QStringLiteral("inspectorRackAddButton"));
  impl_->racks[i].removeButton = new QPushButton("Remove");
  impl_->racks[i].removeButton->setObjectName(QStringLiteral("inspectorRackRemoveButton"));
  impl_->racks[i].moveUpButton = new QPushButton(QStringLiteral("Move Up"));
  impl_->racks[i].moveUpButton->setObjectName(QStringLiteral("inspectorRackMoveUpButton"));
  impl_->racks[i].moveDownButton = new QPushButton(QStringLiteral("Move Down"));
  impl_->racks[i].moveDownButton->setObjectName(QStringLiteral("inspectorRackMoveDownButton"));
    applyInspectorButton(impl_->racks[i].addButton, false);
    applyInspectorButton(impl_->racks[i].removeButton, false);
    applyInspectorButton(impl_->racks[i].moveUpButton, false);
    applyInspectorButton(impl_->racks[i].moveDownButton, false);
    // Keep the stage-local add affordance visible; composition stages are
    // intentionally readable without opening a separate picker first.
    impl_->racks[i].addButton->setVisible(true);
    impl_->racks[i].addButton->setEnabled(false);
    btnLayout->addWidget(impl_->racks[i].addButton);
    btnLayout->addWidget(impl_->racks[i].moveUpButton);
    btnLayout->addWidget(impl_->racks[i].moveDownButton);
    btnLayout->addWidget(impl_->racks[i].removeButton);
    impl_->racks[i].addButton->setToolTip(
        QStringLiteral("Add a new %1 effect to this stage.")
            .arg(rackNames[i]));
    impl_->racks[i].removeButton->setToolTip(QStringLiteral("Remove the selected effect(s)."));
    impl_->racks[i].moveUpButton->setToolTip(QStringLiteral("Move the selected effect up."));
    impl_->racks[i].moveDownButton->setToolTip(QStringLiteral("Move the selected effect down."));

    rackLayout->addWidget(impl_->racks[i].listWidget);
    rackLayout->addLayout(btnLayout);
    rackLayout->setContentsMargins(kInspectorRackMarginL, 8,
                                   kInspectorRackMarginR,
                                   kInspectorRackMarginB);
    rackGroup->setLayout(rackLayout);

    stackPanelLayout->addWidget(rackGroup);

    // Button signals
    QObject::connect(impl_->racks[i].addButton, &QPushButton::clicked, this,
                     [this, i]() { impl_->handleAddEffectClicked(i); });
    QObject::connect(impl_->racks[i].removeButton, &QPushButton::clicked, this,
                     [this, i]() { impl_->handleRemoveEffectClicked(i); });
    QObject::connect(
        impl_->racks[i].moveUpButton, &QPushButton::clicked, this, [this, i]() {
          auto *list = impl_->racks[i].listWidget;
          if (!list)
            return;
          auto *item = list->currentItem();
          if (!item)
            return;
          const QString effectId = item->data(Qt::UserRole).toString();
          if (effectId.trimmed().isEmpty())
            return;
          if (impl_->moveEffectById(effectId, -1)) {
            impl_->updateEffectsList();
            if (impl_->statusLabel) {
              impl_->statusLabel->setText(
                  QStringLiteral("Status: Effect moved up"));
            }
          }
        });
    QObject::connect(impl_->racks[i].moveDownButton, &QPushButton::clicked,
                     this, [this, i]() {
                       auto *list = impl_->racks[i].listWidget;
                       if (!list)
                         return;
                       auto *item = list->currentItem();
                       if (!item)
                         return;
                       const QString effectId =
                           item->data(Qt::UserRole).toString();
                       if (effectId.trimmed().isEmpty())
                         return;
                       if (impl_->moveEffectById(effectId, 1)) {
                         impl_->updateEffectsList();
                         if (impl_->statusLabel) {
                           impl_->statusLabel->setText(
                               QStringLiteral("Status: Effect moved down"));
                         }
                       }
                     });
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::customContextMenuRequested,
        this, [this, i](const QPoint &pos) {
          auto *lw = impl_->racks[i].listWidget;
          if (!lw)
            return;
          QListWidgetItem *item = lw->itemAt(pos);
          impl_->showRackContextMenu(i, item, lw->viewport()->mapToGlobal(pos));
        });
    if (impl_->racks[i].listWidget->viewport()) {
      QObject::connect(impl_->racks[i].listWidget->viewport(),
                       &QWidget::customContextMenuRequested, this,
                       [this, i](const QPoint &pos) {
                         auto *lw = impl_->racks[i].listWidget;
                         if (!lw)
                           return;
                         QListWidgetItem *item = lw->itemAt(pos);
                         impl_->showRackContextMenu(
                             i, item, lw->viewport()->mapToGlobal(pos));
                       });
    }
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem *, QListWidgetItem *) {
          impl_->syncFocusedEffectFromRackSelection();
        });
    QObject::connect(
        impl_->racks[i].listWidget, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem *item) {
          if (!item)
            return;
          const QString effectId = item->data(Qt::UserRole).toString();
          if (effectId.trimmed().isEmpty())
            return;
          const QVariant enabledData = item->data(kEffectRackEnabledRole);
          const bool isEnabled = enabledData.isValid()
                                     ? enabledData.toBool()
                                     : item->text().startsWith(QStringLiteral("Enabled"));
          if (impl_->setEffectEnabledById(effectId, !isEnabled)) {
            impl_->updateEffectRackItemEnabled(effectId, !isEnabled);
            if (impl_->statusLabel) {
              impl_->statusLabel->setText(
                  QStringLiteral("Status: Effect %1")
                      .arg(!isEnabled ? "enabled" : "disabled"));
            }
          }
        });
  }
  stackPanelLayout->addStretch(1);
  auto *effectsSurface =
      new ArtifactEffectTabSurface(stackPanel, detailPanel, impl_->effectsTabWidget);
  effectsLayout->addWidget(effectsSurface, 1);
  QObject::connect(impl_->effectsQuickAddButton, &QPushButton::clicked, this,
                   [this]() { impl_->handleAddEffectClicked(-1); });

  auto *quickAddEffectShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space), this);
  quickAddEffectShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  quickAddEffectShortcut->setObjectName(QStringLiteral("QuickAddEffectShortcut"));
  QObject::connect(quickAddEffectShortcut, &QShortcut::activated, this,
                   [this]() { impl_->handleAddEffectClicked(-1); });

  effectsLayout->setContentsMargins(
      kInspectorSectionMarginL, kInspectorSectionMarginT,
      kInspectorSectionMarginR, kInspectorSectionMarginB);
  effectsLayout->setSpacing(8);

  impl_->effectsTabWidget->setLayout(effectsLayout);
  impl_->effectsScrollArea->setWidget(impl_->effectsTabWidget);
  impl_->tabWidget->addTab(impl_->effectsScrollArea, "Effects");

  // タブをメインレイアウトに追加
  mainLayout->addWidget(impl_->tabWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->containerWidget->setLayout(mainLayout);

  setWidget(impl_->containerWidget);
  setWidgetResizable(true);

  // 初期状態: プロジェクトなし -> 無効化
  impl_->setNoProjectState();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent &) {
            if (!impl_) {
              return;
            }
            impl_->handleProjectCreated();
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent &event) {
            if (!impl_) {
              return;
            }
            const CompositionID cid(event.compositionId);
            impl_->handleCompositionChanged(cid);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent &event) {
            if (!impl_) {
              return;
            }
            const CompositionID cid(event.compositionId);
            // compositionId が nil の場合は既存の currentCompositionId_
            // を上書きしない。 nil を代入すると updateLayerInfo の nil
            // チェックで即 return してしまう。
            if (!cid.isNil()) {
              impl_->currentCompositionId_ = cid;
            } else if (impl_->currentCompositionId_.isNil()) {
              // フォールバック: サービスから直接取得
              if (auto *svc = ArtifactProjectService::instance()) {
                if (auto comp = svc->currentComposition().lock()) {
                  impl_->currentCompositionId_ = comp->id();
                }
              }
            }
            impl_->handleLayerSelected(event);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent &event) {
            if (!impl_ ||
                event.changeType != LayerChangedEvent::ChangeType::Created) {
              return;
            }
            const CompositionID cid(event.compositionId);
            const LayerID lid(event.layerId);
            if (cid.isNil() || lid.isNil())
              return;
            // 追加先コンポジションが現在表示中のコンポジションと一致する場合、追加レイヤーを自動選択
            const bool cidMatches = !impl_->currentCompositionId_.isNil() &&
                                    cid == impl_->currentCompositionId_;
            if (cidMatches) {
              impl_->handleLayerSelected(LayerSelectionChangedEvent{
                  event.compositionId,
                  event.layerId,
                  LayerSelectionChangeReason::SelectionBridgeSync});
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ShowEffectInspectorRequested>(
          [this](const ShowEffectInspectorRequested &) {
            if (!impl_) return;
            if (impl_->tabWidget) {
              impl_->tabWidget->setCurrentIndex(2); // Effects tab
            }
            impl_->containerWidget->show();
            impl_->containerWidget->raise();
          }));
  impl_->refreshRackButtons();
}

ArtifactInspectorWidget::~ArtifactInspectorWidget() { delete impl_; }

QSize ArtifactInspectorWidget::sizeHint() const { return QSize(300, 600); }

void ArtifactInspectorWidget::clear() { update(); }

void ArtifactInspectorWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (!impl_ || !event)
    return;
  impl_->showContextMenu(event->globalPos());
}

} // namespace Artifact
