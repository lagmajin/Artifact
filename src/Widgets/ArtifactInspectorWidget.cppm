module;
#include <QtGui/QIcon>
#include <QMetaType>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
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
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
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
#include <QProxyStyle>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QKeyEvent>
#include <QRadioButton>
#include <QShortcut>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStyleOptionViewItem>
#include <QSplitter>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>
#include <QVariant>
#include <QVector>
#include <QUrl>
#include <QWidget>
#include <Diagnostics/WidgetCreationDiagnostics.hpp>
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
import Artifact.Widgets.TemplateParameters;
import Artifact.Template.Document;
import Widgets.CommonStyle;
import Artifact.Widgets.InspectorStyle;
import Artifact.Widgets.InspectorRasterizerSettings;
import Artifact.Widgets.InspectorSurfaces;
import Artifact.Widgets.InspectorInteraction;
import Artifact.Widgets.Inspector.EffectTabSurface;
import Artifact.Widgets.Inspector.ComponentTabSurface;
import Artifact.Widgets.InspectorEffectCatalog;
import Artifact.Widgets.InspectorEffectPicker;
import Artifact.Widgets.InspectorEffectRackPresentation;
import Artifact.Effect.SurfaceFX;
import Graphics.Effect.SurfaceFX;
import Settings.Accessibility;

import Artifact.Service.Project;
import Artifact.Service.Effect;
import Memory.SharedPtr;
import Clipboard.ClipboardManager;
import Artifact.Project.PresetManager;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Component.System;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Image.ImageF32x4_RGBA;
import Artifact.Widgets.ObjectPicker;
import Artifact.Layer.Matte;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Switch;
import Artifact.Layer.Clone;
import Artifact.Effect.Clone.Core;
import Artifact.Effect.Clone.Basic;
import Artifact.Effect.Clone.Advanced;
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
import Artifact.Effect.Distort.ImageMorph;
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
import Artifact.Effect.Keying.IBKKeyer;
import Artifact.Effect.Rasterizer.DifferenceMatte;
import Artifact.Effect.Wave;
import Artifact.Effect.Spherize;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.AppDialogs;

namespace Artifact {

using namespace ArtifactCore;
using namespace detail;

static QPoint accessibilityMenuPosition(const QMenu &menu,
                                        const QPoint &origin) {
  int x = origin.x();
  int y = origin.y();
  Accessibility::adjustContextMenuPosition(x, y, menu.sizeHint().width());
  return QPoint(x, y);
}

// using namespace ArtifactWidgets;

namespace {
constexpr int kEffectRackCount = 5;
constexpr int kInspectorSectionMarginL = 8;
constexpr int kInspectorSectionMarginT = 8;
constexpr int kInspectorSectionMarginR = 8;
constexpr int kInspectorSectionMarginB = 8;
constexpr int kInspectorSectionSpacing = 4;

class InspectorActionButton final : public QPushButton {
 public:
  explicit InspectorActionButton(const QString& text, QWidget* parent = nullptr)
      : QPushButton(text, parent) {}

  void setOwnerDrawn(bool enabled) {
    setInspectorButtonOwnerDrawn(this, enabled);
  }

  void setAction(std::function<void()> action) {
    setInspectorButtonAction(this, std::move(action));
  }

  void triggerAction() {
    triggerInspectorButtonAction(this);
  }
};

class SelectionActionBlocker final {
 public:
  explicit SelectionActionBlocker(QListWidget* list) : list_(list) {
    setInspectorSelectionActionEnabled(list_, false);
  }
  ~SelectionActionBlocker() {
    setInspectorSelectionActionEnabled(list_, true);
  }

 private:
  QListWidget* list_ = nullptr;
};

constexpr int kInspectorNoteMargin = 6;
constexpr int kInspectorRackMarginL = 6;
constexpr int kInspectorRackMarginT = 10;
constexpr int kInspectorRackMarginR = 6;
constexpr int kInspectorRackMarginB = 6;
constexpr auto kInspectorContext = "Panel.Inspector";

bool applyLayerMaskSnapshotDirect(const ArtifactAbstractLayerPtr &layer,
                                  const std::vector<LayerMask> &masks) {
  if (!layer) {
    return false;
  }
  layer->clearMasks();
  for (const auto &mask : masks) {
    layer->addMask(mask);
  }
  if (layer->maskCount() != static_cast<int>(masks.size())) {
    return false;
  }
  return true;
}

class SurfaceFXElementSnapshotCommand final : public UndoCommand {
 public:
  SurfaceFXElementSnapshotCommand(ArtifactAbstractEffectPtr effect,
                                  ArtifactCore::SurfaceFXData before,
                                  ArtifactCore::SurfaceFXData after,
                                  QString label)
      : effect_(std::move(effect)), before_(std::move(before)),
        after_(std::move(after)), label_(std::move(label)) {}

  void redo() override { lastOperationSucceeded_ = apply(after_, before_); }
  void undo() override { lastOperationSucceeded_ = apply(before_, after_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return label_; }

 private:
  bool apply(const ArtifactCore::SurfaceFXData &data,
             const ArtifactCore::SurfaceFXData &compensation) {
    auto *surface = effect_
        ? dynamic_cast<SurfaceFXEffect *>(effect_.get())
        : nullptr;
    if (!surface) return false;
    surface->setData(data);
    if (surface->data().toJson() != data.toJson()) {
      surface->setData(compensation);
      return false;
    }
    if (auto *manager = UndoManager::instance()) {
      manager->notifyAnythingChanged();
    }
    return true;
  }

  ArtifactAbstractEffectPtr effect_;
  ArtifactCore::SurfaceFXData before_;
  ArtifactCore::SurfaceFXData after_;
  QString label_;
  bool lastOperationSucceeded_ = true;
};

struct LayerTabComponentState {
  bool hasLayer = false;
  bool canEditComponents = false;
  bool physicsEnabled = false;
  bool scriptEnabled = false;
  bool layoutEnabled = false;
  bool cloneEnabled = false;
  bool collisionEnabled = false;
  bool jointEnabled = false;
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
  state.jointEnabled = state.hasLayer &&
      layerBooleanProperty(layer, QStringLiteral("component.joint.enabled"));
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
  if (state.jointEnabled) {
    active.push_back(QStringLiteral("Joint"));
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

bool layerBooleanProperty(const ArtifactAbstractLayerPtr &layer,
                          const QString &propertyPath);

QColor toneColor(detail::LayerPresentationBadgeTone tone, const QColor &base,
                 const QColor &accent) {
  switch (tone) {
  case detail::LayerPresentationBadgeTone::Container:
    return blendColor(base, accent, 0.18);
  case detail::LayerPresentationBadgeTone::Media:
    return blendColor(base, accent, 0.10);
  case detail::LayerPresentationBadgeTone::Motion:
    return blendColor(base, accent.lighter(108), 0.16);
  case detail::LayerPresentationBadgeTone::Special:
    return blendColor(base, accent.darker(108), 0.14);
  case detail::LayerPresentationBadgeTone::Neutral:
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

detail::LayerPresentationBadgeTone toneFromRackIndex(int rackIndex) {
  switch (stageFromRackIndex(rackIndex)) {
  case EffectPipelineStage::Generator:
  case EffectPipelineStage::GeometryTransform:
    return detail::LayerPresentationBadgeTone::Motion;
  case EffectPipelineStage::MaterialRender:
    return detail::LayerPresentationBadgeTone::Media;
  case EffectPipelineStage::Rasterizer:
    return detail::LayerPresentationBadgeTone::Special;
  case EffectPipelineStage::LayerTransform:
    return detail::LayerPresentationBadgeTone::Container;
  default:
    return detail::LayerPresentationBadgeTone::Neutral;
  }
}

constexpr int kEffectRackEnabledRole = Qt::UserRole + 1;
constexpr int kEffectRackHasMaskRole = Qt::UserRole + 2;
constexpr int kEffectRackNameRole = Qt::UserRole + 3;
constexpr int kEffectRackMaskCountRole = Qt::UserRole + 4;

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
      } else if (auto *service = ArtifactProjectService::instance()) {
        if (auto project = service->getCurrentProjectSharedPtr()) {
          std::function<FootageItem*(ProjectItem*)> findInput =
              [&](ProjectItem* item) -> FootageItem* {
            if (!item) return nullptr;
            if (item->id == ref.sourceLayerId &&
                item->type() == eProjectItemType::Footage) {
              auto* footage = static_cast<FootageItem*>(item);
              return footage->assetUsage == ProjectAssetUsage::RenderInput
                  ? footage : nullptr;
            }
            for (auto* child : item->children) {
              if (auto* found = findInput(child)) return found;
            }
            return nullptr;
          };
          for (auto* root : project->projectItems()) {
            if (auto* footage = findInput(root)) {
              sourceName = footage->name.toQString();
              break;
            }
          }
        }
        if (sourceName == QStringLiteral("<missing>") && ref.enabled && hasInvalid) {
          *hasInvalid = true;
        }
      }
    } else if (ref.enabled && hasInvalid) {
      *hasInvalid = true;
    }

    if (ref.enabled && (ref.sourceLayerId == layer->id() || ref.sourceLayerId.isNil())) {
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
  const auto videoLayer = layer ? ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer) : nullptr;
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
    case ProxyQuality::Eighth:
      return QStringLiteral("1/8");
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

bool matteSourceWouldCreateCycle(const ArtifactCompositionPtr& comp,
                                 const ArtifactAbstractLayerPtr& targetLayer,
                                 const LayerID& sourceLayerId) {
  if (!comp || !targetLayer || sourceLayerId.isNil() ||
      targetLayer->id() == sourceLayerId) {
    return true;
  }

  const LayerID targetLayerId = targetLayer->id();
  QSet<QString> visited;
  QVector<LayerID> pending{sourceLayerId};
  while (!pending.isEmpty()) {
    const LayerID currentId = pending.back();
    pending.pop_back();
    if (currentId.isNil()) {
      continue;
    }

    const QString key = currentId.toString();
    if (visited.contains(key)) {
      continue;
    }
    visited.insert(key);
    if (currentId == targetLayerId) {
      return true;
    }

    const auto currentLayer = comp->layerById(currentId);
    if (!currentLayer) {
      continue;
    }
    for (const auto& ref : currentLayer->matteReferences()) {
      if (ref.enabled && !ref.sourceLayerId.isNil()) {
        pending.push_back(ref.sourceLayerId);
      }
    }
  }
  return false;
}

bool applyMatteReferenceChange(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<LayerMatteReference> beforeRefs,
    std::vector<LayerMatteReference> afterRefs) {
  if (!layer) {
    return false;
  }
  if (auto* undo = UndoManager::instance()) {
    return undo->push(std::make_unique<ChangeLayerMatteReferencesCommand>(
        layer, std::move(beforeRefs), std::move(afterRefs)));
  }
  layer->setMatteReferences(afterRefs);
  layer->changed();
  return true;
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

  return applyMatteReferenceChange(layer, std::move(beforeRefs),
                                   std::move(afterRefs));
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
  if (matteSourceWouldCreateCycle(comp, layer, sourceLayerId)) {
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
  ref.sourceAssetPath.clear();
  ref.enabled = true;

  return applyMatteReferenceChange(layer, std::move(beforeRefs),
                                   std::move(afterRefs));
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
  if (matteSourceWouldCreateCycle(comp, layer, sourceLayerId)) {
    return false;
  }

  auto beforeRefs = layer->matteReferences();
  auto afterRefs = beforeRefs;

  LayerMatteReference ref;
  ref.sourceLayerId = sourceLayerId;
  ref.sourceAssetPath.clear();
  ref.enabled = true;
  ref.type = MatteType::Alpha;
  ref.blendMode = MatteBlendMode::Add;
  ref.fitMode = MatteFitMode::Stretch;
  ref.opacity = 1.0f;
  ref.invert = false;
  afterRefs.push_back(ref);

  return applyMatteReferenceChange(layer, std::move(beforeRefs),
                                   std::move(afterRefs));
}

QVector<FootageItem*> projectRenderInputSources() {
  QVector<FootageItem*> result;
  auto* service = ArtifactProjectService::instance();
  auto project = service ? service->getCurrentProjectSharedPtr() : nullptr;
  if (!project) return result;
  std::function<void(ProjectItem*)> collect = [&](ProjectItem* item) {
    if (!item) return;
    if (item->type() == eProjectItemType::Footage) {
      auto* footage = static_cast<FootageItem*>(item);
      if (footage->assetUsage == ProjectAssetUsage::RenderInput) {
        result.push_back(footage);
      }
    }
    for (auto* child : item->children) collect(child);
  };
  for (auto* root : project->projectItems()) collect(root);
  return result;
}

bool setMatteSourceToProjectInput(const CompositionID& compositionId,
                                  const LayerID& layerId,
                                  int matteIndex,
                                  const ArtifactCore::Id& projectItemId,
                                  bool append) {
  auto comp = resolveCompositionForId(compositionId);
  auto layer = comp && !layerId.isNil() ? comp->layerById(layerId) : nullptr;
  if (!layer || projectItemId.isNil()) return false;
  const auto inputs = projectRenderInputSources();
  if (std::none_of(inputs.begin(), inputs.end(), [&](const FootageItem* footage) {
        return footage && footage->id == projectItemId;
      })) {
    return false;
  }
  auto beforeRefs = layer->matteReferences();
  auto afterRefs = beforeRefs;
  if (append) {
    LayerMatteReference ref;
    ref.sourceLayerId = projectItemId;
    const auto matched = std::find_if(inputs.begin(), inputs.end(), [&](const FootageItem* footage) {
      return footage && footage->id == projectItemId;
    });
    ref.sourceAssetPath = matched != inputs.end() ? (*matched)->filePath : QString();
    ref.type = matched != inputs.end() &&
                       (*matched)->renderInputRole == ProjectRenderInputRole::LumaMatte
                   ? MatteType::Luma
                   : MatteType::Alpha;
    afterRefs.push_back(ref);
  } else {
    if (matteIndex < 0 || matteIndex >= static_cast<int>(afterRefs.size())) return false;
    afterRefs[matteIndex].sourceLayerId = projectItemId;
    const auto matched = std::find_if(inputs.begin(), inputs.end(), [&](const FootageItem* footage) {
      return footage && footage->id == projectItemId;
    });
    afterRefs[matteIndex].sourceAssetPath =
        matched != inputs.end() ? (*matched)->filePath : QString();
    if (matched != inputs.end()) {
      if ((*matched)->renderInputRole == ProjectRenderInputRole::LumaMatte) {
        afterRefs[matteIndex].type = MatteType::Luma;
      } else if ((*matched)->renderInputRole == ProjectRenderInputRole::AlphaMatte) {
        afterRefs[matteIndex].type = MatteType::Alpha;
      }
    }
    afterRefs[matteIndex].enabled = true;
  }
  return applyMatteReferenceChange(layer, std::move(beforeRefs),
                                   std::move(afterRefs));
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

  return applyMatteReferenceChange(layer, std::move(beforeRefs),
                                   std::move(afterRefs));
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
      if (event->button() != Qt::RightButton) {
        QLabel::mousePressEvent(event);
        return;
      }
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
        const auto inputSources = projectRenderInputSources();
        if (!inputSources.isEmpty()) {
          QMenu* inputMenu = menu.addMenu(QStringLiteral("Use Input Source"));
          for (const auto* footage : inputSources) {
            if (!footage) continue;
            QAction* inputAction = inputMenu->addAction(footage->name.toQString());
            inputAction->setData(QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("input_append")},
                {QStringLiteral("projectItemId"), footage->id.toString()}});
          }
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
            const auto inputSources = projectRenderInputSources();
            const auto found = std::find_if(
                inputSources.begin(), inputSources.end(), [&](const FootageItem* footage) {
                  return footage && footage->id == ref.sourceLayerId;
                });
            sourceName = found != inputSources.end()
                ? (*found)->name.toQString()
                : ref.sourceLayerId.toString();
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

        const auto inputSources = projectRenderInputSources();
        if (!inputSources.isEmpty()) {
          QMenu* inputMenu = refMenu->addMenu(QStringLiteral("Use Input Source"));
          for (const auto* footage : inputSources) {
            if (!footage) continue;
            QAction* inputAction = inputMenu->addAction(footage->name.toQString());
            inputAction->setData(QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("input_replace")},
                {QStringLiteral("index"), i},
                {QStringLiteral("projectItemId"), footage->id.toString()}});
          }
        }

        QMenu *typeMenu = refMenu->addMenu(QStringLiteral("Set matte type"));
        for (int typeIndex = 0; typeIndex < typeLabels.size(); ++typeIndex) {
          QAction *typeAction = typeMenu->addAction(typeLabels[typeIndex]);
          typeAction->setData(QVariantMap{{QStringLiteral("kind"), QStringLiteral("type")},
                                          {QStringLiteral("index"), i},
                                          {QStringLiteral("type"), typeIndex}});
        }
      }

      if (QAction *chosen = menu.exec(
              accessibilityMenuPosition(menu, QCursor::pos()))) {
        const QVariantMap data = chosen->data().toMap();
        const QString kind = data.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("add_selected")) {
          const auto selectedLayerId = LayerID(data.value(QStringLiteral("selectedLayerId")).toString());
          addMatteSourceToLayer(compositionId_, layerId_, selectedLayerId);
          event->accept();
          return;
        }
        if (kind == QStringLiteral("input_append")) {
          setMatteSourceToProjectInput(
              compositionId_, layerId_, -1,
              ArtifactCore::Id(data.value(QStringLiteral("projectItemId")).toString()), true);
          event->accept();
          return;
        }
        bool indexOk = false;
        const int index = data.value(QStringLiteral("index")).toInt(&indexOk);
        if (!indexOk) {
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
        } else if (kind == QStringLiteral("input_replace")) {
          setMatteSourceToProjectInput(
              compositionId_, layerId_, index,
              ArtifactCore::Id(data.value(QStringLiteral("projectItemId")).toString()), false);
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
    const bool hasProxy = layer && ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer) &&
                          ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)->hasProxy();
    setCursor(hasProxy ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (!event) {
      QLabel::mousePressEvent(event);
      return;
    }
    if (event->button() == Qt::LeftButton) {
      const auto videoLayer = layer_ ? ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer_) : nullptr;
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

} // namespace

W_OBJECT_IMPL(ArtifactInspectorWidget)

class ArtifactInspectorWidget::Impl {
private:
public:
  Impl();
  ~Impl();
  QWidget *containerWidget = nullptr;
  QTabWidget *tabWidget = nullptr;
  ArtifactTemplateParametersWidget *templateParametersWidget = nullptr;

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
  QString focusedGeneratorId_;
  QString focusedTransformId_;
  QString focusedModifierId_;
  LayerID focusedComponentLayerId_;
  bool componentEditorExpanded_ = false;
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
  QListWidget *generatorListWidget = nullptr;
  InspectorActionButton *transformComponentButton = nullptr;
  InspectorActionButton *removeTransformComponentButton = nullptr;
  InspectorActionButton *transformDuplicateButton = nullptr;
  InspectorActionButton *transformMoveUpButton = nullptr;
  InspectorActionButton *transformMoveDownButton = nullptr;
  QListWidget *transformListWidget = nullptr;
  InspectorActionButton *fieldComponentButton = nullptr;
  InspectorActionButton *removeFieldComponentButton = nullptr;
  InspectorActionButton *fieldMoveUpButton = nullptr;
  InspectorActionButton *fieldMoveDownButton = nullptr;
  QListWidget *fieldListWidget = nullptr;
  InspectorActionButton *cloneModifierButton = nullptr;
  InspectorActionButton *removeCloneModifierButton = nullptr;
  InspectorActionButton *cloneModifierMoveUpButton = nullptr;
  InspectorActionButton *cloneModifierMoveDownButton = nullptr;
  QListWidget *cloneModifierListWidget = nullptr;
  QWidget *clonerStructureWidget = nullptr;
  QLabel *componentUtilitiesLabel = nullptr;
  InspectorActionButton *openScriptButton = nullptr;
  InspectorActionButton *applyLipSyncButton = nullptr;
  InspectorActionButton *addEffectorButton = nullptr;
  InspectorActionButton *removeEffectorButton = nullptr;
  ArtifactPropertyWidget *componentPropertyWidget = nullptr;
  QWidget *componentPropertySurface = nullptr;
  QString lastComponentPropertyStateSignature_;
  QLabel *statusLabel = nullptr;

  // Effects Pipeline Tab
  QScrollArea *effectsScrollArea = nullptr;
  QWidget *effectsTabWidget = nullptr;
  QLabel *effectsStateLabel = nullptr;
  QLabel *effectsTargetLabel = nullptr;
  QLabel *effectsStackSummaryLabel = nullptr;
  QLineEdit *effectPropertyFilterEdit = nullptr;
  QTabWidget *effectsModeTabs = nullptr;
  QLabel *effectEditorTitleLabel = nullptr;
  QLabel *effectParametersHintLabel = nullptr;
  InspectorActionButton *effectEnableButton = nullptr;
  ArtifactPropertyWidget *effectPropertyWidget = nullptr;
  QWidget *effectPropertySurface = nullptr;
  QWidget *surfaceElementPanel = nullptr;
  QListWidget *surfaceElementListWidget = nullptr;
  int surfaceElementIndex_ = 0;
  QPushButton *effectsQuickAddButton = nullptr;
  QString focusedEffectId_;
  ArtifactAbstractLayerPtr lastSyncedLayer_;
  QString lastSyncedFocusedEffectId_;
  QString lastEffectPropertyStateSignature_;

  struct EffectRack {
    QWidget *groupBox = nullptr;
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
  void ensureComponentPropertyWidget();
  void updateEffectsList();
  void addSelectedEffectToCurrentTarget(const QString &effectId);
  void updateEffectRackItemEnabled(const QString &effectId, bool enabled);
  void updatePropertiesForEffect(const QString &effectId);
  QString currentSelectedEffectIdFromRacks() const;
  void syncFocusedEffectFromRackSelection();
  void syncEffectPropertyWidget();
  void syncTemplateParameters();
  void ensureEffectPropertyWidget();
  void updateSurfaceElementEditor(const ArtifactAbstractEffectPtr &effect);
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
           syncTemplateParameters();
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

void ArtifactInspectorWidget::Impl::ensureEffectPropertyWidget() {
  if (effectPropertyWidget || !effectPropertySurface) {
    return;
  }

  effectPropertyWidget = new ArtifactPropertyWidget(effectPropertySurface);
  effectPropertyWidget->setVisible(false);
  effectPropertyWidget->setMinimumHeight(220);
  applyInspectorOwnerDrawScrollBars(effectPropertyWidget);
  effectPropertyWidget->setFilterText(
      effectPropertyFilterEdit ? effectPropertyFilterEdit->text() : QString());
  setInspectorPropertySurfaceEditor(effectPropertySurface, effectPropertyWidget);
}

void ArtifactInspectorWidget::Impl::updateSurfaceElementEditor(
    const ArtifactAbstractEffectPtr &effect) {
  if (!surfaceElementPanel || !surfaceElementListWidget) {
    return;
  }
  const auto *surface = effect
      ? dynamic_cast<const SurfaceFXEffect *>(effect.get())
      : nullptr;
  if (!surface) {
    surfaceElementPanel->setVisible(false);
    return;
  }

  surfaceElementPanel->setVisible(true);
  SelectionActionBlocker blocker(surfaceElementListWidget);
  surfaceElementListWidget->clear();
  const auto &elements = surface->data().elements;
  const auto elementTypeLabel = [](ArtifactCore::SurfaceFXElementType type) {
    switch (type) {
    case ArtifactCore::SurfaceFXElementType::Droplet:
      return QStringLiteral("Droplet");
    case ArtifactCore::SurfaceFXElementType::Streak:
      return QStringLiteral("Streak");
    case ArtifactCore::SurfaceFXElementType::Condensation:
      return QStringLiteral("Condensation");
    case ArtifactCore::SurfaceFXElementType::Dirt:
      return QStringLiteral("Dirt");
    case ArtifactCore::SurfaceFXElementType::TextureDecal:
      return QStringLiteral("Texture Decal");
    case ArtifactCore::SurfaceFXElementType::Scratch:
    default:
      return QStringLiteral("Scratch");
    }
  };
  for (int index = 0; index < static_cast<int>(elements.size()); ++index) {
    const auto &element = elements[static_cast<std::size_t>(index)];
    QString label = element.id.trimmed();
    if (label.isEmpty()) {
      label = QStringLiteral("Element %1").arg(index + 1);
    }
    auto *item = new QListWidgetItem(
        QStringLiteral("%1  ·  %2").arg(label, elementTypeLabel(element.type)),
        surfaceElementListWidget);
    item->setData(Qt::UserRole, index);
  }
  if (surfaceElementListWidget->count() == 0) {
    surfaceElementIndex_ = 0;
    return;
  }
  surfaceElementIndex_ = std::clamp(surfaceElementIndex_, 0,
                                    surfaceElementListWidget->count() - 1);
  surfaceElementListWidget->setCurrentRow(surfaceElementIndex_);
}

void ArtifactInspectorWidget::Impl::syncEffectPropertyWidget() {
  if (!effectPropertyWidget && focusedEffectId_.trimmed().isEmpty()) {
    if (effectPropertySurface) {
      effectPropertySurface->setVisible(false);
    }
    if (effectEnableButton) {
      effectEnableButton->setVisible(false);
      effectEnableButton->setEnabled(false);
    }
    if (effectEditorTitleLabel) {
      effectEditorTitleLabel->setText(QStringLiteral("Select an effect to inspect"));
    }
    if (effectParametersHintLabel) {
      effectParametersHintLabel->setText(
          QStringLiteral("Select an effect above to reveal its parameters here."));
      effectParametersHintLabel->setVisible(true);
    }
    return;
  }
  ensureEffectPropertyWidget();
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
    if (surfaceElementPanel) {
      surfaceElementPanel->setVisible(false);
    }
    effectPropertyWidget->setVisible(showPropertyWidget);
    if (effectPropertySurface) {
      effectPropertySurface->setVisible(showPropertyWidget);
    }
    if (effectEnableButton) {
      effectEnableButton->setVisible(showPropertyWidget);
      effectEnableButton->setEnabled(showPropertyWidget);
    }
    if (effectEditorTitleLabel) {
      effectEditorTitleLabel->setText(showPropertyWidget
                                          ? QStringLiteral("Effect Editor")
                                          : QStringLiteral("Select an effect to inspect"));
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
    updateSurfaceElementEditor(effect);
    effectPropertyWidget->setFilterText(
        effectPropertyFilterEdit ? effectPropertyFilterEdit->text() : QString());
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
  updateSurfaceElementEditor(currentEffectById(resolvedFocusedEffectId));
  effectPropertyWidget->setFilterText(
      effectPropertyFilterEdit ? effectPropertyFilterEdit->text() : QString());

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

void ArtifactInspectorWidget::Impl::syncTemplateParameters() {
  if (!templateParametersWidget) return;
  if (currentLayerId_.isNil() || currentCompositionId_.isNil()) {
    templateParametersWidget->setParameters(QJsonArray{});
    return;
  }
  auto* service = ArtifactProjectService::instance();
  if (!service) {
    templateParametersWidget->setParameters(QJsonArray{});
    return;
  }
  const auto result = service->findComposition(currentCompositionId_);
  const auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
  const auto layer = composition ? composition->layerById(currentLayerId_)
                                 : ArtifactAbstractLayerPtr{};
  if (!layer) {
    templateParametersWidget->setParameters(QJsonArray{});
    return;
  }
  const auto document = ArtifactTemplateDocument::fromLayers(
      QVector<ArtifactAbstractLayerPtr>{layer}, layer->layerName());
  templateParametersWidget->setDocument(document);
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
        "component.collision.enabled|component.joint.enabled|"
        "component.crowd.enabled|"
        "component.particleEmitter.enabled|component.fluid.enabled");
  }
  QStringList filters = {
      QStringLiteral("physics.enabled"),
      QStringLiteral("component.script.enabled"),
      QStringLiteral("component.layout.enabled"),
      QStringLiteral("component.cloner.enabled"),
      QStringLiteral("component.collision.enabled"),
      QStringLiteral("component.joint.enabled"),
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
  if (layerBooleanProperty(layer, QStringLiteral("component.joint.enabled"))) {
    filters.push_back(QStringLiteral("component.joint."));
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
  if (componentName == QStringLiteral("Joint")) {
    return QStringLiteral("component.joint.");
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

bool hasReorderableItemBefore(const QListWidget *list, int row) {
  if (!list || row <= 0) {
    return false;
  }
  for (int index = row - 1; index >= 0; --index) {
    if (generatorItemSupportsReorder(list->item(index))) {
      return true;
    }
  }
  return false;
}

bool hasReorderableItemAfter(const QListWidget *list, int row) {
  if (!list || row < 0) {
    return false;
  }
  for (int index = row + 1; index < list->count(); ++index) {
    if (generatorItemSupportsReorder(list->item(index))) {
      return true;
    }
  }
  return false;
}

QString transformItemFilterText(const QListWidgetItem *item) {
  if (!item) {
    return {};
  }
  return item->data(Qt::UserRole + 1).toString().trimmed();
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
    focusedGeneratorId_.clear();
    focusedTransformId_.clear();
    focusedModifierId_.clear();
    focusedComponentLayerId_ = LayerID{};
    componentEditorExpanded_ = false;
  }
  if (focusedComponentName_ == QStringLiteral("Generator") &&
      state.generatorCount > 0) {
    activeName = focusedComponentName_;
  } else if (focusedComponentName_ == QStringLiteral("Transform") &&
             state.cloneEnabled && !focusedTransformId_.isEmpty()) {
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
        activeName == QStringLiteral("Transform") ||
        activeName == QStringLiteral("Field") ||
        activeName == QStringLiteral("Clone Modifier");
    clonerStructureWidget->setVisible(
        componentEditorExpanded_ && hasLayer && state.cloneEnabled &&
        showsClonerStructure);
  }
  if (activeComponentLabel) {
    activeComponentLabel->setText(
        activeName.isEmpty()
            ? QStringLiteral("Active Component  |  None")
            : QStringLiteral("Active Component  |  %1").arg(activeName));
    activeComponentLabel->setVisible(false);
  }
  if (physicsComponentButton) {
    physicsComponentButton->setEnabled(canEditComponents);
    physicsComponentButton->setChecked(
        componentEditorExpanded_ && activeName == QStringLiteral("Physics"));
    physicsComponentButton->setProperty("artifactComponentEnabled",
                                        state.physicsEnabled);
    physicsComponentButton->setText(QStringLiteral("Physics"));
    applyInspectorComponentStateButton(
        physicsComponentButton,
        componentEditorExpanded_ && activeName == QStringLiteral("Physics"));
    physicsComponentButton->setVisible(state.physicsEnabled);
    physicsComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Physics component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Physics."));
  }
  if (scriptComponentButton) {
    scriptComponentButton->setEnabled(canEditComponents);
    scriptComponentButton->setChecked(
        componentEditorExpanded_ && activeName == QStringLiteral("Script"));
    scriptComponentButton->setProperty("artifactComponentEnabled",
                                       state.scriptEnabled);
    scriptComponentButton->setText(QStringLiteral("Script"));
    applyInspectorComponentStateButton(
        scriptComponentButton,
        componentEditorExpanded_ && activeName == QStringLiteral("Script"));
    scriptComponentButton->setVisible(state.scriptEnabled);
    scriptComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Script component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Script."));
  }
  if (layoutComponentButton) {
    layoutComponentButton->setEnabled(canEditComponents);
    layoutComponentButton->setChecked(
        componentEditorExpanded_ && activeName == QStringLiteral("Layout"));
    layoutComponentButton->setProperty("artifactComponentEnabled",
                                       state.layoutEnabled);
    layoutComponentButton->setText(QStringLiteral("Layout"));
    applyInspectorComponentStateButton(
        layoutComponentButton,
        componentEditorExpanded_ && activeName == QStringLiteral("Layout"));
    layoutComponentButton->setVisible(state.layoutEnabled);
    layoutComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Layout component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Layout."));
  }
  if (cloneComponentButton) {
    cloneComponentButton->setEnabled(canEditComponents);
    cloneComponentButton->setChecked(
        componentEditorExpanded_ && activeName == QStringLiteral("Cloner"));
    cloneComponentButton->setProperty("artifactComponentEnabled",
                                      state.cloneEnabled);
    cloneComponentButton->setText(QStringLiteral("Cloner"));
    applyInspectorComponentStateButton(
        cloneComponentButton,
        componentEditorExpanded_ && activeName == QStringLiteral("Cloner"));
    cloneComponentButton->setVisible(state.cloneEnabled);
    cloneComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Cloner component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Cloner."));
  }
  if (fluidComponentButton) {
    fluidComponentButton->setEnabled(canEditComponents);
    fluidComponentButton->setChecked(
        componentEditorExpanded_ && activeName == QStringLiteral("Fluid"));
    fluidComponentButton->setProperty("artifactComponentEnabled",
                                      state.fluidEnabled);
    fluidComponentButton->setText(QStringLiteral("Fluid"));
    applyInspectorComponentStateButton(
        fluidComponentButton,
        componentEditorExpanded_ && activeName == QStringLiteral("Fluid"));
    fluidComponentButton->setVisible(state.fluidEnabled);
    fluidComponentButton->setToolTip(
        canEditComponents ? QStringLiteral("Show the Fluid component settings.")
                          : QStringLiteral("Select a layer inside a composition to add Fluid."));
  }
  if (transformComponentButton) {
    transformComponentButton->setEnabled(canEditComponents && state.cloneEnabled);
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
  const auto* inspectorCloneLayer =
      (canEditComponents && layer) ? dynamic_cast<const ArtifactCloneLayer*>(layer.get()) : nullptr;
  if (addEffectorButton) {
    const int effectorCount = inspectorCloneLayer ? inspectorCloneLayer->effectorCount() : 0;
    addEffectorButton->setEnabled(inspectorCloneLayer != nullptr);
    addEffectorButton->setText(
        effectorCount > 0 ? QStringLiteral("+ Effector (%1)").arg(effectorCount)
                          : QStringLiteral("+ Effector"));
    addEffectorButton->setToolTip(
        canEditComponents
            ? QStringLiteral("Add an effector to this layer's clone chain.")
            : QStringLiteral("Select a clone layer to add Effectors."));
    addEffectorButton->setVisible(inspectorCloneLayer != nullptr);
  }
  if (removeEffectorButton) {
    removeEffectorButton->setEnabled(
        inspectorCloneLayer && inspectorCloneLayer->effectorCount() > 0);
    removeEffectorButton->setVisible(inspectorCloneLayer != nullptr);
  }
  if (removeTransformComponentButton && !transformListWidget) {
    removeTransformComponentButton->setEnabled(false);
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
            : focusedGeneratorId_;
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
          currentIsExtra &&
          hasReorderableItemBefore(generatorListWidget,
                                   generatorListWidget->currentRow()));
    }
    if (generatorMoveDownButton) {
      generatorMoveDownButton->setEnabled(
          currentIsExtra &&
          hasReorderableItemAfter(generatorListWidget,
                                  generatorListWidget->currentRow()));
    }
  }
  if (transformListWidget) {
    SelectionActionBlocker blocker(transformListWidget);
    const QString selectedTransformId =
        transformListWidget->currentItem()
            ? transformListWidget->currentItem()->data(Qt::UserRole).toString()
            : focusedTransformId_;
    transformListWidget->clear();
    if (hasLayer && state.cloneEnabled) {
      const auto names = layer->clonerTransformNames();
      int restoredRow = -1;
      for (int row = 0; row < static_cast<int>(names.size()); ++row) {
        auto* item = new QListWidgetItem(names[static_cast<std::size_t>(row)],
                                         transformListWidget);
        const QString transformId = QStringLiteral("transform.%1").arg(row);
        item->setData(Qt::UserRole, transformId);
        item->setData(Qt::UserRole + 1,
                      QStringLiteral("component.cloner.transforms.%1.").arg(row));
        item->setData(Qt::UserRole + 2, true);
        item->setToolTip(transformId);
        if (transformId == selectedTransformId) {
          restoredRow = row;
        }
      }
      if (transformListWidget->count() > 0) {
        transformListWidget->setCurrentRow(restoredRow >= 0 ? restoredRow : 0);
      }
    }
    transformListWidget->setVisible(transformListWidget->count() > 0);
    const auto* currentItem = transformListWidget->currentItem();
    const bool hasSelectedTransform = canEditComponents && currentItem;
    if (removeTransformComponentButton) {
      removeTransformComponentButton->setEnabled(hasSelectedTransform);
    }
    if (transformDuplicateButton) {
      transformDuplicateButton->setEnabled(hasSelectedTransform);
    }
    if (transformMoveUpButton) {
      transformMoveUpButton->setEnabled(
          hasSelectedTransform &&
          hasReorderableItemBefore(transformListWidget,
                                   transformListWidget->currentRow()));
    }
    if (transformMoveDownButton) {
      transformMoveDownButton->setEnabled(
          hasSelectedTransform &&
          hasReorderableItemAfter(transformListWidget,
                                  transformListWidget->currentRow()));
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
            : focusedModifierId_;
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
          currentIsExtra &&
          hasReorderableItemBefore(cloneModifierListWidget,
                                   cloneModifierListWidget->currentRow()));
    }
    if (cloneModifierMoveDownButton) {
      cloneModifierMoveDownButton->setEnabled(
          currentIsExtra &&
          hasReorderableItemAfter(cloneModifierListWidget,
                                  cloneModifierListWidget->currentRow()));
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
        hasLayer && !state.validationIssues.empty());
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
  } else if (activeName == QStringLiteral("Transform")) {
    desiredComponentFilter = transformItemFilterText(
        transformListWidget ? transformListWidget->currentItem() : nullptr);
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
    const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer);
    const bool canShow = static_cast<bool>(audioLayer);
    bool canApply = false;
    ArtifactCore::SharedPtr<ArtifactSwitchLayer> switchTarget;
    if (audioLayer) {
      auto *projectService = ArtifactProjectService::instance();
      auto *selMgr = ArtifactLayerSelectionManager::instance();
      const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
      for (const auto &selectedLayer : selected) {
        if (!selectedLayer || selectedLayer == layer) {
          continue;
        }
        switchTarget = ArtifactCore::dynamicPointerCast<ArtifactSwitchLayer>(selectedLayer);
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
          switchTarget = ArtifactCore::dynamicPointerCast<ArtifactSwitchLayer>(candidate);
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
    componentUtilitiesLabel->setVisible(false);
  }
}

void ArtifactInspectorWidget::Impl::syncComponentPropertyWidget(
    const ArtifactAbstractLayerPtr &layer, const QString &filterText) {
  if (!layer) {
    lastComponentPropertyStateSignature_.clear();
    componentEditorExpanded_ = false;
    if (componentPropertyWidget) {
      componentPropertyWidget->clear();
      componentPropertyWidget->setVisible(false);
    }
    if (componentPropertySurface) {
      componentPropertySurface->setVisible(false);
    }
    return;
  }

  ensureComponentPropertyWidget();
  if (!componentPropertyWidget) {
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
  componentEditorExpanded_ = true;
  if (componentPropertySurface) {
    componentPropertySurface->setVisible(true);
  }
  updateComponentControls(layer);
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

void ArtifactInspectorWidget::Impl::ensureComponentPropertyWidget() {
  if (componentPropertyWidget || !componentPropertySurface) {
    return;
  }

  componentPropertyWidget = new ArtifactPropertyWidget(componentPropertySurface);
  componentPropertyWidget->setProperty("artifactEmbeddedComponentEditor", true);
  componentPropertyWidget->setVisible(false);
  componentPropertyWidget->setMinimumHeight(120);
  applyInspectorOwnerDrawScrollBars(componentPropertyWidget);
  componentPropertyWidget->setFilterText(QString());
  setInspectorPropertySurfaceEditor(componentPropertySurface, componentPropertyWidget);
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
    std::vector<LayerMask> before;
    std::vector<LayerMask> after;
    before.reserve(static_cast<std::size_t>(layer->maskCount()));
    after.reserve(static_cast<std::size_t>(layer->maskCount() + 1));
    for (int index = 0; index < layer->maskCount(); ++index) {
      before.push_back(layer->mask(index));
    }
    if (choice != QMessageBox::Yes) {
      after = before;
    }
    after.push_back(mask);
    if (auto *mgr = UndoManager::instance()) {
      if (!mgr->push(std::make_unique<MaskEditCommand>(
              layer, std::move(before), std::move(after)))) {
        return;
      }
    } else {
      if (choice == QMessageBox::Yes) {
        layer->clearMasks();
      }
      layer->addMask(mask);
    }
    layer->changed();
  });

  if (!currentLayerId_.isNil() && !currentCompositionId_.isNil()) {
    auto *projectService = ArtifactProjectService::instance();
    ArtifactCompositionPtr comp;
    if (projectService) {
      const auto findResult = projectService->findComposition(currentCompositionId_);
      if (findResult.success) {
        comp = findResult.ptr.lock();
      }
    }
    auto layer = comp ? comp->layerById(currentLayerId_)
                      : ArtifactAbstractLayerPtr{};
    if (layer && layer->maskCount() > 1) {
      menu.addSeparator();
      menu.addAction(QStringLiteral("Enable All Masks"), [layer]() {
        std::vector<LayerMask> before;
        std::vector<LayerMask> after;
        before.reserve(static_cast<std::size_t>(layer->maskCount()));
        after.reserve(static_cast<std::size_t>(layer->maskCount()));
        bool changed = false;
        for (int i = 0; i < layer->maskCount(); ++i) {
          const auto mask = layer->mask(i);
          auto next = mask;
          next.setEnabled(true);
          changed = changed || !mask.isEnabled();
          before.push_back(mask);
          after.push_back(next);
        }
        if (changed) {
          auto *mgr = UndoManager::instance();
          if (mgr) {
            if (!mgr->push(std::make_unique<MaskEditCommand>(
                    layer, std::move(before), std::move(after)))) {
              return;
            }
          } else if (!applyLayerMaskSnapshotDirect(layer, after)) {
            return;
          }
          layer->changed();
        }
      });
      menu.addAction(QStringLiteral("Disable All Masks"), [layer]() {
        std::vector<LayerMask> before;
        std::vector<LayerMask> after;
        before.reserve(static_cast<std::size_t>(layer->maskCount()));
        after.reserve(static_cast<std::size_t>(layer->maskCount()));
        bool changed = false;
        for (int i = 0; i < layer->maskCount(); ++i) {
          const auto mask = layer->mask(i);
          auto next = mask;
          next.setEnabled(false);
          changed = changed || mask.isEnabled();
          before.push_back(mask);
          after.push_back(next);
        }
        if (changed) {
          auto *mgr = UndoManager::instance();
          if (mgr) {
            if (!mgr->push(std::make_unique<MaskEditCommand>(
                    layer, std::move(before), std::move(after)))) {
              return;
            }
          } else if (!applyLayerMaskSnapshotDirect(layer, after)) {
            return;
          }
          layer->changed();
        }
      });
      menu.addAction(QStringLiteral("Invert All Mask States"), [layer]() {
        std::vector<LayerMask> before;
        std::vector<LayerMask> after;
        before.reserve(static_cast<std::size_t>(layer->maskCount()));
        after.reserve(static_cast<std::size_t>(layer->maskCount()));
        for (int i = 0; i < layer->maskCount(); ++i) {
          const auto mask = layer->mask(i);
          auto next = mask;
          next.setEnabled(!mask.isEnabled());
          before.push_back(mask);
          after.push_back(next);
        }
        auto *mgr = UndoManager::instance();
        if (mgr) {
          if (!mgr->push(std::make_unique<MaskEditCommand>(
                  layer, std::move(before), std::move(after)))) {
            return;
          }
        } else if (!applyLayerMaskSnapshotDirect(layer, after)) {
          return;
        }
        layer->changed();
      });
      const auto addMaskModeAction = [&menu, layer](const QString &label,
                                                     const MaskMode mode) {
        menu.addAction(label, [layer, mode]() {
          std::vector<LayerMask> before;
          std::vector<LayerMask> after;
          before.reserve(static_cast<std::size_t>(layer->maskCount()));
          after.reserve(static_cast<std::size_t>(layer->maskCount()));
          bool changed = false;
          for (int maskIndex = 0; maskIndex < layer->maskCount(); ++maskIndex) {
            const auto mask = layer->mask(maskIndex);
            auto next = mask;
            for (int pathIndex = 0; pathIndex < next.maskPathCount(); ++pathIndex) {
              auto path = next.maskPath(pathIndex);
              changed = changed || path.mode() != mode;
              path.setMode(mode);
              next.setMaskPath(pathIndex, path);
            }
            before.push_back(mask);
            after.push_back(next);
          }
          if (!changed) {
            return;
          }
          auto *mgr = UndoManager::instance();
          if (mgr) {
            if (!mgr->push(std::make_unique<MaskEditCommand>(
                    layer, std::move(before), std::move(after)))) {
              return;
            }
          } else if (!applyLayerMaskSnapshotDirect(layer, after)) {
            return;
          }
          layer->changed();
        });
      };
      addMaskModeAction(QStringLiteral("Set All Mask Paths: Add"), MaskMode::Add);
      addMaskModeAction(QStringLiteral("Set All Mask Paths: Subtract"), MaskMode::Subtract);
      addMaskModeAction(QStringLiteral("Set All Mask Paths: Intersect"), MaskMode::Intersect);
      addMaskModeAction(QStringLiteral("Set All Mask Paths: Difference"), MaskMode::Difference);
      for (int index = 0; index < layer->maskCount(); ++index) {
        if (index > 0) {
          const int targetIndex = index - 1;
          menu.addAction(QStringLiteral("Mask %1 Up").arg(index + 1),
                         [layer, index, targetIndex]() {
                            auto *mgr = UndoManager::instance();
                            if (mgr) {
                              if (!mgr->push(std::make_unique<MoveMaskCommand>(
                                      layer, index, targetIndex))) {
                                return;
                              }
                            } else if (!layer->moveMask(index, targetIndex)) {
                              return;
                            }
                            layer->changed();
                         });
        }
        if (index + 1 < layer->maskCount()) {
          const int targetIndex = index + 1;
          menu.addAction(QStringLiteral("Mask %1 Down").arg(index + 1),
                         [layer, index, targetIndex]() {
                            auto *mgr = UndoManager::instance();
                            if (mgr) {
                              if (!mgr->push(std::make_unique<MoveMaskCommand>(
                                      layer, index, targetIndex))) {
                                return;
                              }
                            } else if (!layer->moveMask(index, targetIndex)) {
                              return;
                            }
                            layer->changed();
                         });
        }
      }
    }
  }
  menu.exec(accessibilityMenuPosition(menu, globalPos));
}

void ArtifactInspectorWidget::Impl::showRackContextMenu(
    int rackIndex, QListWidgetItem *item, const QPoint &globalPos) {
  QMenu menu;

  if (!item) {
    menu.addAction("Refresh Inspector", [this]() {
      updateLayerInfo();
      updateEffectsList();
    });
    menu.exec(accessibilityMenuPosition(menu, globalPos));
    return;
  }

  const QString effectId = item->data(Qt::UserRole).toString();
  if (effectId.isEmpty()) {
    menu.exec(accessibilityMenuPosition(menu, globalPos));
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

  QAction *copyEffectAction = menu.addAction(QStringLiteral("Copy Effect"));
  QObject::connect(copyEffectAction, &QAction::triggered,
                   [this, effect, effectId]() {
                     if (!effect) return;
                     QJsonObject effectJson;
                     effectJson[QStringLiteral("id")] = effectId;
                     effectJson[QStringLiteral("displayName")] =
                         effect->displayName().toQString();
                     effectJson[QStringLiteral("enabled")] = effect->isEnabled();
                     effectJson[QStringLiteral("mix")] = effect->mix();
                     effectJson[QStringLiteral("allowOverscan")] = effect->allowOverscan();
                     effectJson[QStringLiteral("pipelineStage")] =
                         static_cast<int>(effect->pipelineStage());
                     effectJson[QStringLiteral("computeMode")] =
                         static_cast<int>(effect->computeMode());
                     if (effect->hasEffectRegion()) {
                       const QRectF region = effect->effectRegion();
                       QJsonObject regionJson;
                       regionJson[QStringLiteral("x")] = region.x();
                       regionJson[QStringLiteral("y")] = region.y();
                       regionJson[QStringLiteral("width")] = region.width();
                       regionJson[QStringLiteral("height")] = region.height();
                       effectJson[QStringLiteral("effectRegion")] = regionJson;
                     }
                     QJsonArray properties;
                     for (const auto &property : effect->getProperties()) {
                       QJsonObject propertyObject;
                       propertyObject[QStringLiteral("name")] = property.getName();
                       propertyObject[QStringLiteral("value")] =
                           QJsonValue::fromVariant(property.getValue());
                       properties.append(propertyObject);
                     }
                     effectJson[QStringLiteral("properties")] = properties;
                     ArtifactCore::ClipboardManager::instance().copyEffect(
                         effectJson, effect->displayName().toQString(),
                         currentLayerId_.toString());
                   });

  auto &clipboard = ArtifactCore::ClipboardManager::instance();
  clipboard.syncFromSystemClipboard();
  QAction *pasteEffectAction = menu.addAction(QStringLiteral("Paste Effect"));
  pasteEffectAction->setEnabled(clipboard.hasEffectData() &&
                                 !currentLayerId_.isNil());
  QObject::connect(pasteEffectAction, &QAction::triggered,
                   [this]() {
                     if (currentLayerId_.isNil()) return;
                     auto &clipboard = ArtifactCore::ClipboardManager::instance();
                     clipboard.syncFromSystemClipboard();
                     const QJsonObject effectJson = clipboard.pasteEffect();
                     const QString sourceId = effectJson.value(QStringLiteral("id")).toString();
                     auto *effectService = ArtifactEffectService::instance();
                     auto *projectService = ArtifactProjectService::instance();
                     if (!effectService || !projectService || sourceId.isEmpty()) return;
                     auto effectToPaste = effectService->createEffect(EffectID(sourceId));
                     if (!effectToPaste) {
                       effectToPaste = std::make_unique<ArtifactAbstractEffect>();
                       effectToPaste->setEffectID(UniString::fromQString(sourceId));
                     }
                     QString pastedId = sourceId + QStringLiteral("__paste");
                     int pastedIndex = 2;
                     while (currentEffectById(pastedId)) {
                       pastedId = QStringLiteral("%1__paste%2").arg(sourceId).arg(pastedIndex++);
                     }
                     effectToPaste->setEffectID(UniString::fromQString(pastedId));
                     effectToPaste->setDisplayName(
                         effectJson.value(QStringLiteral("displayName")).toString(sourceId));
                     effectToPaste->setPipelineStage(static_cast<EffectPipelineStage>(
                         effectJson.value(QStringLiteral("pipelineStage"))
                             .toInt(static_cast<int>(EffectPipelineStage::Rasterizer))));
                     effectToPaste->setComputeMode(static_cast<ComputeMode>(
                         effectJson.value(QStringLiteral("computeMode")).toInt(0)));
                     effectToPaste->setEnabled(
                         effectJson.value(QStringLiteral("enabled")).toBool(true));
                     if (effectJson.contains(QStringLiteral("mix"))) {
                       effectToPaste->setMix(static_cast<float>(
                           effectJson.value(QStringLiteral("mix")).toDouble(1.0)));
                     }
                     if (effectJson.contains(QStringLiteral("allowOverscan"))) {
                       effectToPaste->setAllowOverscan(
                           effectJson.value(QStringLiteral("allowOverscan")).toBool(false));
                     }
                     for (const auto &value : effectJson.value(QStringLiteral("properties")).toArray()) {
                       const QJsonObject property = value.toObject();
                       const QString name = property.value(QStringLiteral("name")).toString();
                       if (!name.isEmpty()) {
                         effectToPaste->setPropertyValue(
                             UniString::fromQString(name),
                             property.value(QStringLiteral("value")).toVariant());
                       }
                     }
                     if (effectJson.value(QStringLiteral("effectRegion")).isObject()) {
                       const QJsonObject regionJson =
                           effectJson.value(QStringLiteral("effectRegion")).toObject();
                       const QRectF region(
                           regionJson.value(QStringLiteral("x")).toDouble(),
                           regionJson.value(QStringLiteral("y")).toDouble(),
                           regionJson.value(QStringLiteral("width")).toDouble(),
                           regionJson.value(QStringLiteral("height")).toDouble());
                       if (region.isValid() && region.width() > 0.0 &&
                           region.height() > 0.0) {
                         effectToPaste->setEffectRegion(region);
                       }
                     }
                     auto effectPtr = ArtifactCore::makeShared(effectToPaste.release(),
                         [](ArtifactAbstractEffect* p) { delete p; });
                     if (projectService->addEffectToLayerWithUndo(currentLayerId_, effectPtr)) {
                       updateEffectsList();
                       if (statusLabel) statusLabel->setText(QStringLiteral("Status: Effect pasted"));
                     }
                   });

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
          -> ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA> {
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
      std::int64_t maskFrame = sourceLayer->currentFrame();
      if (auto *maskComposition = static_cast<ArtifactAbstractComposition *>(
              sourceLayer->composition())) {
        maskFrame = maskComposition->framePosition().framePosition();
      }
      sourceMask.applyToImage(maskW, maskH, &maskMat, 0.0f, 0.0f, 1.0f,
                              1.0f, maskFrame);
    }

    auto maskImage = ArtifactCore::makeShared<ArtifactCore::ImageF32x4_RGBA>();
    maskImage->setFromRGBA32F(maskMat.ptr<float>(), maskW, maskH);
    return maskImage;
  };

  auto captureEffectMaskImages =
      [](const ArtifactAbstractEffectPtr &effect) {
        std::vector<ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA>> masks;
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
      if (!mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
              effect, beforeMasks, afterMasks,
              QStringLiteral("Apply Layer Mask To Effect")))) {
        return;
      }
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
      const std::vector<ArtifactCore::SharedPtr<ArtifactCore::ImageF32x4_RGBA>> afterMasks;
      if (auto *mgr = UndoManager::instance()) {
        if (!mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
                effect, beforeMasks, afterMasks,
                QStringLiteral("Clear Effect Mask Images")))) {
          return;
        }
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
                       if (!mgr->push(std::make_unique<SetEffectMaskImagesCommand>(
                               effect, beforeMasks, afterMasks,
                               applyMode == QMessageBox::Yes
                                   ? QStringLiteral("Replace Effect Mask Images")
                                   : QStringLiteral("Append Effect Mask Image")))) {
                         return;
                       }
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

                     const QJsonObject beforePreset =
                         ArtifactPresetManager::effectToPresetJson(effect);
                     if (!ArtifactPresetManager::loadEffectPreset(effect,
                                                                   filePath)) {
                       QMessageBox::warning(
                           containerWidget, QStringLiteral("Effect Preset"),
                           QStringLiteral("エフェクトプリセットを読み込めませんでした。"));
                       return;
                     }

                     const QJsonObject afterPreset =
                         ArtifactPresetManager::effectToPresetJson(effect);
                     auto command = std::make_unique<EffectPresetSnapshotCommand>(
                         effect, beforePreset, afterPreset);
                     if (auto *undo = UndoManager::instance()) {
                       if (!undo->push(std::move(command))) {
                         auto restoreTarget = effect;
                         ArtifactPresetManager::applyPresetJsonToEffect(
                             restoreTarget, beforePreset);
                         return;
                       }
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

  menu.exec(accessibilityMenuPosition(menu, globalPos));
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

  ArtifactAbstractEffectPtr capturedEffect;
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
  // ProjectChangedEvent is also used for mutations and does not distinguish
  // close. Re-check the service state so a close/reset cannot leave stale
  // layer properties enabled in the inspector.
  auto *projectService = ArtifactProjectService::instance();
  if (!projectService || !projectService->hasProject()) {
    handleProjectClosed();
    return;
  }
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
  syncTemplateParameters();
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
  if (!layerNoteEdit) {
    return;
  }

  auto projectService = ArtifactProjectService::instance();
  if (!projectService || currentCompositionId_.isNil() ||
      currentLayerId_.isNil()) {
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
    effectsTargetLabel->setText(QStringLiteral("Target: Open a project to inspect effects"));
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
    effectsTargetLabel->setText(QStringLiteral("Target: Open a project to inspect effects"));
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
          QStringLiteral("Target: Select a composition to inspect effects"));
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
  const QString rackFilter = effectPropertyFilterEdit
                                 ? effectPropertyFilterEdit->text().trimmed()
                                 : QString();
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
        bool matches = rackFilter.isEmpty() ||
                       effect->displayName().toQString().contains(
                           rackFilter, Qt::CaseInsensitive) ||
                       effect->effectID().toQString().contains(
                           rackFilter, Qt::CaseInsensitive);
        if (!matches) {
          for (const auto &property : effect->editableProperties()) {
            if (property && property->getName().contains(
                                rackFilter, Qt::CaseInsensitive)) {
              matches = true;
              break;
            }
          }
        }
        if (matches) {
          rackEffects[rackIdx].push_back(effect);
        }
      }
    }
  }

  for (int i = 0; i < kEffectRackCount; ++i) {
    const QString rackSignature =
        computeRackSignature(i, rackEffects[i]) + QStringLiteral("|filter=") + rackFilter;
    if (rackSignature == lastRackSignatures_[i]) {
      if (racks[i].groupBox) {
        setInspectorEffectRackSurfaceTitle(racks[i].groupBox,
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
      setInspectorEffectRackSurfaceTitle(racks[i].groupBox,
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
      const auto uiDescriptor = effect->uiDescriptor();
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
          QStringLiteral("%1 on this %2. UI: Preview %3, Preset %4, Appearance %5, Section %6.%7%8 Single click to focus. Double click toggles enable/disable. Right click for effect actions.")
              .arg(effectName,
                   editingCompositionEffects() ? QStringLiteral("composition")
                                               : QStringLiteral("layer"),
                   uiDescriptor.preview ? QStringLiteral("available") : QStringLiteral("n/a"),
                   uiDescriptor.preset ? QStringLiteral("available") : QStringLiteral("n/a"),
                   uiDescriptor.appearance ? QStringLiteral("available") : QStringLiteral("n/a"),
                   uiDescriptor.section,
                   hasMask ? QStringLiteral(" Mask attached.") : QString(),
                   effectMaskCount > 0
                       ? QStringLiteral(" Effect mask images: %1.").arg(effectMaskCount)
                       : QString()));
      racks[i].listWidget->addItem(item);
    }
  }

  const int visibleEffectCount = std::accumulate(
      rackEffects.begin(), rackEffects.end(), 0,
      [](int count, const auto &rack) { return count + static_cast<int>(rack.size()); });
  if (effectCount > 0 && !rackFilter.isEmpty() && visibleEffectCount == 0) {
    setEffectsStateText("No effects match the current filter.", true);
  } else if (effectCount == 0) {
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
    state.targetText = QStringLiteral("Target: Open a project to inspect effects");
    state.stackSummaryText =
        QStringLiteral("Choose a project and composition to browse the effect stack.");
    return state;
  }

  if (currentCompositionId.isNil()) {
    state.stateText = QStringLiteral("Open a composition to manage effects.");
    state.targetText = QStringLiteral("Target: Select a composition to inspect effects");
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

  ArtifactAbstractEffectPtr newEffect;
  if (normalizedId == QStringLiteral("fractal_noise")) {
    newEffect = ArtifactCore::makeShared<FractalNoiseGenerator>();
  } else if (normalizedId == QStringLiteral("procedural_texture")) {
    newEffect = ArtifactCore::makeShared<ProceduralTextureGeneratorEffect>();
  } else if (normalizedId == QStringLiteral("transform_2d")) {
    newEffect = ArtifactCore::makeShared<LayerTransform2D>();
  } else {
    auto effect = effectService->createEffect(EffectID(normalizedId));
    if (effect) {
      newEffect = ArtifactCore::makeShared(effect.release(),
                                           [](ArtifactAbstractEffect* p) { delete p; });
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
          [&newEffect](const ArtifactAbstractEffectPtr &effect) {
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

  auto* dialog = createInspectorEffectPickerDialog(
      buildEffectCatalogEntries(), stageFromRackIndex(rackIndex), targetLabel,
      containerWidget ? containerWidget : tabWidget);
  if (!dialog || dialog->exec() != QDialog::Accepted) {
    return;
  }
  addSelectedEffectToCurrentTarget(
      inspectorEffectPickerSelectedEffectId(dialog));
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
                                   ArtifactAbstractEffectPtr
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
  auto audio = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer);
  if (!audio) {
    QMessageBox::warning(containerWidget, QStringLiteral("Lip Sync"),
                         QStringLiteral("Select an audio layer first."));
    return;
  }

  ArtifactCore::SharedPtr<ArtifactSwitchLayer> switchTarget;
  auto *selMgr = ArtifactLayerSelectionManager::instance();
  const auto selected = selMgr ? selMgr->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
  for (const auto &selectedLayer : selected) {
    if (!selectedLayer || selectedLayer == layer) {
      continue;
    }
    switchTarget = ArtifactCore::dynamicPointerCast<ArtifactSwitchLayer>(selectedLayer);
    if (switchTarget) {
      break;
    }
  }
  if (!switchTarget) {
    for (const auto &candidate : comp->allLayer()) {
      if (!candidate || candidate == layer) {
        continue;
      }
      switchTarget = ArtifactCore::dynamicPointerCast<ArtifactSwitchLayer>(candidate);
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

void ArtifactInspectorWidget::update()
{
  if (!impl_) {
    return;
  }
  impl_->scheduleRefresh();
}

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
  setAccessibleName(QStringLiteral("Inspector"));
  setAccessibleDescription(QStringLiteral("Inspect and edit the selected layer and composition"));
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

  auto* componentsStack = new StudioSectionStack();
  impl_->componentsGroup = componentsStack;
  applyInspectorPalette(impl_->componentsGroup);
  impl_->componentsSummaryLabel = createInspectorChromeLabel(
      QStringLiteral("Components: select a layer"),
      InspectorChromeLabelRole::Summary, impl_->componentsGroup);
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
  impl_->transformComponentButton = new InspectorActionButton("+ Transform");
  impl_->removeTransformComponentButton = new InspectorActionButton("Remove Transform");
  impl_->transformDuplicateButton = new InspectorActionButton("Duplicate Transform");
  impl_->transformMoveUpButton = new InspectorActionButton("Transform Up");
  impl_->transformMoveDownButton = new InspectorActionButton("Transform Down");
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
  impl_->addEffectorButton = new InspectorActionButton("+ Effector");
  impl_->removeEffectorButton = new InspectorActionButton("- Effector");
  for (auto *button : {impl_->physicsComponentButton,
                       impl_->scriptComponentButton,
                       impl_->layoutComponentButton,
                       impl_->cloneComponentButton,
                       impl_->fluidComponentButton,
                       impl_->generatorComponentButton,
                       impl_->removeGeneratorComponentButton,
                       impl_->generatorMoveUpButton,
                       impl_->generatorMoveDownButton,
                       impl_->transformComponentButton,
                       impl_->removeTransformComponentButton,
                       impl_->transformDuplicateButton,
                       impl_->transformMoveUpButton,
                       impl_->transformMoveDownButton,
                       impl_->fieldComponentButton,
                       impl_->removeFieldComponentButton,
                       impl_->fieldMoveUpButton,
                       impl_->fieldMoveDownButton,
                       impl_->cloneModifierButton,
                       impl_->removeCloneModifierButton,
                       impl_->cloneModifierMoveUpButton,
                       impl_->cloneModifierMoveDownButton,
                        impl_->openScriptButton,
                        impl_->applyLipSyncButton,
                        impl_->addEffectorButton,
                        impl_->removeEffectorButton}) {
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
    componentButton->setMinimumHeight(32);
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
  applyInspectorButton(impl_->addEffectorButton, false);
  applyInspectorButton(impl_->removeEffectorButton, false);
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
  impl_->addEffectorButton->setToolTip(
      QStringLiteral("Add an effector to this layer's clone chain."));
  impl_->removeEffectorButton->setToolTip(
      QStringLiteral("Remove an effector from this layer's clone chain."));
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

  impl_->activeComponentLabel = createInspectorChromeLabel(
      QStringLiteral("Active Component  |  None"),
      InspectorChromeLabelRole::Active, impl_->componentsGroup);
  impl_->activeComponentLabel->setMinimumHeight(32);
  applyInspectorLabelPalette(impl_->activeComponentLabel, true);
  impl_->activeComponentLabel->setVisible(false);

  auto *componentStackLabel = createInspectorChromeLabel(
      QStringLiteral("Layer Components"),
      InspectorChromeLabelRole::Section, impl_->componentsGroup);
  componentStackLabel->setMinimumHeight(28);
  applyInspectorLabelPalette(componentStackLabel, true);
  componentsStack->appendWidget(componentStackLabel);
  componentsStack->appendWidget(impl_->componentsSummaryLabel);
  componentsStack->appendWidget(impl_->cloneComponentButton);
  componentsStack->appendWidget(impl_->layoutComponentButton);
  componentsStack->appendWidget(impl_->physicsComponentButton);
  componentsStack->appendWidget(impl_->fluidComponentButton);
  componentsStack->appendWidget(impl_->scriptComponentButton);

  // Show the selected component's parameters before secondary management
  // controls so the edit loop stays next to the component rows.
  impl_->componentPropertySurface = createInspectorPropertySurface(
      nullptr, impl_->componentsGroup);
  impl_->componentPropertySurface->setObjectName(
      QStringLiteral("inspectorComponentPropertySurface"));
  impl_->componentPropertySurface->setVisible(false);
  componentsStack->appendWidget(impl_->componentPropertySurface, true);

  auto* effectorRow = createInspectorCanvasSurface(componentsStack);
  auto* effectorLayout = new QHBoxLayout(effectorRow);
  effectorLayout->setContentsMargins(0, 0, 0, 0);
  effectorLayout->addStretch(1);
  effectorLayout->addWidget(impl_->addEffectorButton);
  effectorLayout->addWidget(impl_->removeEffectorButton);
  effectorLayout->addStretch(1);
  componentsStack->appendWidget(effectorRow);

  auto* addComponentRow = createInspectorCanvasSurface(componentsStack);
  auto *addComponentLayout = new QHBoxLayout(addComponentRow);
  addComponentLayout->setContentsMargins(0, 0, 0, 0);
  addComponentLayout->addStretch(1);
  addComponentLayout->addWidget(impl_->addComponentButton);
  addComponentLayout->addStretch(1);
  componentsStack->appendWidget(addComponentRow);

  auto *componentDivider = createInspectorDivider(impl_->componentsGroup);
  componentDivider->setObjectName(QStringLiteral("inspectorComponentDivider"));
  componentDivider->setFrameShape(QFrame::HLine);
  componentDivider->setFrameShadow(QFrame::Plain);
  applyInspectorPalette(componentDivider, false);
  componentsStack->appendWidget(componentDivider);
  componentsStack->appendWidget(impl_->activeComponentLabel);

  impl_->clonerStructureWidget =
      createInspectorCanvasSurface(impl_->componentsGroup);
  auto *clonerStructureLayout = new QVBoxLayout(impl_->clonerStructureWidget);
  clonerStructureLayout->setContentsMargins(0, 4, 0, 0);
  clonerStructureLayout->setSpacing(6);
  auto *clonerStructureLabel = createInspectorChromeLabel(
      QStringLiteral("Cloner Structure"),
      InspectorChromeLabelRole::Section, impl_->clonerStructureWidget);
  clonerStructureLabel->setMinimumHeight(28);
  applyInspectorLabelPalette(clonerStructureLabel, true);
  clonerStructureLayout->addWidget(clonerStructureLabel);

  auto generatorHeaderLayout = new QHBoxLayout();
  auto *generatorHeaderLabel = createInspectorChromeLabel(
      QStringLiteral("Generators"), InspectorChromeLabelRole::Section,
      impl_->clonerStructureWidget);
  applyInspectorLabelPalette(generatorHeaderLabel, true);
  generatorHeaderLayout->addWidget(generatorHeaderLabel, 1);
  generatorHeaderLayout->addWidget(impl_->generatorComponentButton);
  generatorHeaderLayout->addWidget(impl_->generatorMoveUpButton);
  generatorHeaderLayout->addWidget(impl_->generatorMoveDownButton);
  generatorHeaderLayout->addWidget(impl_->removeGeneratorComponentButton);
  clonerStructureLayout->addLayout(generatorHeaderLayout);
  impl_->generatorListWidget = createInspectorSelectionList();
  impl_->generatorListWidget->setItemDelegate(
      createInspectorComponentStackItemDelegate(impl_->generatorListWidget));
  impl_->generatorListWidget->setVisible(false);
  impl_->generatorListWidget->setMaximumHeight(96);
  impl_->generatorListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->generatorListWidget);
  impl_->generatorListWidget->setAlternatingRowColors(false);
  impl_->generatorListWidget->setFrameShape(QFrame::NoFrame);
  impl_->generatorListWidget->setSpacing(2);
  applyInspectorOwnerDrawScrollBars(impl_->generatorListWidget);
  clonerStructureLayout->addWidget(impl_->generatorListWidget);

  auto transformHeaderLayout = new QHBoxLayout();
  auto *transformHeaderLabel = createInspectorChromeLabel(
      QStringLiteral("Transforms"), InspectorChromeLabelRole::Section,
      impl_->clonerStructureWidget);
  applyInspectorLabelPalette(transformHeaderLabel, true);
  transformHeaderLayout->addWidget(transformHeaderLabel, 1);
  transformHeaderLayout->addWidget(impl_->transformComponentButton);
  transformHeaderLayout->addWidget(impl_->transformDuplicateButton);
  transformHeaderLayout->addWidget(impl_->transformMoveUpButton);
  transformHeaderLayout->addWidget(impl_->transformMoveDownButton);
  transformHeaderLayout->addWidget(impl_->removeTransformComponentButton);
  clonerStructureLayout->addLayout(transformHeaderLayout);
  impl_->transformListWidget = createInspectorSelectionList();
  impl_->transformListWidget->setItemDelegate(
      createInspectorComponentStackItemDelegate(impl_->transformListWidget));
  impl_->transformListWidget->setVisible(false);
  impl_->transformListWidget->setMaximumHeight(96);
  impl_->transformListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->transformListWidget);
  impl_->transformListWidget->setAlternatingRowColors(false);
  impl_->transformListWidget->setFrameShape(QFrame::NoFrame);
  impl_->transformListWidget->setSpacing(2);
  applyInspectorOwnerDrawScrollBars(impl_->transformListWidget);
  clonerStructureLayout->addWidget(impl_->transformListWidget);

  auto fieldHeaderLayout = new QHBoxLayout();
  auto *fieldHeaderLabel = createInspectorChromeLabel(
      QStringLiteral("Fields"), InspectorChromeLabelRole::Section,
      impl_->clonerStructureWidget);
  applyInspectorLabelPalette(fieldHeaderLabel, true);
  fieldHeaderLayout->addWidget(fieldHeaderLabel, 1);
  fieldHeaderLayout->addWidget(impl_->fieldComponentButton);
  fieldHeaderLayout->addWidget(impl_->fieldMoveUpButton);
  fieldHeaderLayout->addWidget(impl_->fieldMoveDownButton);
  fieldHeaderLayout->addWidget(impl_->removeFieldComponentButton);
  clonerStructureLayout->addLayout(fieldHeaderLayout);
  impl_->fieldListWidget = createInspectorSelectionList();
  impl_->fieldListWidget->setItemDelegate(
      createInspectorComponentStackItemDelegate(impl_->fieldListWidget));
  impl_->fieldListWidget->setVisible(false);
  impl_->fieldListWidget->setMaximumHeight(96);
  impl_->fieldListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->fieldListWidget);
  impl_->fieldListWidget->setAlternatingRowColors(false);
  impl_->fieldListWidget->setFrameShape(QFrame::NoFrame);
  impl_->fieldListWidget->setSpacing(2);
  applyInspectorOwnerDrawScrollBars(impl_->fieldListWidget);
  clonerStructureLayout->addWidget(impl_->fieldListWidget);

  auto cloneModifierHeaderLayout = new QHBoxLayout();
  auto *cloneModifierHeaderLabel = createInspectorChromeLabel(
      QStringLiteral("Clone Modifiers"), InspectorChromeLabelRole::Section,
      impl_->clonerStructureWidget);
  applyInspectorLabelPalette(cloneModifierHeaderLabel, true);
  cloneModifierHeaderLayout->addWidget(cloneModifierHeaderLabel, 1);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierButton);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierMoveUpButton);
  cloneModifierHeaderLayout->addWidget(impl_->cloneModifierMoveDownButton);
  cloneModifierHeaderLayout->addWidget(impl_->removeCloneModifierButton);
  clonerStructureLayout->addLayout(cloneModifierHeaderLayout);
  impl_->cloneModifierListWidget = createInspectorSelectionList();
  impl_->cloneModifierListWidget->setItemDelegate(
      createInspectorComponentStackItemDelegate(impl_->cloneModifierListWidget));
  impl_->cloneModifierListWidget->setVisible(false);
  impl_->cloneModifierListWidget->setMaximumHeight(96);
  impl_->cloneModifierListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  applyInspectorList(impl_->cloneModifierListWidget);
  impl_->cloneModifierListWidget->setAlternatingRowColors(false);
  impl_->cloneModifierListWidget->setFrameShape(QFrame::NoFrame);
  impl_->cloneModifierListWidget->setSpacing(2);
  applyInspectorOwnerDrawScrollBars(impl_->cloneModifierListWidget);
  clonerStructureLayout->addWidget(impl_->cloneModifierListWidget);
  impl_->clonerStructureWidget->setVisible(false);
  componentsStack->appendWidget(impl_->clonerStructureWidget);

  impl_->componentUtilitiesLabel = createInspectorChromeLabel(
      QStringLiteral("Layer Utilities"), InspectorChromeLabelRole::Section,
      impl_->componentsGroup);
  applyInspectorLabelPalette(impl_->componentUtilitiesLabel, true);
  impl_->componentUtilitiesLabel->setVisible(false);
  componentsStack->appendWidget(impl_->componentUtilitiesLabel);
  componentsStack->appendWidget(impl_->openScriptButton);
  componentsStack->appendWidget(impl_->applyLipSyncButton);
  componentsStack->setContentsMargins(
      kInspectorNoteMargin, kInspectorNoteMargin, kInspectorNoteMargin,
      kInspectorNoteMargin);
  componentsStack->setSpacing(kInspectorSectionSpacing);
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
    const auto property = layer->getProperty(propertyPath);
    if (!property) {
      return;
    }
    const QVariant beforeValue = property->getValue();
    auto command = std::make_unique<SetLayerPropertyValueCommand>(
        layer, propertyPath, beforeValue, QVariant(nextEnabled),
        QStringLiteral("Toggle %1 Component").arg(displayName));
    bool applied = false;
    if (auto *manager = UndoManager::instance()) {
      applied = manager->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (applied) {
      impl_->focusedComponentName_ = displayName;
      impl_->focusedGeneratorId_.clear();
      impl_->focusedTransformId_.clear();
      impl_->focusedModifierId_.clear();
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
    if (impl_->componentEditorExpanded_ &&
        impl_->focusedComponentName_ == displayName) {
      impl_->componentEditorExpanded_ = false;
      if (impl_->componentPropertyWidget) {
        impl_->componentPropertyWidget->setVisible(false);
      }
      if (impl_->componentPropertySurface) {
        impl_->componentPropertySurface->setVisible(false);
      }
      impl_->updateComponentControls(layer);
      return;
    }
    impl_->focusedComponentName_ = displayName;
    impl_->focusedGeneratorId_.clear();
    impl_->focusedTransformId_.clear();
    impl_->focusedModifierId_.clear();
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
    const QPoint componentMenuOrigin =
        impl_->addComponentButton->mapToGlobal(
            QPoint(0, impl_->addComponentButton->height()));
    const auto selected = menu.exec(
        accessibilityMenuPosition(menu, componentMenuOrigin));
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
  auto applyComponentDescriptorMutation =
      [](const ArtifactAbstractLayerPtr &layer, const QString &label,
         const std::function<void()> &mutation) {
        if (!layer || !mutation) {
          return false;
        }
        const auto before = layer->componentDescriptorSnapshot();
        mutation();
        const auto after = layer->componentDescriptorSnapshot();
        if (before == after) {
          return true;
        }
        auto command = std::make_unique<LayerComponentDescriptorSnapshotCommand>(
            layer, before, after);
        if (auto *manager = UndoManager::instance()) {
          if (manager->push(std::move(command))) {
            return true;
          }
          layer->restoreComponentDescriptorSnapshot(before);
          return false;
        }
        return true;
      };
  impl_->generatorComponentButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                         QStringLiteral("matrix"),
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Add Generator"),
                             [layer, generatorChoice]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.generators.add"),
                                   generatorChoice);
                             })) {
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
  impl_->removeGeneratorComponentButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Remove Generator"),
                             [layer, generatorId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.generators.remove"),
                                   generatorId);
                             })) {
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
  const auto resolveInspectorCloneLayer =
      [this]() -> ArtifactAbstractLayerPtr {
    if (impl_->currentCompositionId_.isNil() ||
        impl_->currentLayerId_.isNil()) {
      return nullptr;
    }
    auto projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return nullptr;
    }
    auto findResult = projectService->findComposition(impl_->currentCompositionId_);
    if (!findResult.success) {
      return nullptr;
    }
    auto comp = findResult.ptr.lock();
    if (!comp) {
      return nullptr;
    }
    return comp->layerById(impl_->currentLayerId_);
  };
  const auto refreshAfterEffectorChange = [this](ArtifactCloneLayer* cloneLayer) {
    if (!cloneLayer) {
      return;
    }
    auto projectService = ArtifactProjectService::instance();
    if (projectService) {
      auto findResult = projectService->findComposition(impl_->currentCompositionId_);
      if (findResult.success) {
        if (auto comp = findResult.ptr.lock()) {
          if (auto layerPtr = comp->layerById(impl_->currentLayerId_)) {
            impl_->updateComponentControls(layerPtr);
          }
        }
      }
    }
    impl_->lastLayerInfoSignature_.clear();
    impl_->scheduleRefresh(
        ArtifactInspectorWidget::Impl::LayerInfoDirty |
        ArtifactInspectorWidget::Impl::EffectsDirty);
    if (impl_->statusLabel) {
      impl_->statusLabel->setText(
          QStringLiteral("Status: clone chain updated (%1 effectors)")
              .arg(cloneLayer->effectorCount()));
    }
  };
  const auto applyEffectorStackMutation =
      [](const ArtifactAbstractLayerPtr &layer, const QString &label,
         const std::function<void(ArtifactCloneLayer&)> &mutation) {
        if (!layer || !mutation) {
          return false;
        }
        auto *cloneLayer = dynamic_cast<ArtifactCloneLayer*>(layer.get());
        if (!cloneLayer) {
          return false;
        }
        const auto before = cloneLayer->effectorStackSnapshot();
        mutation(*cloneLayer);
        const auto after = cloneLayer->effectorStackSnapshot();
        if (before == after) {
          return true;
        }
        auto command = std::make_unique<CloneEffectorStackSnapshotCommand>(
            layer, before, after);
        if (auto *manager = UndoManager::instance()) {
          if (manager->push(std::move(command))) {
            return true;
          }
          cloneLayer->restoreEffectorStackSnapshot(before);
          return false;
        }
        return true;
      };
  impl_->addEffectorButton->setAction([this, resolveInspectorCloneLayer,
                                       refreshAfterEffectorChange,
                                       applyEffectorStackMutation]() {
                     const auto cloneLayerObject = resolveInspectorCloneLayer();
                     auto* cloneLayer = cloneLayerObject
                         ? dynamic_cast<ArtifactCloneLayer*>(cloneLayerObject.get())
                         : nullptr;
                     if (!cloneLayer) {
                       return;
                     }
                     const QStringList effectorChoices = {
                         QStringLiteral("transform"),
                         QStringLiteral("step"),
                         QStringLiteral("random"),
                         QStringLiteral("delay"),
                         QStringLiteral("sound"),
                         QStringLiteral("noise"),
                     };
                     bool accepted = false;
                     const QString choice = QInputDialog::getItem(
                         this, QStringLiteral("Add Effector"),
                         QStringLiteral("Effector Type"),
                         effectorChoices, 0, false, &accepted);
                     if (!accepted || choice.trimmed().isEmpty()) {
                       return;
                     }
                     ArtifactCore::SharedPtr<AbstractCloneEffector> effector;
                     if (choice == QLatin1String("transform")) {
                       effector = ArtifactCore::makeShared<TransformCloneEffector>();
                     } else if (choice == QLatin1String("step")) {
                       effector = ArtifactCore::makeShared<StepCloneEffector>();
                     } else if (choice == QLatin1String("random")) {
                       effector = ArtifactCore::makeShared<RandomCloneEffector>();
                     } else if (choice == QLatin1String("delay")) {
                       effector = ArtifactCore::makeShared<DelayCloneEffector>();
                     } else if (choice == QLatin1String("sound")) {
                       effector = ArtifactCore::makeShared<SoundCloneEffector>();
                     } else if (choice == QLatin1String("noise")) {
                       effector = ArtifactCore::makeShared<NoiseCloneEffector>();
                     }
                     if (!effector) {
                       return;
                     }
                     applyEffectorStackMutation(
                         cloneLayerObject, QStringLiteral("Add Clone Effector"),
                         [effector = std::move(effector)](
                             ArtifactCloneLayer& layer) mutable {
                           layer.addEffector(std::move(effector));
                         });
                     refreshAfterEffectorChange(cloneLayer);
                   });
  impl_->removeEffectorButton->setAction([this, resolveInspectorCloneLayer,
                                          refreshAfterEffectorChange,
                                          applyEffectorStackMutation]() {
                     const auto cloneLayerObject = resolveInspectorCloneLayer();
                     auto* cloneLayer = cloneLayerObject
                         ? dynamic_cast<ArtifactCloneLayer*>(cloneLayerObject.get())
                         : nullptr;
                     if (!cloneLayer || cloneLayer->effectorCount() == 0) {
                       return;
                     }
                     QStringList entries;
                     const int count = cloneLayer->effectorCount();
                     entries.reserve(count);
                     for (int i = 0; i < count; ++i) {
                       const auto effector = cloneLayer->effectorAt(i);
                       entries.push_back(
                           QStringLiteral("%1: %2")
                               .arg(i + 1)
                               .arg(effector ? effector->effectorTypeName()
                                             : QStringLiteral("custom")));
                     }
                     bool accepted = false;
                     const QString choice = QInputDialog::getItem(
                         this, QStringLiteral("Remove Effector"),
                         QStringLiteral("Effector"), entries, 0, false, &accepted);
                     if (!accepted) {
                       return;
                     }
                     const int sep = choice.indexOf(QLatin1Char(':'));
                     int index = -1;
                     if (sep > 0) {
                       index = choice.left(sep).toInt() - 1;
                     }
                     if (index < 0 || index >= count) {
                       return;
                     }
                     applyEffectorStackMutation(
                         cloneLayerObject, QStringLiteral("Remove Clone Effector"),
                         [index](ArtifactCloneLayer& layer) {
                           layer.removeEffector(index);
                         });
                     refreshAfterEffectorChange(cloneLayer);
                   });
  impl_->generatorMoveUpButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Generator Up"),
                             [layer, generatorId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.generators.moveUp"),
                                   generatorId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget->currentItem()));
                     }
                   });
  impl_->generatorMoveDownButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Generator Down"),
                             [layer, generatorId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.generators.moveDown"),
                                   generatorId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           generatorItemFilterText(
                               impl_->generatorListWidget->currentItem()));
                     }
                   });
  setInspectorSelectionAction(impl_->generatorListWidget,
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
        impl_->focusedGeneratorId_ = current
            ? current->data(Qt::UserRole).toString().trimmed()
            : QString();
        impl_->focusedTransformId_.clear();
        impl_->focusedModifierId_.clear();
        impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
        impl_->updateComponentControls(layer);
        impl_->focusComponentProperties(layer, filterText);
      });
  auto resolveCurrentInspectorLayer = [this]() -> ArtifactAbstractLayerPtr {
    if (impl_->currentCompositionId_.isNil() || impl_->currentLayerId_.isNil()) {
      return {};
    }
    auto* projectService = ArtifactProjectService::instance();
    if (!projectService) {
      return {};
    }
    const auto result = projectService->findComposition(impl_->currentCompositionId_);
    const auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
    return composition ? composition->layerById(impl_->currentLayerId_)
                       : ArtifactAbstractLayerPtr{};
  };
  auto refreshTransformStack = [this](const ArtifactAbstractLayerPtr& layer) {
    if (!layer) return;
    impl_->updateComponentControls(layer);
    impl_->focusComponentProperties(
        layer, transformItemFilterText(impl_->transformListWidget
                                            ? impl_->transformListWidget->currentItem()
                                            : nullptr));
    impl_->lastLayerInfoSignature_.clear();
    impl_->scheduleRefresh(ArtifactInspectorWidget::Impl::LayerInfoDirty |
                           ArtifactInspectorWidget::Impl::EffectsDirty);
  };
  auto applyClonerTransformMutation =
      [](const ArtifactAbstractLayerPtr &layer, const QString &label,
         const std::function<void()> &mutation) {
        if (!layer || !mutation) {
          return false;
        }
        const auto before = layer->clonerTransformsSnapshot();
        mutation();
        const auto after = layer->clonerTransformsSnapshot();
        if (before == after) {
          return true;
        }
        auto command = std::make_unique<ClonerTransformStackSnapshotCommand>(
            layer, before, after);
        if (auto *manager = UndoManager::instance()) {
          if (manager->push(std::move(command))) {
            return true;
          }
          layer->restoreClonerTransformsSnapshot(before);
          return false;
        }
        return true;
      };
  impl_->transformComponentButton->setAction([this, resolveCurrentInspectorLayer,
                                               refreshTransformStack,
                                               applyClonerTransformMutation]() {
    const auto layer = resolveCurrentInspectorLayer();
    if (!layer || !applyClonerTransformMutation(
                      layer, QStringLiteral("Add Cloner Transform"), [layer]() {
                        layer->setLayerPropertyValue(
                            QStringLiteral("component.cloner.transforms.add"), true);
                      })) {
      return;
    }
    const auto names = layer->clonerTransformNames();
    impl_->focusedComponentName_ = QStringLiteral("Transform");
    impl_->focusedTransformId_ = names.empty()
        ? QString()
        : QStringLiteral("transform.%1").arg(static_cast<int>(names.size()) - 1);
    impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
    refreshTransformStack(layer);
  });
  impl_->removeTransformComponentButton->setAction(
      [this, resolveCurrentInspectorLayer, refreshTransformStack,
       applyClonerTransformMutation]() {
        const auto layer = resolveCurrentInspectorLayer();
        const auto* item = impl_->transformListWidget
                               ? impl_->transformListWidget->currentItem()
                               : nullptr;
        if (!layer || !item) return;
        const int index = item->data(Qt::UserRole).toString().section(QLatin1Char('.'), -1).toInt();
        if (applyClonerTransformMutation(
                layer, QStringLiteral("Remove Cloner Transform"),
                [layer, index]() {
                  layer->setLayerPropertyValue(
                      QStringLiteral("component.cloner.transforms.remove"), index);
                })) {
          impl_->focusedTransformId_.clear();
          refreshTransformStack(layer);
        }
      });
  auto transformMoveAction = [this, resolveCurrentInspectorLayer,
                               refreshTransformStack,
                               applyClonerTransformMutation](const QString& property) {
    const auto layer = resolveCurrentInspectorLayer();
    const auto* item = impl_->transformListWidget
                           ? impl_->transformListWidget->currentItem()
                           : nullptr;
    if (!layer || !item) return;
    const int index = item->data(Qt::UserRole).toString().section(QLatin1Char('.'), -1).toInt();
    if (applyClonerTransformMutation(
            layer, QStringLiteral("Move Cloner Transform"),
            [layer, property, index]() {
              layer->setLayerPropertyValue(property, index);
            })) {
      refreshTransformStack(layer);
    }
  };
  impl_->transformDuplicateButton->setAction(
      [this, resolveCurrentInspectorLayer, refreshTransformStack,
       applyClonerTransformMutation]() {
        const auto layer = resolveCurrentInspectorLayer();
        const auto* item = impl_->transformListWidget
                               ? impl_->transformListWidget->currentItem()
                               : nullptr;
        if (!layer || !item) return;
        const int index = item->data(Qt::UserRole).toString().section(QLatin1Char('.'), -1).toInt();
        if (applyClonerTransformMutation(
                layer, QStringLiteral("Duplicate Cloner Transform"),
                [layer, index]() {
                  layer->setLayerPropertyValue(
                      QStringLiteral("component.cloner.transforms.duplicate"), index);
                })) {
          refreshTransformStack(layer);
        }
      });
  impl_->transformMoveUpButton->setAction(
      [transformMoveAction]() { transformMoveAction(
          QStringLiteral("component.cloner.transforms.moveUp")); });
  impl_->transformMoveDownButton->setAction(
      [transformMoveAction]() { transformMoveAction(
          QStringLiteral("component.cloner.transforms.moveDown")); });
  setInspectorSelectionAction(impl_->transformListWidget,
      [this, resolveCurrentInspectorLayer](QListWidgetItem* current) {
        const auto layer = resolveCurrentInspectorLayer();
        if (!layer || !current) return;
        impl_->focusedComponentName_ = QStringLiteral("Transform");
        impl_->focusedTransformId_ = current->data(Qt::UserRole).toString().trimmed();
        impl_->focusedGeneratorId_.clear();
        impl_->focusedModifierId_.clear();
        impl_->focusedComponentLayerId_ = impl_->currentLayerId_;
        impl_->updateComponentControls(layer);
        impl_->focusComponentProperties(layer, transformItemFilterText(current));
      });
  impl_->fieldComponentButton->setAction([this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Add Field"),
                             [layer, fieldChoice]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.fields.add"),
                                   fieldChoice);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget
                                          ? impl_->fieldListWidget->currentItem()
                                          : nullptr));
                     }
                   });
  impl_->removeFieldComponentButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Remove Field"),
                             [layer, fieldId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.fields.remove"),
                                   fieldId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  impl_->fieldMoveUpButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Field Up"),
                             [layer, fieldId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.fields.moveUp"),
                                   fieldId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  impl_->fieldMoveDownButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Field Down"),
                             [layer, fieldId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.fields.moveDown"),
                                   fieldId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer, fieldItemFilterText(
                                      impl_->fieldListWidget->currentItem()));
                     }
                   });
  setInspectorSelectionAction(impl_->fieldListWidget,
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
  impl_->cloneModifierButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                         QStringLiteral("formula"),
                         QStringLiteral("spline"),
                     };
                     bool accepted = false;
                     const QString modifierChoice = QInputDialog::getItem(
                         this, QStringLiteral("Add Clone Modifier"),
                         QStringLiteral("Modifier Type"), modifierChoices, 0,
                         false, &accepted);
                     if (!accepted || modifierChoice.trimmed().isEmpty()) {
                       return;
                     }
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Add Clone Modifier"),
                             [layer, modifierChoice]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.cloneModifiers.add"),
                                   modifierChoice);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget
                                   ? impl_->cloneModifierListWidget->currentItem()
                                   : nullptr));
                     }
                   });
  impl_->removeCloneModifierButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Remove Clone Modifier"),
                             [layer, modifierId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.cloneModifiers.remove"),
                                   modifierId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  impl_->cloneModifierMoveUpButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Clone Modifier Up"),
                             [layer, modifierId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.cloneModifiers.moveUp"),
                                   modifierId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  impl_->cloneModifierMoveDownButton->setAction(
      [this, applyComponentDescriptorMutation]() {
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
                     if (applyComponentDescriptorMutation(
                             layer, QStringLiteral("Move Clone Modifier Down"),
                             [layer, modifierId]() {
                               layer->setLayerPropertyValue(
                                   QStringLiteral("component.cloneModifiers.moveDown"),
                                   modifierId);
                             })) {
                       impl_->updateComponentControls(layer);
                       impl_->focusComponentProperties(
                           layer,
                           cloneModifierItemFilterText(
                               impl_->cloneModifierListWidget->currentItem()));
                     }
                   });
  setInspectorSelectionAction(impl_->cloneModifierListWidget,
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
        impl_->focusedModifierId_ = current
            ? current->data(Qt::UserRole).toString().trimmed()
            : QString();
        impl_->focusedGeneratorId_.clear();
        impl_->focusedTransformId_.clear();
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
  auto *componentsTab = WidgetCreationDiagnostics::createMeasured(
      QStringLiteral("Components"), QStringLiteral("inspector-surface"),
      QStringLiteral("inspector-default-components-surface"),
      [this]() {
        return new ArtifactComponentTabSurface(impl_->componentsGroup);
      });
  componentsTab->setObjectName(
      QStringLiteral("inspectorComponentsSurface"));
  componentsTab->setParent(this);
  impl_->templateParametersWidget = new ArtifactTemplateParametersWidget(this);
  impl_->templateParametersWidget->setObjectName(
      QStringLiteral("inspectorTemplateParametersSurface"));
  impl_->tabWidget->addTab(layerInfoWidget, "Layer");
  impl_->tabWidget->addTab(componentsTab, "Components");
  impl_->tabWidget->addTab(impl_->templateParametersWidget,
                           QStringLiteral("Template"));

  // ================== Effects Pipeline Tab ==================
  impl_->effectsScrollArea = new QScrollArea();
  impl_->effectsScrollArea->setObjectName(QStringLiteral("inspectorEffectsScrollArea"));
  impl_->effectsScrollArea->setWidgetResizable(true);
  impl_->effectsScrollArea->setFrameShape(QFrame::NoFrame);
  applyInspectorOwnerDrawScrollBars(impl_->effectsScrollArea);
  impl_->effectsTabWidget = createInspectorCanvasSurface();
  impl_->effectsTabWidget->setObjectName(QStringLiteral("inspectorEffectsTabWidget"));
  auto effectsLayout = new QVBoxLayout();
  auto *effectsHeaderFrame =
      createInspectorEffectPanelSurface(InspectorEffectPanelRole::Header);
  effectsHeaderFrame->setObjectName(QStringLiteral("inspectorEffectsHeaderFrame"));
  applyInspectorPalette(effectsHeaderFrame, false);
  auto *effectsHeaderLayout = new QVBoxLayout(effectsHeaderFrame);
  effectsHeaderLayout->setContentsMargins(10, 10, 10, 10);
  effectsHeaderLayout->setSpacing(6);

  impl_->effectsStateLabel = createInspectorChromeLabel(
      QStringLiteral("Open a composition to manage effects."),
      InspectorChromeLabelRole::Summary, effectsHeaderFrame);
  impl_->effectsStateLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStateLabel, true);
  effectsHeaderLayout->addWidget(impl_->effectsStateLabel);

  impl_->effectsTargetLabel = createInspectorChromeLabel(
      QStringLiteral("Target: Select a composition to inspect effects"),
      InspectorChromeLabelRole::Active, effectsHeaderFrame);
  impl_->effectsTargetLabel->setMinimumHeight(30);
  impl_->effectsTargetLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsTargetLabel, false);
  effectsHeaderLayout->addWidget(impl_->effectsTargetLabel);

  impl_->effectPropertyFilterEdit = new QLineEdit(effectsHeaderFrame);
  impl_->effectPropertyFilterEdit->setObjectName(
      QStringLiteral("inspectorEffectPropertyFilter"));
  impl_->effectPropertyFilterEdit->setPlaceholderText(
      QStringLiteral("Filter effect properties"));
  impl_->effectPropertyFilterEdit->setFrame(false);
  applyInspectorPalette(impl_->effectPropertyFilterEdit, true);
  effectsHeaderLayout->addWidget(impl_->effectPropertyFilterEdit);
  QObject::connect(impl_->effectPropertyFilterEdit, &QLineEdit::textChanged,
                   this, [this](const QString &text) {
                     if (impl_->effectPropertyWidget) {
                       impl_->effectPropertyWidget->setFilterText(text);
                     }
                     impl_->updateEffectsList();
                   });

  auto *effectsToolbarLayout = new QHBoxLayout();
  effectsToolbarLayout->setContentsMargins(0, 0, 0, 0);
  effectsToolbarLayout->setSpacing(8);
  auto* effectsQuickAddButton =
      new InspectorActionButton(QStringLiteral("+ Add Effect"));
  effectsQuickAddButton->setOwnerDrawn(true);
  impl_->effectsQuickAddButton = effectsQuickAddButton;
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

  auto *stackPanel =
      createInspectorEffectPanelSurface(InspectorEffectPanelRole::Stack);
  stackPanel->setObjectName(QStringLiteral("inspectorEffectsStackPanel"));
  applyInspectorPalette(stackPanel, false);
  auto *stackPanelLayout = new QVBoxLayout(stackPanel);
  stackPanelLayout->setContentsMargins(8, 8, 8, 8);
  stackPanelLayout->setSpacing(8);

  impl_->effectsStackSummaryLabel = createInspectorChromeLabel(
      QStringLiteral("Effect Controls"), InspectorChromeLabelRole::Section,
      stackPanel);
  impl_->effectsStackSummaryLabel->setMinimumHeight(28);
  impl_->effectsStackSummaryLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectsStackSummaryLabel, false);
  stackPanelLayout->addWidget(impl_->effectsStackSummaryLabel);

  auto *detailPanel =
      createInspectorEffectPanelSurface(InspectorEffectPanelRole::Detail);
  detailPanel->setObjectName(QStringLiteral("inspectorEffectsDetailPanel"));
  applyInspectorPalette(detailPanel, false);
  auto *detailPanelLayout = new QVBoxLayout(detailPanel);
  detailPanelLayout->setContentsMargins(8, 8, 8, 8);
  detailPanelLayout->setSpacing(8);

  impl_->effectEditorTitleLabel = createInspectorChromeLabel(
      QStringLiteral("Selected Effect Controls"),
      InspectorChromeLabelRole::Active, detailPanel);
  impl_->effectEditorTitleLabel->setMinimumHeight(32);
  applyInspectorLabelPalette(impl_->effectEditorTitleLabel, true);
  detailPanelLayout->addWidget(impl_->effectEditorTitleLabel);

  impl_->effectEnableButton =
      new InspectorActionButton(QStringLiteral("Enabled"));
  impl_->effectEnableButton->setOwnerDrawn(true);
  impl_->effectEnableButton->setCheckable(true);
  impl_->effectEnableButton->setVisible(false);
  impl_->effectEnableButton->setMinimumHeight(28);
  applyInspectorButton(impl_->effectEnableButton, false);
  impl_->effectEnableButton->setToolTip(
      QStringLiteral("Temporarily bypass the selected effect."));
  detailPanelLayout->addWidget(impl_->effectEnableButton);

  impl_->surfaceElementPanel = new QWidget(detailPanel);
  impl_->surfaceElementPanel->setVisible(false);
  auto *surfaceElementLayout = new QVBoxLayout(impl_->surfaceElementPanel);
  surfaceElementLayout->setContentsMargins(0, 2, 0, 2);
  surfaceElementLayout->setSpacing(4);
  auto *surfaceElementTitle = createInspectorChromeLabel(
      QStringLiteral("Lens Surface Elements"),
      InspectorChromeLabelRole::Section, impl_->surfaceElementPanel);
  surfaceElementTitle->setMinimumHeight(26);
  applyInspectorLabelPalette(surfaceElementTitle, false);
  surfaceElementLayout->addWidget(surfaceElementTitle);
  auto *surfaceElementActions = new QHBoxLayout();
  surfaceElementActions->setContentsMargins(0, 0, 0, 0);
  surfaceElementActions->setSpacing(4);
  auto *addSurfaceElementButton = new InspectorActionButton(
      QStringLiteral("Add Decal"), impl_->surfaceElementPanel);
  auto *duplicateSurfaceElementButton = new InspectorActionButton(
      QStringLiteral("Duplicate"), impl_->surfaceElementPanel);
  auto *deleteSurfaceElementButton = new InspectorActionButton(
      QStringLiteral("Delete"), impl_->surfaceElementPanel);
  auto *moveSurfaceElementUpButton = new InspectorActionButton(
      QStringLiteral("Up"), impl_->surfaceElementPanel);
  auto *moveSurfaceElementDownButton = new InspectorActionButton(
      QStringLiteral("Down"), impl_->surfaceElementPanel);
  for (auto *button : {addSurfaceElementButton, duplicateSurfaceElementButton,
                       deleteSurfaceElementButton,
                       moveSurfaceElementUpButton, moveSurfaceElementDownButton}) {
    button->setOwnerDrawn(true);
    button->setMinimumHeight(26);
    applyInspectorButton(button, false);
    surfaceElementActions->addWidget(button, 1);
  }
  surfaceElementLayout->addLayout(surfaceElementActions);
  impl_->surfaceElementListWidget = createInspectorSelectionList(
      impl_->surfaceElementPanel);
  impl_->surfaceElementListWidget->setObjectName(
      QStringLiteral("lensSurfaceElementList"));
  impl_->surfaceElementListWidget->setSelectionMode(
      QAbstractItemView::SingleSelection);
  impl_->surfaceElementListWidget->setMinimumHeight(48);
  impl_->surfaceElementListWidget->setMaximumHeight(132);
  setInspectorSelectionAction(impl_->surfaceElementListWidget,
      [this](QListWidgetItem *item) {
        if (!item) {
          return;
        }
        const int index = item->data(Qt::UserRole).toInt();
        const auto effect = impl_->currentEffectById(impl_->focusedEffectId_);
        auto *surface = effect
            ? dynamic_cast<SurfaceFXEffect *>(effect.get())
            : nullptr;
        if (!surface) {
          return;
        }
        impl_->surfaceElementIndex_ = std::clamp(index, 0,
                                                 static_cast<int>(surface->data().elements.size()) - 1);
        surface->setPropertyValue(
            ArtifactCore::UniString::fromQString(QStringLiteral("Surface Element Index")),
            impl_->surfaceElementIndex_);
        if (impl_->effectPropertyWidget) {
          impl_->effectPropertyWidget->updateProperties();
        }
      });
  const auto selectedSurfaceElement = [this]() -> SurfaceFXEffect * {
    const auto effect = impl_->currentEffectById(impl_->focusedEffectId_);
    return effect ? dynamic_cast<SurfaceFXEffect *>(effect.get()) : nullptr;
  };
  const auto refreshSurfaceElementEditor = [this]() {
    const auto effect = impl_->currentEffectById(impl_->focusedEffectId_);
    impl_->updateSurfaceElementEditor(effect);
    if (impl_->effectPropertyWidget) {
      impl_->effectPropertyWidget->updateProperties();
    }
    impl_->scheduleRefresh(ArtifactInspectorWidget::Impl::EffectsDirty);
  };
  addSurfaceElementButton->setAction([this, refreshSurfaceElementEditor]() {
    auto *surface = [&]() -> SurfaceFXEffect * {
      const auto effect = impl_->currentEffectById(impl_->focusedEffectId_);
      return effect ? dynamic_cast<SurfaceFXEffect *>(effect.get()) : nullptr;
    }();
    if (!surface || surface->data().elements.size() >= 128) return;
    const auto before = surface->data();
    auto after = before;
    ArtifactCore::SurfaceFXElement element;
    const int index = static_cast<int>(after.elements.size());
    element.id = QStringLiteral("surface-decal-%1").arg(index + 1);
    element.type = ArtifactCore::SurfaceFXElementType::TextureDecal;
    element.x = 0.42f;
    element.y = 0.42f;
    element.width = 0.16f;
    element.height = 0.16f;
    element.opacity = 0.75f;
    element.tintR = 0.55f;
    element.tintG = 0.08f;
    element.tintB = 0.04f;
    element.blendMode = QStringLiteral("multiply");
    element.seedOffset = index;
    after.elements.push_back(std::move(element));
    const int previousSurfaceElementIndex = impl_->surfaceElementIndex_;
    impl_->surfaceElementIndex_ = index;
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<SurfaceFXElementSnapshotCommand>(
        impl_->currentEffectById(impl_->focusedEffectId_), before,
        std::move(after), QStringLiteral("Add Lens Surface Decal"));
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      impl_->surfaceElementIndex_ = previousSurfaceElementIndex;
      return;
    }
    refreshSurfaceElementEditor();
  });
  duplicateSurfaceElementButton->setAction([this, selectedSurfaceElement,
                                             refreshSurfaceElementEditor]() {
    auto *surface = selectedSurfaceElement();
    if (!surface || surface->data().elements.empty()) return;
    const auto before = surface->data();
    auto after = before;
    auto &elements = after.elements;
    const int index = std::clamp(impl_->surfaceElementIndex_, 0,
                                 static_cast<int>(elements.size()) - 1);
    auto copy = elements[static_cast<std::size_t>(index)];
    copy.id = QStringLiteral("%1-copy").arg(copy.id.trimmed().isEmpty()
                                                ? QStringLiteral("surface-element-%1").arg(index + 1)
                                                : copy.id);
    copy.seedOffset += 1000 + index;
    elements.insert(elements.begin() + index + 1, copy);
    const int previousSurfaceElementIndex = impl_->surfaceElementIndex_;
    impl_->surfaceElementIndex_ = index + 1;
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<SurfaceFXElementSnapshotCommand>(
        impl_->currentEffectById(impl_->focusedEffectId_), before,
        std::move(after), QStringLiteral("Duplicate Lens Surface Element"));
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      impl_->surfaceElementIndex_ = previousSurfaceElementIndex;
      return;
    }
    refreshSurfaceElementEditor();
  });
  deleteSurfaceElementButton->setAction([this, selectedSurfaceElement,
                                          refreshSurfaceElementEditor]() {
    auto *surface = selectedSurfaceElement();
    if (!surface || surface->data().elements.empty()) return;
    const auto before = surface->data();
    auto after = before;
    auto &elements = after.elements;
    const int index = std::clamp(impl_->surfaceElementIndex_, 0,
                                 static_cast<int>(elements.size()) - 1);
    elements.erase(elements.begin() + index);
    const int previousSurfaceElementIndex = impl_->surfaceElementIndex_;
    impl_->surfaceElementIndex_ = std::max(0, index - 1);
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<SurfaceFXElementSnapshotCommand>(
        impl_->currentEffectById(impl_->focusedEffectId_), before,
        std::move(after), QStringLiteral("Delete Lens Surface Element"));
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      impl_->surfaceElementIndex_ = previousSurfaceElementIndex;
      return;
    }
    refreshSurfaceElementEditor();
  });
  moveSurfaceElementUpButton->setAction([this, selectedSurfaceElement,
                                         refreshSurfaceElementEditor]() {
    auto *surface = selectedSurfaceElement();
    if (!surface || surface->data().elements.empty()) return;
    const auto before = surface->data();
    auto after = before;
    auto &elements = after.elements;
    const int index = std::clamp(impl_->surfaceElementIndex_, 0,
                                 static_cast<int>(elements.size()) - 1);
    if (index <= 0) return;
    std::swap(elements[static_cast<std::size_t>(index)],
              elements[static_cast<std::size_t>(index - 1)]);
    const int previousSurfaceElementIndex = impl_->surfaceElementIndex_;
    impl_->surfaceElementIndex_ = index - 1;
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<SurfaceFXElementSnapshotCommand>(
        impl_->currentEffectById(impl_->focusedEffectId_), before,
        std::move(after), QStringLiteral("Move Lens Surface Element Up"));
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      impl_->surfaceElementIndex_ = previousSurfaceElementIndex;
      return;
    }
    refreshSurfaceElementEditor();
  });
  moveSurfaceElementDownButton->setAction([this, selectedSurfaceElement,
                                           refreshSurfaceElementEditor]() {
    auto *surface = selectedSurfaceElement();
    if (!surface || surface->data().elements.empty()) return;
    const auto before = surface->data();
    auto after = before;
    auto &elements = after.elements;
    const int index = std::clamp(impl_->surfaceElementIndex_, 0,
                                 static_cast<int>(elements.size()) - 1);
    if (index >= static_cast<int>(elements.size()) - 1) return;
    std::swap(elements[static_cast<std::size_t>(index)],
              elements[static_cast<std::size_t>(index + 1)]);
    const int previousSurfaceElementIndex = impl_->surfaceElementIndex_;
    impl_->surfaceElementIndex_ = index + 1;
    auto *undo = UndoManager::instance();
    auto command = std::make_unique<SurfaceFXElementSnapshotCommand>(
        impl_->currentEffectById(impl_->focusedEffectId_), before,
        std::move(after), QStringLiteral("Move Lens Surface Element Down"));
    bool applied = false;
    if (undo) {
      applied = undo->push(std::move(command));
    } else {
      command->redo();
      applied = command->lastOperationSucceeded();
    }
    if (!applied) {
      impl_->surfaceElementIndex_ = previousSurfaceElementIndex;
      return;
    }
    refreshSurfaceElementEditor();
  });
  surfaceElementLayout->addWidget(impl_->surfaceElementListWidget);
  detailPanelLayout->addWidget(impl_->surfaceElementPanel);

  impl_->effectParametersHintLabel = createInspectorChromeLabel(
      QStringLiteral("Select an effect above to reveal its parameters here."),
      InspectorChromeLabelRole::Summary, detailPanel);
  impl_->effectParametersHintLabel->setWordWrap(true);
  applyInspectorLabelPalette(impl_->effectParametersHintLabel, false);
  detailPanelLayout->addWidget(impl_->effectParametersHintLabel);

  impl_->effectPropertySurface = createInspectorPropertySurface(
      nullptr, detailPanel);
  impl_->effectPropertySurface->setObjectName(
      QStringLiteral("inspectorEffectPropertySurface"));
  impl_->effectPropertySurface->setVisible(false);
  detailPanelLayout->addWidget(impl_->effectPropertySurface, 1);

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
    auto rackGroup = createInspectorEffectRackSurface(rackNames[i]);
    impl_->racks[i].groupBox = rackGroup;
    applyInspectorPalette(rackGroup, false);
    auto rackLayout = new QVBoxLayout();

    impl_->racks[i].listWidget = createInspectorEffectRackList();
    impl_->racks[i].listWidget->setDragEnabled(true);
    impl_->racks[i].listWidget->setAcceptDrops(true);
    impl_->racks[i].listWidget->setDropIndicatorShown(true);
    impl_->racks[i].listWidget->setDragDropMode(QAbstractItemView::DragDrop);
    impl_->racks[i].listWidget->setDefaultDropAction(Qt::MoveAction);
    setInspectorEffectRackReorderHandler(impl_->racks[i].listWidget,
        [this](const QStringList &effectIds, int distance) {
          if (effectIds.isEmpty() || distance == 0) {
            return;
          }
          const int direction = distance > 0 ? 1 : -1;
          const int steps = std::abs(distance);
          bool moved = false;
          for (int step = 0; step < steps; ++step) {
            // Move the lower rows first when moving down and the upper rows
            // first when moving up so a multi-selection keeps its order.
            const int begin = direction > 0 ? effectIds.size() - 1 : 0;
            const int end = direction > 0 ? -1 : effectIds.size();
            const int increment = direction > 0 ? -1 : 1;
            bool movedThisStep = false;
            for (int index = begin; index != end; index += increment) {
              const QString effectId = effectIds.at(index).trimmed();
              if (effectId.isEmpty()) {
                continue;
              }
              if (impl_->moveEffectById(effectId, direction)) {
                moved = true;
                movedThisStep = true;
              }
            }
            if (!movedThisStep) {
              break;
            }
          }
          if (moved) {
            impl_->updateEffectsList();
            impl_->syncEffectPropertyWidget();
          }
        });
    impl_->racks[i].listWidget->setMinimumHeight(38);
    impl_->racks[i].listWidget->setMaximumHeight(132);
    impl_->racks[i].listWidget->setUniformItemSizes(true);
    impl_->racks[i].listWidget->setFrameShape(QFrame::NoFrame);
    impl_->racks[i].listWidget->setSpacing(5);
    impl_->racks[i].listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    impl_->racks[i].listWidget->setItemDelegate(
        createInspectorEffectRackItemDelegate(i, impl_->racks[i].listWidget));
    impl_->racks[i].listWidget->setToolTip(
        QStringLiteral("Single click an effect to edit its parameters below. Double click toggles enable/disable. Right click opens effect actions."));
    applyInspectorList(impl_->racks[i].listWidget);
    applyInspectorOwnerDrawScrollBars(impl_->racks[i].listWidget);
    impl_->racks[i].listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    if (impl_->racks[i].listWidget->viewport()) {
      impl_->racks[i].listWidget->viewport()->setContextMenuPolicy(
          Qt::CustomContextMenu);
    }

    auto btnLayout = new QHBoxLayout();
  auto* rackAddButton = new InspectorActionButton(QStringLiteral("+ Add"));
  rackAddButton->setOwnerDrawn(true);
  impl_->racks[i].addButton = rackAddButton;
  impl_->racks[i].addButton->setObjectName(QStringLiteral("inspectorRackAddButton"));
  auto* rackRemoveButton =
      new InspectorActionButton(QStringLiteral("Remove"));
  rackRemoveButton->setOwnerDrawn(true);
  impl_->racks[i].removeButton = rackRemoveButton;
  impl_->racks[i].removeButton->setObjectName(QStringLiteral("inspectorRackRemoveButton"));
  auto* rackMoveUpButton =
      new InspectorActionButton(QStringLiteral("Move Up"));
  rackMoveUpButton->setOwnerDrawn(true);
  impl_->racks[i].moveUpButton = rackMoveUpButton;
  impl_->racks[i].moveUpButton->setObjectName(QStringLiteral("inspectorRackMoveUpButton"));
  auto* rackMoveDownButton =
      new InspectorActionButton(QStringLiteral("Move Down"));
  rackMoveDownButton->setOwnerDrawn(true);
  impl_->racks[i].moveDownButton = rackMoveDownButton;
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
    rackLayout->setContentsMargins(kInspectorRackMarginL, 34,
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
  auto *effectsSurface = WidgetCreationDiagnostics::createMeasured(
      QStringLiteral("Effects"), QStringLiteral("inspector-surface"),
      QStringLiteral("inspector-default-effects-surface"),
      [this, stackPanel, detailPanel]() {
        return new ArtifactEffectTabSurface(stackPanel, detailPanel,
                                            impl_->effectsTabWidget);
      });
  effectsLayout->addWidget(effectsSurface, 1);
  QObject::connect(impl_->effectsQuickAddButton, &QPushButton::clicked, this,
                   [this]() { impl_->handleAddEffectClicked(-1); });

  auto *quickAddEffectShortcut =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space),
                    impl_->effectsScrollArea);
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
  impl_->effectsScrollArea->setParent(this);
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
      impl_->eventBus_.subscribe<LayerNoteChangedEvent>(
          [this](const LayerNoteChangedEvent &event) {
            const QString compositionId = event.compositionId;
            const QString layerId = event.layerId;
            const QString note = event.note;
            const auto apply = [this, compositionId, layerId, note]() {
              if (!impl_ || !impl_->layerNoteEdit ||
                  impl_->currentCompositionId_.toString() != compositionId ||
                  impl_->currentLayerId_.toString() != layerId) {
                return;
              }
              impl_->lastLayerNoteText_ = note;
              QSignalBlocker blocker(impl_->layerNoteEdit);
              impl_->layerNoteEdit->setPlainText(note);
              impl_->layerNoteEdit->setEnabled(true);
              if (impl_->layerNoteGroup) {
                impl_->layerNoteGroup->setEnabled(true);
                impl_->layerNoteGroup->hide();
              }
            };
            if (QThread::currentThread() == thread()) {
              apply();
            } else {
              QMetaObject::invokeMethod(this, apply, Qt::QueuedConnection);
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
