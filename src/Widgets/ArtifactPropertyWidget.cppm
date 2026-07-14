module;

#include <QColor>
#include <QApplication>
#include <QCursor>
#include <QPoint>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QDir>
#include <QMultiHash>
#include <QPalette>
#include <QPushButton>
#include <QToolButton>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectimpl.h>


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
#include <limits>
#include <list>
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

module Artifact.Widgets.ArtifactPropertyWidget;

import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Layer.InitParams;
import Artifact.Composition.Abstract;
import Artifact.Widgets.PropertyEditor;
import Property;
import Property.Abstract;
import Property.Group;
import Undo.UndoManager;
import Artifact.Effect.Abstract;
import Artifact.Application.Manager;
import Utils.String.UniString;
import Widgets.Utils.CSS;
import Artifact.Widgets.Timeline;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Widgets.PropertyEditor;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;
import Time.Rational;
import Script.Expression.Evaluator;
import Property.SerializationBridge;
import Settings.Accessibility;

namespace Artifact {

namespace detail {

struct LayerStateToggleDef {
  const char *propertyName;
  const char *label;
  const char *tooltip;
};

void applyPropertySearchPalette(QLineEdit *edit);
void applyPropertySectionLabel(QLabel *label, bool prominent);
void clearLayoutRecursive(QLayout *layout);
void updateScaleSupplementaryText(
    ArtifactPropertyEditorRowWidget *row,
    const ArtifactAbstractLayerPtr &layer,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
    const QVariant &value);
void launchExpressionCopilot(
    QWidget *parent, const QString &propertyName,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const QString &initialExpression, const ArtifactAbstractLayerPtr &layer,
    const RationalTime &currentTime,
    const std::function<void(const QString &)> &applyHandler);
void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer);

constexpr std::array<LayerStateToggleDef, 8> kLayerStateToggleDefs = {{
    {"layer.visible", "Visible", "Show or hide the layer"},
    {"layer.locked", "Lock", "Prevent direct edits on the layer"},
    {"layer.selectionLocked", "Sel", "Prevent selection in the layer panel"},
    {"layer.transformLocked", "Xform", "Prevent transform edits"},
    {"layer.timingLocked", "Time", "Prevent timing edits"},
    {"layer.guide", "Guide", "Mark as guide layer"},
    {"layer.solo", "Solo", "Solo this layer"},
    {"layer.shy", "Shy", "Hide the layer from the panel"},
}};

struct EffectPresentationDescriptor {
  QString stageText;
  QString headingText;
  QString stageNoteText;
  LayerPresentationBadgeTone badgeTone = LayerPresentationBadgeTone::Neutral;
};

template <typename EffectPtr>
EffectPresentationDescriptor describeEffectPresentation(const EffectPtr &effect) {
  EffectPresentationDescriptor descriptor;
  descriptor.stageText = QStringLiteral("Effect");
  descriptor.headingText = QStringLiteral("Effect");
  descriptor.stageNoteText = QStringLiteral("Stage: Unknown");
  descriptor.badgeTone = LayerPresentationBadgeTone::Neutral;
  if (!effect) {
    return descriptor;
  }

  switch (effect->pipelineStage()) {
  case EffectPipelineStage::Generator:
    descriptor.stageText = QStringLiteral("Generator");
    descriptor.stageNoteText = QStringLiteral("Stage: Generator");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    break;
  case EffectPipelineStage::GeometryTransform:
    descriptor.stageText = QStringLiteral("Geo Transform");
    descriptor.stageNoteText = QStringLiteral("Stage: Geometry Transform");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    break;
  case EffectPipelineStage::MaterialRender:
    descriptor.stageText = QStringLiteral("Material");
    descriptor.stageNoteText = QStringLiteral("Stage: Material Render");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    break;
  case EffectPipelineStage::Rasterizer:
    descriptor.stageText = QStringLiteral("Rasterizer");
    descriptor.stageNoteText = QStringLiteral("Stage: Rasterizer");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    break;
  case EffectPipelineStage::LayerTransform:
    descriptor.stageText = QStringLiteral("Layer Transform");
    descriptor.stageNoteText = QStringLiteral("Stage: Layer Transform");
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    break;
  default:
    break;
  }

  descriptor.headingText =
      QStringLiteral("%1 · %2")
          .arg(descriptor.stageText, effect->displayName().toQString());
  return descriptor;
}

class ScopedPropertyEditGuard final {
public:
  explicit ScopedPropertyEditGuard(int &depth) : depth_(depth) { ++depth_; }
  ~ScopedPropertyEditGuard() { --depth_; }

private:
  int &depth_;
};

class CollapsibleSectionButton final : public QToolButton {
public:
  explicit CollapsibleSectionButton(QWidget *parent = nullptr)
      : QToolButton(parent) {
    setCheckable(true);
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  }

  void setTarget(QWidget *target) {
    target_ = target;
    applyState(isChecked());
  }

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event) {
      event->accept();
    }
    toggle();
    applyState(isChecked());
  }

private:
  void applyState(bool expanded) {
    setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    if (target_) {
      target_->setVisible(expanded);
    }
  }

  QWidget *target_ = nullptr;
};

QStringList loadFavoriteProperties();
void saveFavoriteProperties(const QStringList &favorites);
bool isFavorite(const QString &propertyPath);
void toggleFavorite(const QString &propertyPath);
bool isExpandedInspectorSection(const QString &groupName);
bool shouldHideInspectorPropertyGroup(const QString &groupName);
bool isClonerSection(const QString &groupName);
bool isSourceReframeSection(const QString &groupName);

struct PropertyPresentationProfile {
  QString id;
  QStringList visibleGroups;
};

PropertyPresentationProfile
propertyPresentationProfile(const ArtifactAbstractLayerPtr &layer);
bool presentationAllowsGroup(const PropertyPresentationProfile &profile,
                             const QString &groupName);
void applyPresentationPropertyRules(
    const PropertyPresentationProfile &profile, const QString &groupName,
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties);
std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
applyFavoriteFilter(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    bool favoriteOnly);
std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
filteredGroupProperties(
    const ArtifactAbstractLayerPtr &layer, const QString &groupName,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties);
std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
inspectorProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties);
void registerCurrentLayerPropertySnapshot(
    const ArtifactAbstractLayerPtr &layer, const QString &focusedEffectId);
void alignPropertyRowLabels(
    const std::vector<ArtifactPropertyEditorRowWidget *> &rows, int minWidth,
    int maxWidth);
void addRowsFromProperties(
    QWidget *owner, QVBoxLayout *layout,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    const QString &filterText,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue,
    const std::function<RationalTime()> &currentTimeProvider,
    const std::function<void(const QString &)> &keyframeChanged,
    const ArtifactAbstractLayerPtr &layer, bool *addedAny = nullptr,
    const QString &registryScope = QString(),
    QMultiHash<QString, ArtifactPropertyEditorRowWidget *> *registry = nullptr,
    std::vector<ArtifactPropertyEditorRowWidget *> *collectedRows = nullptr,
    const std::function<void(ArtifactPropertyEditorRowWidget *,
                             const std::shared_ptr<ArtifactCore::AbstractProperty> &)>
        &decorateRow = {},
    const std::function<void(
        ArtifactPropertyEditorRowWidget *,
        const std::shared_ptr<ArtifactCore::AbstractProperty> &,
        const QVariant &)> &rowValueChanged = {});

} // namespace detail

using namespace detail;

namespace detail {

PropertyPresentationProfile
propertyPresentationProfile(const ArtifactAbstractLayerPtr &layer) {
  if (layer && layer->getProperty(QStringLiteral("solid.color"))) {
    return {QStringLiteral("solid"),
            {QStringLiteral("Initial"), QStringLiteral("Transform"),
             QStringLiteral("Solid")}};
  }
  return {QStringLiteral("basic"),
          {QStringLiteral("Initial"), QStringLiteral("Transform")}};
}

bool presentationAllowsGroup(const PropertyPresentationProfile &profile,
                             const QString &groupName) {
  if (profile.visibleGroups.isEmpty()) {
    return true;
  }
  return profile.visibleGroups.contains(groupName.trimmed(),
                                        Qt::CaseInsensitive);
}

void applyPresentationPropertyRules(
    const PropertyPresentationProfile &profile, const QString &groupName,
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties) {
  if (profile.id != QStringLiteral("solid") ||
      groupName.compare(QStringLiteral("Solid"), Qt::CaseInsensitive) != 0) {
    return;
  }

  const auto fillMode = std::find_if(
      properties.begin(), properties.end(), [](const auto &property) {
        return property && property->getName().compare(
                               QStringLiteral("solid.fillType"),
                               Qt::CaseInsensitive) == 0;
      });
  const bool usesGradient =
      fillMode != properties.end() && (*fillMode)->getValue().toInt() != 0;
  if (usesGradient) {
    return;
  }

  std::erase_if(properties, [](const auto &property) {
    if (!property) {
      return true;
    }
    const QString name = property->getName();
    return name.compare(QStringLiteral("solid.color"),
                        Qt::CaseInsensitive) != 0 &&
           name.compare(QStringLiteral("solid.fillType"),
                        Qt::CaseInsensitive) != 0;
  });
}

} // namespace detail


W_OBJECT_IMPL(ArtifactPropertyWidget)

class ArtifactPropertyWidget::Impl {
public:
  ArtifactPropertyWidget *owner = nullptr;
  QWidget *containerWidget = nullptr;
  QVBoxLayout *mainLayout = nullptr;
  ArtifactAbstractLayerPtr currentLayer;
  std::vector<ArtifactAbstractEffectPtr> compositionEffects;
  QSet<ArtifactAbstractLayerPtr> targetLayers;
  QMetaObject::Connection currentLayerChangedConnection;
  QTimer *rebuildTimer = nullptr;
  QTimer *updateValuesTimer = nullptr;
  int rebuildDebounceMs = 80;
  int updateValuesDebounceMs = 16;
  QString filterText;
  QString focusedEffectId;
  bool rebuilding = false;
  bool needsRebuildWhenVisible = false;
  bool valueColumnFirst = false;
  bool sliderBeforeValue = false;
  bool favoriteOnly = false;
  bool isPlaying = false;
  int localPropertyEditDepth = 0;
  QMultiHash<QString, ArtifactPropertyEditorRowWidget *> propertyEditors;
  QString rebuildSignature;
  QString pendingScrollGroupName;
  qint64 lastPropertyUpdateFramePosition = std::numeric_limits<qint64>::min();
  int64_t lastPropertyUpdateFps = -1;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void scheduleRebuild(int delayMs = -1) {
    if (!rebuildTimer) {
      return;
    }
    if (updateValuesTimer) {
      updateValuesTimer->stop();
    }
    const int delay = (delayMs < 0) ? rebuildDebounceMs : delayMs;
    rebuildTimer->start(std::max(0, delay));
  }

  void scheduleUpdateValues() {
    if (!updateValuesTimer || rebuilding) {
      return;
    }
    // If a full rebuild is already scheduled, don't bother with partial update
    if (rebuildTimer && rebuildTimer->isActive()) {
      return;
    }
    invalidatePropertyValueCache();
    updateValuesTimer->start(updateValuesDebounceMs);
  }

  QString computeRebuildSignature() const;
  void invalidatePropertyValueCache();
  void rebuildUI();
  void scrollToGroupByName(const QString &groupName);
  void updatePropertyValues();
  void applyLockState();
  ArtifactPropertyEditorRowWidget* activeExpressionRow() const;
  std::shared_ptr<ArtifactCore::AbstractProperty> resolveRowProperty(
      const ArtifactPropertyEditorRowWidget *row) const;
  void openExpressionCopilotForProperty(
      const QString& propertyName,
      const std::shared_ptr<ArtifactCore::AbstractProperty>& propertyPtr,
      const QString& initialExpression,
      const RationalTime& currentTime);
};

void ArtifactPropertyWidget::showEvent(QShowEvent *event) {
  QScrollArea::showEvent(event);
  if (impl_->needsRebuildWhenVisible) {
    impl_->needsRebuildWhenVisible = false;
    QTimer::singleShot(0, this, [this]() {
      if (impl_) {
        impl_->rebuildUI();
      }
    });
  } else {
    impl_->updatePropertyValues();
  }
}

std::shared_ptr<ArtifactCore::AbstractProperty>
ArtifactPropertyWidget::Impl::resolveRowProperty(
    const ArtifactPropertyEditorRowWidget *row) const {
  if (!row || !currentLayer) {
    return {};
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  if (auto propertyPtr = currentLayer->getProperty(propertyName)) {
    return propertyPtr;
  }

  const auto effects = currentLayer->getEffects();
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    if (!focusedEffectId.isEmpty() &&
        effect->effectID().toQString() != focusedEffectId) {
      continue;
    }

    const QString effectName = effect->displayName().toQString();
    for (const auto &property : effect->getProperties()) {
      if (property.getName().compare(propertyName, Qt::CaseInsensitive) != 0) {
        continue;
      }
      if (!propertyScope.isEmpty() &&
          effectName.compare(propertyScope, Qt::CaseInsensitive) != 0 &&
          QStringLiteral("%1/%2").arg(effectName, propertyName)
              .compare(propertyScope, Qt::CaseInsensitive) != 0) {
        continue;
      }
      return std::make_shared<ArtifactCore::AbstractProperty>(property);
    }
  }

  return {};
}

QString ArtifactPropertyWidget::Impl::computeRebuildSignature() const {
  QString signature;
  signature.reserve(2048);

  signature += QStringLiteral("layers:");
  {
    QStringList ids;
    for (const auto &tl : targetLayers) {
      if (tl) { ids.push_back(tl->id().toString()); }
    }
    ids.sort();
    signature += ids.join(QLatin1Char(','));
  }
  signature += QLatin1Char('\n');

  signature += QStringLiteral("layer:");
  signature +=
      currentLayer ? currentLayer->id().toString() : QStringLiteral("<none>");
  signature += QLatin1Char('\n');

  signature += QStringLiteral("filter:");
  signature += filterText.trimmed();
  signature += QLatin1Char('\n');

  signature += QStringLiteral("focused:");
  signature += focusedEffectId.trimmed();
  signature += QLatin1Char('\n');

  signature += QStringLiteral("composition_effects:");
  signature += QString::number(compositionEffects.size());
  signature += QLatin1Char('\n');
  for (const auto &effect : compositionEffects) {
    if (!effect) {
      continue;
    }
    signature += QStringLiteral("composition_effect:");
    signature += effect->effectID().toQString();
    signature += QLatin1Char('|');
    signature += effect->displayName().toQString();
    signature += QLatin1Char('|');
    signature += QString::number(static_cast<int>(effect->pipelineStage()));
    signature += QLatin1Char('\n');
    for (const auto &property : effect->getProperties()) {
      signature += QStringLiteral("composition_effect_property:");
      signature += property.getName();
      signature += QLatin1Char('\n');
    }
  }

  signature += QStringLiteral("valueColumnFirst:");
  signature += valueColumnFirst ? QStringLiteral("1") : QStringLiteral("0");
  signature += QLatin1Char('\n');

  signature += QStringLiteral("sliderBeforeValue:");
  signature += sliderBeforeValue ? QStringLiteral("1") : QStringLiteral("0");
  signature += QLatin1Char('\n');

  signature += QStringLiteral("favoriteOnly:");
  signature += favoriteOnly ? QStringLiteral("1") : QStringLiteral("0");
  signature += QLatin1Char('\n');

  if (!currentLayer) {
    return signature;
  }

  const auto focused = focusedEffectId.trimmed();

  const auto layerGroups = currentLayer->getLayerPropertyGroups();
  for (const auto &groupDef : layerGroups) {
    signature += QStringLiteral("group:");
    signature += groupDef.name();
    signature += QLatin1Char('\n');

    const auto sortedProps = groupDef.sortedProperties();
    signature += QStringLiteral("group_count:");
    signature += QString::number(sortedProps.size());
    signature += QLatin1Char('\n');
    for (const auto &property : sortedProps) {
      if (!property) {
        continue;
      }
      signature += QStringLiteral("property:");
      signature += property->getName();
      signature += QLatin1Char('\n');
      if (property->getName().compare(QStringLiteral("solid.fillType"),
                                      Qt::CaseInsensitive) == 0) {
        signature += QStringLiteral("structural_value:");
        signature += property->getValue().toString();
        signature += QLatin1Char('\n');
      }
    }
  }

  const auto effects = currentLayer->getEffects();
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    if (!focused.isEmpty() && effect->effectID().toQString() != focused) {
      continue;
    }

    signature += QStringLiteral("effect:");
    signature += effect->effectID().toQString();
    signature += QLatin1Char('\n');

    signature += QStringLiteral("effect_name:");
    signature += effect->displayName().toQString();
    signature += QLatin1Char('\n');

    signature += QStringLiteral("effect_stage:");
    signature += QString::number(static_cast<int>(effect->pipelineStage()));
    signature += QLatin1Char('\n');

    ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
    for (const auto &property : effect->getProperties()) {
      propGroup.addProperty(
          std::make_shared<ArtifactCore::AbstractProperty>(property));
    }

    const auto effectSummary =
        prioritizedSummaryProperties(propGroup.sortedProperties(), {}, 3);
    signature += QStringLiteral("effect_summary_count:");
    signature += QString::number(effectSummary.size());
    signature += QLatin1Char('\n');
    for (const auto &property : effectSummary) {
      if (!property) {
        continue;
      }
      signature += QStringLiteral("effect_summary_property:");
      signature += property->getName();
      signature += QLatin1Char('\n');
    }
  }

  return signature;
}

ArtifactPropertyWidget::ArtifactPropertyWidget(QWidget *parent)
    : QScrollArea(parent), impl_(new Impl()) {
  impl_->owner = this;
  ArtifactPropertyEditorRowWidget::setGlobalLayoutMode(
      impl_->valueColumnFirst ? ArtifactPropertyRowLayoutMode::EditorThenLabel
                              : ArtifactPropertyRowLayoutMode::LabelThenEditor);
  setGlobalNumericEditorLayoutMode(
      impl_->sliderBeforeValue
          ? ArtifactNumericEditorLayoutMode::SliderThenValue
          : ArtifactNumericEditorLayoutMode::ValueThenSlider);
  setObjectName(QStringLiteral("artifactPropertyWidget"));
  setMinimumWidth(360);
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  applyPropertyPanelPalette(this);
  setContextMenuPolicy(Qt::CustomContextMenu);
  impl_->containerWidget = new QWidget(this);
  impl_->containerWidget->setObjectName(
      QStringLiteral("artifactPropertyContainer"));
  applyPropertyPanelPalette(impl_->containerWidget);
  impl_->mainLayout = new QVBoxLayout(impl_->containerWidget);
  impl_->mainLayout->setAlignment(Qt::AlignTop);
  impl_->mainLayout->setContentsMargins(8, 8, 8, 10);
  impl_->mainLayout->setSpacing(6);

  setWidget(impl_->containerWidget);
  if (viewport()) {
    applyPropertyPanelPalette(viewport());
  }
  impl_->rebuildTimer = new QTimer(this);
  impl_->rebuildTimer->setSingleShot(true);
  QObject::connect(impl_->rebuildTimer, &QTimer::timeout, this,
                   [this]() { impl_->rebuildUI(); });

  impl_->updateValuesTimer = new QTimer(this);
  impl_->updateValuesTimer->setSingleShot(true);
  QObject::connect(impl_->updateValuesTimer, &QTimer::timeout, this,
                   [this]() { impl_->updatePropertyValues(); });
  QObject::connect(UndoManager::instance(), &UndoManager::historyChanged, this, [this]() {
    impl_->scheduleUpdateValues();
  });

  QObject::connect(this, &QWidget::customContextMenuRequested, this,
                   [this](const QPoint &pos) {
                     QMenu menu(this);
                     QAction *sliderBeforeValueAct =
                         menu.addAction(QStringLiteral("Slider before value"));
                     sliderBeforeValueAct->setCheckable(true);
                     sliderBeforeValueAct->setChecked(impl_->sliderBeforeValue);
                     QObject::connect(sliderBeforeValueAct, &QAction::toggled,
                                      this, [this](bool checked) {
                                        setSliderBeforeValue(checked);
                                      });
                     menu.exec(mapToGlobal(pos));
                   });
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<PlaybackStateChangedEvent>(
          [this](const PlaybackStateChangedEvent &event) {
            impl_->isPlaying = event.state == PlaybackState::Playing;
            if (!impl_->isPlaying) {
              impl_->updatePropertyValues();
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<FrameChangedEvent>(
          [this](const FrameChangedEvent &) {
            if (isVisible()) {
              // [Optimization] If playing, only update if it's the first frame
              // of playback or not playing.
              // High-frequency UI updates during playback can cause
              // significant lag.
              if (!impl_->isPlaying) {
                impl_->updatePropertyValues();
              }
            } else {
              impl_->needsRebuildWhenVisible = true;
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        if (impl_->currentLayer) {
          impl_->scheduleRebuild();
        }
      }));
}

ArtifactPropertyWidget::~ArtifactPropertyWidget() {
  if (impl_->currentLayerChangedConnection) {
    QObject::disconnect(impl_->currentLayerChangedConnection);
  }
  delete impl_;
}

void ArtifactPropertyWidget::Impl::invalidatePropertyValueCache() {
  lastPropertyUpdateFramePosition = std::numeric_limits<qint64>::min();
  lastPropertyUpdateFps = -1;
}

QSize ArtifactPropertyWidget::sizeHint() const { return QSize(300, 600); }

void ArtifactPropertyWidget::setLayer(ArtifactAbstractLayerPtr layer) {
  if (impl_->currentLayer == layer && impl_->targetLayers.size() <= 1 &&
      impl_->compositionEffects.empty()) {
    // Same layer object can still need a full rebuild when visibility or groups change.
    impl_->invalidatePropertyValueCache();
    updateProperties();
    return;
  }

  if (impl_->currentLayerChangedConnection) {
    QObject::disconnect(impl_->currentLayerChangedConnection);
  }

  impl_->currentLayer = layer;
  impl_->compositionEffects.clear();
  impl_->targetLayers.clear();
  if (layer) {
    impl_->targetLayers.insert(layer);
  }
  impl_->invalidatePropertyValueCache();

  // Connect to new layer
  if (impl_->currentLayer) {
    impl_->currentLayerChangedConnection =
        connect(impl_->currentLayer.get(), &ArtifactAbstractLayer::changed,
                this, [this]() {
                  if (impl_->localPropertyEditDepth > 0) {
                    return;
                  }
                  impl_->invalidatePropertyValueCache();
                  impl_->scheduleRebuild();
                });
  }

  updateProperties();
}

void ArtifactPropertyWidget::setCompositionEffects(
    const std::vector<std::shared_ptr<ArtifactAbstractEffect>>& effects) {
  if (impl_->currentLayerChangedConnection) {
    QObject::disconnect(impl_->currentLayerChangedConnection);
    impl_->currentLayerChangedConnection = {};
  }

  impl_->currentLayer = nullptr;
  impl_->targetLayers.clear();
  impl_->compositionEffects = effects;
  impl_->invalidatePropertyValueCache();
  updateProperties();
}

void ArtifactPropertyWidget::setLayers(const QSet<ArtifactAbstractLayerPtr>& layers) {
  if (layers.isEmpty()) {
    clear();
    return;
  }

  if (impl_->currentLayerChangedConnection) {
    QObject::disconnect(impl_->currentLayerChangedConnection);
  }

  // Use first layer as the primary for display
  auto primary = *layers.begin();
  impl_->currentLayer = primary;
  impl_->compositionEffects.clear();
  impl_->targetLayers = layers;
  impl_->invalidatePropertyValueCache();

  if (primary) {
    impl_->currentLayerChangedConnection =
        connect(primary.get(), &ArtifactAbstractLayer::changed,
                this, [this]() {
                  if (impl_->localPropertyEditDepth > 0) {
                    return;
                  }
                  impl_->invalidatePropertyValueCache();
                  impl_->scheduleRebuild();
                });
  }

  updateProperties();
}

int ArtifactPropertyWidget::targetLayersCount() const {
  return static_cast<int>(impl_->targetLayers.size());
}

void ArtifactPropertyWidget::setFocusedEffectId(const QString &effectId) {
  if (impl_->focusedEffectId == effectId)
    return;
  impl_->focusedEffectId = effectId;
  updateProperties();
}

void ArtifactPropertyWidget::clear() {
  impl_->currentLayer = nullptr;
  impl_->compositionEffects.clear();
  impl_->targetLayers.clear();
  impl_->focusedEffectId.clear();
  impl_->invalidatePropertyValueCache();
  updateProperties();
}

void ArtifactPropertyWidget::setFilterText(const QString &text) {
  if (impl_->filterText == text)
    return;
  impl_->filterText = text;
  updateProperties();
}

QString ArtifactPropertyWidget::filterText() const { return impl_->filterText; }

void ArtifactPropertyWidget::setValueColumnFirst(const bool enabled) {
  if (impl_->valueColumnFirst == enabled) {
    return;
  }
  impl_->valueColumnFirst = enabled;
  ArtifactPropertyEditorRowWidget::setGlobalLayoutMode(
      enabled ? ArtifactPropertyRowLayoutMode::EditorThenLabel
              : ArtifactPropertyRowLayoutMode::LabelThenEditor);
  updateProperties();
}

bool ArtifactPropertyWidget::valueColumnFirst() const {
  return impl_->valueColumnFirst;
}

void ArtifactPropertyWidget::setSliderBeforeValue(const bool enabled) {
  if (impl_->sliderBeforeValue == enabled) {
    return;
  }
  impl_->sliderBeforeValue = enabled;
  setGlobalNumericEditorLayoutMode(
      enabled ? ArtifactNumericEditorLayoutMode::SliderThenValue
              : ArtifactNumericEditorLayoutMode::ValueThenSlider);
  updateProperties();
}

bool ArtifactPropertyWidget::sliderBeforeValue() const {
  return impl_->sliderBeforeValue;
}

void ArtifactPropertyWidget::setFavoriteOnly(const bool enabled) {
  if (impl_->favoriteOnly == enabled)
    return;
  impl_->favoriteOnly = enabled;
  updateProperties();
}

bool ArtifactPropertyWidget::favoriteOnly() const {
  return impl_->favoriteOnly;
}

void ArtifactPropertyWidget::updateProperties() { impl_->scheduleRebuild(80); }

bool ArtifactPropertyWidget::openActiveExpressionCopilot() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr) {
    return false;
  }

  const QString initialExpression = propertyPtr->hasExpression()
      ? propertyPtr->getExpression()
      : (row->editor() ? row->editor()->value().toString() : QString{});
  const auto playback = ArtifactPlaybackService::instance();
  const RationalTime currentTime =
      currentPlaybackTime(playback, impl_->currentLayer);
  impl_->openExpressionCopilotForProperty(propertyName, propertyPtr,
                                          initialExpression, currentTime);
  return true;
}

bool ArtifactPropertyWidget::clearActiveExpression() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr) {
    return false;
  }

  propertyPtr->setExpression(QString{});
  if (impl_->currentLayer) {
    notifyLayerPropertyAnimationChanged(impl_->currentLayer);
  }
  impl_->scheduleRebuild(0);
  impl_->scheduleUpdateValues();
  return true;
}

bool ArtifactPropertyWidget::convertActiveExpressionToKeyframes() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr || !propertyPtr->hasExpression()) {
    return false;
  }

  auto *composition =
      static_cast<ArtifactAbstractComposition *>(impl_->currentLayer->composition());
  const int fps = composition
                      ? std::max<int>(1, static_cast<int>(
                                              std::lround(composition->frameRate().framerate())))
                      : 30;
  int64_t startFrame = impl_->currentLayer->inPoint().framePosition();
  int64_t endFrame = impl_->currentLayer->outPoint().framePosition();
  if (endFrame <= startFrame) {
    endFrame = startFrame + 1;
  }

  ArtifactCore::ExpressionEvaluator evaluator;
  std::vector<std::pair<RationalTime, QVariant>> sampledKeyframes;
  sampledKeyframes.reserve(static_cast<size_t>(endFrame - startFrame));
  for (int64_t frame = startFrame; frame < endFrame; ++frame) {
    const RationalTime time(frame, fps);
    const QVariant sampledValue = propertyPtr->evaluateValue(time, &evaluator);
    sampledKeyframes.emplace_back(time, sampledValue);
  }

  if (sampledKeyframes.empty()) {
    return false;
  }

  propertyPtr->clearKeyFrames();
  propertyPtr->setExpression(QString{});
  for (const auto &[time, value] : sampledKeyframes) {
    propertyPtr->addKeyFrame(time, value);
  }

  if (impl_->currentLayer) {
    notifyLayerPropertyAnimationChanged(impl_->currentLayer);
  }
  impl_->scheduleRebuild(0);
  impl_->scheduleUpdateValues();
  return true;
}

bool ArtifactPropertyWidget::bakeActivePropertyToKeyframes() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr || !propertyPtr->isAnimatable()) {
    return false;
  }

  auto *composition =
      static_cast<ArtifactAbstractComposition *>(impl_->currentLayer->composition());
  const int fps = composition
                      ? std::max<int>(1, static_cast<int>(
                                              std::lround(composition->frameRate().framerate())))
                      : 30;
  int64_t startFrame = impl_->currentLayer->inPoint().framePosition();
  int64_t endFrame = impl_->currentLayer->outPoint().framePosition();
  if (composition) {
    startFrame = std::max<int64_t>(startFrame, composition->frameRange().start());
    endFrame = std::min<int64_t>(endFrame, composition->frameRange().end());
  }
  if (endFrame <= startFrame) {
    endFrame = startFrame + 1;
  }

  const auto previousFrame = composition ? composition->framePosition() : FramePosition(startFrame);
  std::vector<std::pair<RationalTime, QVariant>> sampledKeyframes;
  sampledKeyframes.reserve(static_cast<size_t>(endFrame - startFrame));

  for (int64_t frame = startFrame; frame < endFrame; ++frame) {
    const RationalTime time(frame, fps);
    if (composition) {
      composition->goToFrame(frame);
    } else {
      impl_->currentLayer->goToFrame(frame);
    }
    const QVariant sampledValue = propertyPtr->getValue();
    sampledKeyframes.emplace_back(time, sampledValue);
  }

  if (composition) {
    composition->goToFrame(previousFrame.framePosition());
  } else {
    impl_->currentLayer->goToFrame(previousFrame.framePosition());
  }

  if (sampledKeyframes.empty()) {
    return false;
  }

  propertyPtr->clearKeyFrames();
  for (const auto &[time, value] : sampledKeyframes) {
    propertyPtr->addKeyFrame(time, value);
  }

  if (impl_->currentLayer) {
    notifyLayerPropertyAnimationChanged(impl_->currentLayer);
  }
  impl_->scheduleRebuild(0);
  impl_->scheduleUpdateValues();
  return true;
}

bool ArtifactPropertyWidget::saveActiveExpressionPreset() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr) {
    return false;
  }

  const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
      + QStringLiteral("/presets");
  QDir().mkpath(defaultDir);
  const QString filePath = QFileDialog::getSaveFileName(
      this,
      QStringLiteral("Save Animation Preset"),
      defaultDir,
      QStringLiteral("Animation Preset (*.json)"));
  if (filePath.isEmpty()) {
    return false;
  }

  QJsonObject root;
  root[QStringLiteral("kind")] = QStringLiteral("artifact.animation.property");
  root[QStringLiteral("propertyName")] = propertyName;
  if (!propertyScope.isEmpty()) {
    root[QStringLiteral("propertyScope")] = propertyScope;
  }
  root[QStringLiteral("layerId")] = impl_->currentLayer->id().toString();
  root[QStringLiteral("focusedEffectId")] = impl_->focusedEffectId;

  const auto serialized = ArtifactCore::PropertySerializationBridge::serializeProperty(propertyPtr);
  QJsonObject propertyObj;
  propertyObj[QStringLiteral("name")] = serialized.name;
  propertyObj[QStringLiteral("type")] = serialized.type;
  propertyObj[QStringLiteral("value")] = serialized.value;
  if (!serialized.expression.isEmpty()) {
    propertyObj[QStringLiteral("expression")] = serialized.expression;
  }
  if (!serialized.keyframes.isEmpty()) {
    propertyObj[QStringLiteral("keyframes")] = serialized.keyframes;
  }
  if (!serialized.metadata.isEmpty()) {
    propertyObj[QStringLiteral("metadata")] = serialized.metadata;
  }
  root[QStringLiteral("property")] = propertyObj;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

bool ArtifactPropertyWidget::loadActiveExpressionPreset() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  const QString propertyScope = row->property("propertyScope").toString();
  auto propertyPtr = impl_->resolveRowProperty(row);
  if (!propertyPtr) {
    return false;
  }

  const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
      + QStringLiteral("/presets");
  const QString filePath = QFileDialog::getOpenFileName(
      this,
      QStringLiteral("Load Animation Preset"),
      defaultDir,
      QStringLiteral("Animation Preset (*.json)"));
  if (filePath.isEmpty()) {
    return false;
  }

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return false;
  }
  const QJsonObject root = doc.object();
  const QString savedScope = root.value(QStringLiteral("propertyScope")).toString();
  if (!savedScope.isEmpty() &&
      (propertyScope.isEmpty() ||
       savedScope.compare(propertyScope, Qt::CaseInsensitive) != 0)) {
    return false;
  }
  const QJsonObject propertyObj = root.value(QStringLiteral("property")).toObject();
  if (propertyObj.isEmpty()) {
    return false;
  }

  ArtifactCore::SerializedProperty serialized;
  serialized.name = propertyObj.value(QStringLiteral("name")).toString();
  serialized.type = propertyObj.value(QStringLiteral("type")).toInt();
  serialized.value = propertyObj.value(QStringLiteral("value"));
  serialized.expression = propertyObj.value(QStringLiteral("expression")).toString();
  serialized.keyframes = propertyObj.value(QStringLiteral("keyframes")).toArray();
  serialized.metadata = propertyObj.value(QStringLiteral("metadata")).toObject();

  ArtifactCore::PropertySerializationBridge::deserializeProperty(propertyPtr, serialized);
  if (impl_->currentLayer) {
    notifyLayerPropertyAnimationChanged(impl_->currentLayer);
  }
  impl_->scheduleRebuild(0);
  impl_->scheduleUpdateValues();
  return true;
}

bool ArtifactPropertyWidget::hasActiveExpressionTarget() const {
  if (!impl_) {
    return false;
  }
  return impl_->activeExpressionRow() != nullptr;
}

QString ArtifactPropertyWidget::activePropertyPath() const {
  if (!impl_) {
    return {};
  }
  auto *row = impl_->activeExpressionRow();
  return row ? row->propertyName() : QString{};
}

Artifact::ArtifactAbstractLayerPtr ArtifactPropertyWidget::activePropertyLayer() const {
  if (!impl_) {
    return {};
  }
  return impl_->currentLayer;
}

void ArtifactPropertyWidget::Impl::updatePropertyValues() {
  if (!currentLayer || rebuilding)
    return;

  auto *playback = ArtifactPlaybackService::instance();
  const auto now = currentPlaybackTime(playback, currentLayer);
  const int64_t fps_val = std::max<int64_t>(1, now.scale());
  const qint64 framePosition = now.value();
  if (framePosition == lastPropertyUpdateFramePosition &&
      fps_val == lastPropertyUpdateFps) {
    applyLockState();
    return;
  }
  lastPropertyUpdateFramePosition = framePosition;
  lastPropertyUpdateFps = fps_val;

  for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    auto *row = it.value();
    if (!row)
      continue;

    auto propertyPtr = resolveRowProperty(row);
    if (!propertyPtr)
      continue;

    auto *editor = row->editor();
    if (!editor)
      continue;

    // アニメーション値を計算して反映
    const QVariant animatedValue = propertyPtr->interpolateValue(now);
    if (animatedValue.isValid() && !editor->hasFocus()) {
      editor->setValueFromVariant(animatedValue);
    }

    updateScaleSupplementaryText(row, currentLayer, propertyPtr,
                                 animatedValue.isValid() ? animatedValue
                                                         : propertyPtr->getValue());

    const bool hasAnyKeyframes = !propertyPtr->getKeyFrames().empty();
    row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
    row->setKeyframeModeEnabled(row->isKeyframeModeEnabled() ||
                                propertyPtr->hasKeyFrameAt(now));
    row->setNavigationEnabled(hasAnyKeyframes || propertyPtr->isAnimatable());
  }

  applyLockState();
}

void ArtifactPropertyWidget::Impl::applyLockState() {
  if (!currentLayer) {
    return;
  }

  const bool locked = currentLayer->isLocked();
  for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    auto *row = it.value();
    if (!row) {
      continue;
    }
    const bool isLockRow = row->propertyName().compare(
                               QStringLiteral("layer.locked"),
                               Qt::CaseInsensitive) == 0;
    row->setEnabled(!locked || isLockRow);
  }
}

ArtifactPropertyEditorRowWidget* ArtifactPropertyWidget::Impl::activeExpressionRow() const {
  if (!currentLayer) {
    return nullptr;
  }

  QWidget* focus = QApplication::focusWidget();
  for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    auto* row = it.value();
    if (!row || !row->isVisible()) {
      continue;
    }
    auto property = resolveRowProperty(row);
    if (!property || !property->isAnimatable()) {
      continue;
    }
    if (focus && (row == focus || row->isAncestorOf(focus))) {
      return row;
    }
    if (row->hasFocus()) {
      return row;
    }
    if (row->editor() && row->editor()->hasFocus()) {
      return row;
    }
  }
  return nullptr;
}

void ArtifactPropertyWidget::Impl::openExpressionCopilotForProperty(
    const QString& propertyName,
    const std::shared_ptr<ArtifactCore::AbstractProperty>& propertyPtr,
    const QString& initialExpression,
    const RationalTime& currentTime) {
  if (!propertyPtr) {
    return;
  }

  launchExpressionCopilot(
      owner,
      propertyName,
      propertyPtr,
      initialExpression,
      currentLayer,
      currentTime,
      [this](const QString &) {
        if (currentLayer) {
          notifyLayerPropertyAnimationChanged(currentLayer);
        }
        scheduleRebuild(0);
        scheduleUpdateValues();
      });
}

void ArtifactPropertyWidget::Impl::rebuildUI() {
  if (rebuilding)
    return;

  // 1. 可視性チェック: 非表示なら後回しにする
  if (!owner->isVisible()) {
    needsRebuildWhenVisible = true;
    return;
  }

  rebuilding = true;

  const QString nextSignature = computeRebuildSignature();
  if (nextSignature == rebuildSignature) {
    rebuilding = false;
    updatePropertyValues();
    if (!pendingScrollGroupName.isEmpty()) {
      QTimer::singleShot(0, owner, [this]() { scrollToGroupByName(pendingScrollGroupName); });
      pendingScrollGroupName.clear();
    }
    return;
  }
  rebuildSignature = nextSignature;
  invalidatePropertyValueCache();

  clearLayoutRecursive(mainLayout);
  propertyEditors.clear();
  const bool embeddedComponentEditor =
      owner && owner->property("artifactEmbeddedComponentEditor").toBool();

  if (!currentLayer && compositionEffects.empty()) {
    QLabel *emptyLabel = new QLabel("Select a layer or composition effect to edit properties");
    emptyLabel->setObjectName(QStringLiteral("propertyEmptyLabel"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    applyPropertySectionLabel(emptyLabel, true);
    mainLayout->addWidget(emptyLabel);
    rebuilding = false;
    return;
  }

  if (currentLayer && !embeddedComponentEditor) {
    const QString layerName = currentLayer->layerName().trimmed();
    const QString selectionText = targetLayers.size() > 1
        ? QStringLiteral("%1 Layers Selected").arg(targetLayers.size())
        : (layerName.isEmpty() ? QStringLiteral("Layer Properties") : layerName);
    auto *selectionHeader = new QLabel(
        selectionText);
    selectionHeader->setObjectName(QStringLiteral("propertySelectionHeader"));
    selectionHeader->setMinimumHeight(38);
    selectionHeader->setContentsMargins(10, 4, 10, 4);
    QFont headerFont = selectionHeader->font();
    headerFont.setPointSize(12);
    headerFont.setWeight(QFont::DemiBold);
    selectionHeader->setFont(headerFont);
    applyPropertySectionLabel(selectionHeader, true);
    mainLayout->addWidget(selectionHeader);
  }

  if (!embeddedComponentEditor) {
    auto *searchEdit = new QLineEdit();
    searchEdit->setObjectName(QStringLiteral("propertyFilterEdit"));
    searchEdit->setPlaceholderText("Search properties...");
    searchEdit->setText(filterText);
    applyPropertySearchPalette(searchEdit);
    QObject::connect(searchEdit, &QLineEdit::textChanged,
                     [this](const QString &text) {
                       filterText = text;
                       scheduleRebuild(80);
                     });

    // Search and favorite filtering are one local browsing decision.
    auto *favRow = new QWidget();
    favRow->setObjectName(QStringLiteral("propertySearchRow"));
    auto *favLayout = new QHBoxLayout(favRow);
    favLayout->setContentsMargins(0, 0, 0, 0);
    favLayout->setSpacing(6);
    favLayout->addWidget(searchEdit, 1);
    auto *favToggle = new QPushButton(QStringLiteral("Favorites"));
    favToggle->setCheckable(true);
    favToggle->setChecked(favoriteOnly);
    favToggle->setObjectName(QStringLiteral("favoriteToggleButton"));
    favToggle->setFlat(true);
    favToggle->setCursor(Qt::PointingHandCursor);
    favToggle->setToolTip(QStringLiteral("Show only favorite properties"));
    favLayout->addWidget(favToggle, 0);
    QObject::connect(favToggle, &QPushButton::toggled, owner,
                     [this](bool checked) {
                       if (owner) {
                         owner->setFavoriteOnly(checked);
                       }
                     });
    mainLayout->addWidget(favRow);
  }

  if (currentLayer && !embeddedComponentEditor) {
    auto *stateRow = new QWidget();
    stateRow->setObjectName(QStringLiteral("layerStateToggleRow"));
    // Visibility/lock/timing state already belongs to the layer panel. Keep
    // the controls alive for compatibility, but do not spend Property Editor
    // vertical space on a duplicate toolbar.
    stateRow->setVisible(false);
    auto *stateLayout = new QHBoxLayout(stateRow);
    stateLayout->setContentsMargins(4, 2, 4, 2);
    stateLayout->setSpacing(4);

    for (const auto &toggleDef : kLayerStateToggleDefs) {
      const auto property = currentLayer->getProperty(QString::fromLatin1(toggleDef.propertyName));
      if (!property ||
          property->getType() != ArtifactCore::PropertyType::Boolean) {
        continue;
      }
      auto *button = new QToolButton(stateRow);
      button->setCheckable(true);
      button->setAutoRaise(true);
      button->setToolButtonStyle(Qt::ToolButtonTextOnly);
      button->setText(QString::fromLatin1(toggleDef.label));
      button->setToolTip(QString::fromLatin1(toggleDef.tooltip));
      button->setObjectName(QStringLiteral("layerStateToggleButton"));
      button->setChecked(property->getValue().toBool());
      button->setCursor(Qt::PointingHandCursor);
      button->setMinimumWidth(56);
      stateLayout->addWidget(button);
      QObject::connect(button, &QToolButton::toggled, stateRow,
                       [this, propertyName = QString::fromLatin1(toggleDef.propertyName)](bool checked) {
                         if (!currentLayer) {
                           return;
                         }
                         for (const auto &layer : targetLayers) {
                           if (!layer) {
                             continue;
                           }
                           layer->setLayerPropertyValue(propertyName, checked);
                           notifyLayerPropertyAnimationChanged(layer);
                         }
                         scheduleRebuild(0);
                       });
    }
    stateLayout->addStretch();
    mainLayout->addWidget(stateRow);
  }

  if (!currentLayer) {
    bool hasAnyCompositionEffectProperties = false;
    const bool hasFocusedEffect = !focusedEffectId.trimmed().isEmpty();
    const auto currentCompositionEffectTime = []() { return RationalTime(0, 1); };

    if (hasFocusedEffect) {
      auto *focusedLabel = new QLabel(
          QStringLiteral("Focused Composition Effect ID: %1").arg(focusedEffectId));
      focusedLabel->setObjectName(QStringLiteral("propertySectionLabel"));
      applyPropertySectionLabel(focusedLabel, true);
      applyThemeTextPalette(focusedLabel, 110);
      mainLayout->addWidget(focusedLabel);
    }

    for (const auto &effect : compositionEffects) {
      if (!effect) {
        continue;
      }
      if (hasFocusedEffect && effect->effectID().toQString() != focusedEffectId) {
        continue;
      }

      const auto presentation = describeEffectPresentation(effect);
      QGroupBox *group = new QGroupBox(
          QStringLiteral("Composition / %1").arg(presentation.headingText));
      auto *groupLayout = new QVBoxLayout(group);
      groupLayout->setContentsMargins(10, 8, 10, 8);
      groupLayout->setSpacing(5);
      applyPropertySectionBox(group);
      applyThemeTextPalette(group, 120);

      auto *stageLabel = new QLabel(presentation.stageNoteText, group);
      stageLabel->setObjectName(QStringLiteral("propertySectionNote"));
      stageLabel->setWordWrap(true);
      applyPresentationToneLabel(stageLabel, presentation.badgeTone, false);
      groupLayout->addWidget(stageLabel);

      ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
      for (const auto &p : effect->getProperties()) {
        propGroup.addProperty(
            std::make_shared<ArtifactCore::AbstractProperty>(p));
      }

      auto sortedProps = propGroup.sortedProperties();
      if (favoriteOnly) {
        const auto favs = loadFavoriteProperties();
        if (!favs.isEmpty()) {
          std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> favFiltered;
          favFiltered.reserve(sortedProps.size());
          for (const auto &p : sortedProps) {
            if (p && favs.contains(p->getName(), Qt::CaseInsensitive)) {
              favFiltered.push_back(p);
            }
          }
          sortedProps.swap(favFiltered);
        } else {
          sortedProps.clear();
        }
      }
      if (sortedProps.empty()) {
        delete group;
        continue;
      }

      bool addedGroupProperties = false;
      std::vector<ArtifactPropertyEditorRowWidget *> effectRows;
      addRowsFromProperties(
          group, groupLayout, sortedProps, filterText,
          [this, effect](const QString &name, const QVariant &value) {
            ScopedPropertyEditGuard guard(localPropertyEditDepth);
            effect->setPropertyValue(name, value);
            scheduleUpdateValues();
          },
          {},
          currentCompositionEffectTime,
          {},
          ArtifactAbstractLayerPtr{},
          &addedGroupProperties, presentation.headingText, &propertyEditors, &effectRows,
          {});

      if (addedGroupProperties) {
        alignPropertyRowLabels(effectRows, 132, 176);
        mainLayout->addWidget(group);
        hasAnyCompositionEffectProperties = true;
      } else {
        delete group;
      }
    }

    if (!hasAnyCompositionEffectProperties) {
      QLabel *emptyProps = new QLabel("No editable composition effect properties");
      emptyProps->setAlignment(Qt::AlignCenter);
      applyPropertySectionLabel(emptyProps, true);
      mainLayout->addWidget(emptyProps);
    }

    mainLayout->addStretch();

    rebuilding = false;
    return;
  }

  bool hasAnyProperties = false;

  const ArtifactAbstractLayerPtr layer = currentLayer;
  auto *playback = ArtifactPlaybackService::instance();
  const auto currentLayerTime = [playback, layer]() {
    return currentPlaybackTime(playback, layer);
  };
  registerCurrentLayerPropertySnapshot(layer, focusedEffectId);

  const auto notifyLayerKeyframeChanged = [this, layer](const QString &) {
    if (layer) {
      notifyLayerPropertyAnimationChanged(layer);
    }
  };

  const auto layerGroups = layer ? layer->getLayerPropertyGroups()
                                 : std::vector<ArtifactCore::PropertyGroup>{};

  const auto decorateLayerRow =
      [this, layer](ArtifactPropertyEditorRowWidget *row,
                    const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
        if (!row || !property) {
          return;
        }
        updateScaleSupplementaryText(row, layer, property,
                                     property->getValue());
        const QString propName = property->getName();
        const bool fav = isFavorite(propName);
        row->setShowFavoriteButton(true);
        row->setShowResetButton(true);
        row->setFavoriteChecked(fav);
        row->setFavoriteHandler([this, propName](bool checked) {
          toggleFavorite(propName);
          if (favoriteOnly) {
            scheduleRebuild(0);
          }
        });
      };
  const auto updateLayerRowValue =
      [layer](ArtifactPropertyEditorRowWidget *row,
              const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
              const QVariant &value) {
        updateScaleSupplementaryText(row, layer, property, value);
      };

  const auto effects = layer->getEffects();
  const bool hasFocusedEffect = !focusedEffectId.trimmed().isEmpty();
  // The dedicated Components tab owns component configuration. Its embedded
  // editor intentionally bypasses the normal layer-property whitelist.
  const auto presentationProfile =
      embeddedComponentEditor
          ? PropertyPresentationProfile{QStringLiteral("components"), {}}
          : propertyPresentationProfile(layer);

  for (const auto &groupDef : layerGroups) {
    const QString groupName =
        groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name();
    if (shouldHideInspectorPropertyGroup(groupName)) {
      continue;
    }
    if (!presentationAllowsGroup(presentationProfile, groupName)) {
      continue;
    }
    const bool isSourceReframe = isSourceReframeSection(groupName);
    const bool isClonerGroup = isClonerSection(groupName);
    auto sortedProps =
        applyFavoriteFilter(filteredGroupProperties(
            layer, groupName, inspectorProperties(groupDef.sortedProperties())),
            favoriteOnly);
    applyPresentationPropertyRules(presentationProfile, groupName, sortedProps);
    if (sortedProps.empty()) {
      continue;
    }

    const bool usesCollapsibleHeader = isExpandedInspectorSection(groupName);
    QGroupBox *group = new QGroupBox(
        usesCollapsibleHeader
            ? QString()
            : (isSourceReframe ? QStringLiteral("Crop / Pan") : groupName));
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);

    if (groupName.compare(QStringLiteral("Components"), Qt::CaseInsensitive) == 0) {
      const auto validationIssues = layer->validateLayerComponents();
      if (!validationIssues.empty()) {
        QStringList issueLines;
        issueLines.reserve(static_cast<int>(std::min<std::size_t>(
            validationIssues.size(), static_cast<std::size_t>(4))));
        for (const auto &issue : validationIssues) {
          if (issueLines.size() >= 4) {
            break;
          }
          const QString componentLabel = issue.componentId.trimmed().isEmpty()
              ? QStringLiteral("(unnamed component)")
              : issue.componentId;
          issueLines.push_back(QStringLiteral("%1: %2")
                                   .arg(componentLabel, issue.message));
        }
        auto *validationNote = new QLabel(group);
        validationNote->setObjectName(QStringLiteral("propertySectionNote"));
        validationNote->setWordWrap(true);
        validationNote->setText(
            QStringLiteral("Validation issues: %1")
                .arg(static_cast<int>(validationIssues.size())));
        if (!issueLines.isEmpty()) {
          validationNote->setToolTip(issueLines.join(QStringLiteral("\n")));
          validationNote->setText(validationNote->text() + QStringLiteral("\n") +
                                  issueLines.join(QStringLiteral("\n")));
        }
        applyPropertySectionLabel(validationNote, false);
        applyThemeTextPalette(validationNote, 110);
        groupLayout->addWidget(validationNote);
      }
    }
    QWidget *sectionBody = group;
    CollapsibleSectionButton *collapseButton = nullptr;
    if (usesCollapsibleHeader) {
      collapseButton = new CollapsibleSectionButton(group);
      collapseButton->setText(groupName);
      collapseButton->setChecked(false);
      sectionBody = new QWidget(group);
      auto *sectionBodyLayout = new QVBoxLayout(sectionBody);
      sectionBodyLayout->setContentsMargins(0, 0, 0, 0);
      sectionBodyLayout->setSpacing(5);
      groupLayout->addWidget(collapseButton);
      groupLayout->addWidget(sectionBody);
      collapseButton->setTarget(sectionBody);
    }
    QVBoxLayout *contentLayout = groupLayout;
    if (sectionBody != group) {
      contentLayout = static_cast<QVBoxLayout *>(sectionBody->layout());
    }
    bool addedGroupProperties = false;
    std::vector<ArtifactPropertyEditorRowWidget *> groupRows;
    auto groupPreviewOpacity = std::make_shared<std::optional<float>>();
    auto commitLayerValue = [this, layer, groupPreviewOpacity](
                                 const QString &name, const QVariant &value) {
      if (!layer) { return; }
      ScopedPropertyEditGuard guard(localPropertyEditDepth);
      if (name.compare(QStringLiteral("layer.opacity"),
                       Qt::CaseInsensitive) == 0) {
        const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
        for (const auto &tl : this->targetLayers) {
          if (!tl) { continue; }
          const float oldOpacity = groupPreviewOpacity->value_or(tl->opacity());
          if (std::abs(oldOpacity - newOpacity) > 0.0001f) {
            auto *cmd = new ChangeLayerOpacityCommand(tl, oldOpacity, newOpacity);
            UndoManager::instance()->push(
                std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
          }
        }
        groupPreviewOpacity->reset();
      } else if (name.compare(QStringLiteral("source.localized"),
                              Qt::CaseInsensitive) == 0) {
        if (auto* service = ArtifactProjectService::instance()) {
          for (const auto& tl : this->targetLayers) {
            if (!tl) continue;
            if (value.toBool()) {
              service->localizeLayerSourceInCurrentComposition(tl->id());
            } else {
              service->relinkSharedLayerSourceInCurrentComposition(tl->id());
            }
          }
        }
        scheduleRebuild(0);
      } else {
        for (const auto &tl : this->targetLayers) {
          if (!tl) { continue; }
          tl->setLayerPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(tl);
        }
        if (name.startsWith(QStringLiteral("component.cloner."), Qt::CaseInsensitive) ||
            name.compare(QStringLiteral("component.layout.enabled"), Qt::CaseInsensitive) == 0) {
          scheduleRebuild(0);
        }
      }
    };
    auto previewLayerValue = [this, layer, groupPreviewOpacity](
                                  const QString &name, const QVariant &value) {
      if (!layer) { return; }
      ScopedPropertyEditGuard guard(localPropertyEditDepth);
      if (name.compare(QStringLiteral("layer.opacity"),
                       Qt::CaseInsensitive) == 0) {
        const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
        if (!groupPreviewOpacity->has_value()) {
          *groupPreviewOpacity = layer->opacity();
        }
        for (const auto &tl : this->targetLayers) {
          if (!tl) { continue; }
          tl->setOpacity(newOpacity);
        }
      } else if (name.compare(QStringLiteral("source.localized"),
                              Qt::CaseInsensitive) == 0) {
        // Identity changes are commit-only because they create an Undo command.
        return;
      } else {
        for (const auto &tl : targetLayers) {
          if (!tl) { continue; }
          tl->setLayerPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(tl);
        }
        if (name.startsWith(QStringLiteral("component.cloner."), Qt::CaseInsensitive) ||
            name.compare(QStringLiteral("component.layout.enabled"), Qt::CaseInsensitive) == 0) {
          scheduleRebuild(0);
        }
      }
    };

    if (isClonerGroup) {
      auto *headerRow = new QWidget(group);
      auto *headerLayout = new QHBoxLayout(headerRow);
      headerLayout->setContentsMargins(0, 0, 0, 0);
      headerLayout->setSpacing(8);
      auto *note = new QLabel(
          QStringLiteral("Stack any number of clone transforms and reorder them in evaluation order."),
          group);
      note->setObjectName(QStringLiteral("propertySectionNote"));
      note->setWordWrap(true);
      applyPropertySectionLabel(note, false);
      applyThemeTextPalette(note, 120);
      headerLayout->addWidget(note, 1);
      auto *addTransformButton = new QPushButton(QStringLiteral("Add Transform"), group);
      addTransformButton->setCursor(Qt::PointingHandCursor);
      headerLayout->addWidget(addTransformButton, 0);
      contentLayout->addWidget(headerRow);
      addedGroupProperties = true;
      QObject::connect(addTransformButton, &QPushButton::clicked, group, [this, layer]() {
        if (!layer) {
          return;
        }
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        layer->setLayerPropertyValue(QStringLiteral("component.cloner.transforms.add"), true);
        notifyLayerPropertyAnimationChanged(layer);
        pendingScrollGroupName = QStringLiteral("Cloner");
        scheduleRebuild(0);
      });

      std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> baseProps;
      std::map<int, std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>> transformProps;
      for (const auto &prop : sortedProps) {
        if (!prop) {
          continue;
        }
        const QString name = prop->getName();
        const QString prefix = QStringLiteral("component.cloner.transforms.");
        if (!name.startsWith(prefix, Qt::CaseInsensitive)) {
          baseProps.push_back(prop);
          continue;
        }
        const QString tail = name.mid(prefix.size());
        const QStringList parts = tail.split(QLatin1Char('.'), Qt::SkipEmptyParts);
        if (parts.size() != 2) {
          baseProps.push_back(prop);
          continue;
        }
        bool ok = false;
        const int index = parts[0].toInt(&ok);
        if (!ok || index < 0) {
          baseProps.push_back(prop);
          continue;
        }
        transformProps[index].push_back(prop);
      }

      if (!baseProps.empty()) {
        addRowsFromProperties(
            group, contentLayout, baseProps, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, groupName, &propertyEditors, &groupRows,
            decorateLayerRow, updateLayerRowValue);
      }

      for (auto &[transformIndex, props] : transformProps) {
        if (props.empty()) {
          continue;
        }
        auto findProp = [&](const QString &suffix) {
          const QString target = QStringLiteral("component.cloner.transforms.%1.%2")
                                     .arg(transformIndex)
                                     .arg(suffix);
          auto it = std::find_if(props.begin(), props.end(),
                                 [&target](const auto &candidate) {
                                   return candidate &&
                                       candidate->getName().compare(target, Qt::CaseInsensitive) == 0;
                                 });
          return it != props.end() ? *it : std::shared_ptr<ArtifactCore::AbstractProperty>{};
        };
        const auto nameProp = findProp(QStringLiteral("name"));
        const QString transformTitle =
            nameProp && !nameProp->getValue().toString().trimmed().isEmpty()
                ? nameProp->getValue().toString().trimmed()
                : QStringLiteral("Transform %1").arg(transformIndex + 1);

        auto *transformBox = new QGroupBox(group);
        auto *transformLayout = new QVBoxLayout(transformBox);
        transformLayout->setContentsMargins(8, 6, 8, 6);
        transformLayout->setSpacing(5);
        applyPropertySectionBox(transformBox);
        applyThemeTextPalette(transformBox, 118);

        auto *collapseButton = new CollapsibleSectionButton(transformBox);
        collapseButton->setText(transformTitle);
        collapseButton->setChecked(true);
        transformLayout->addWidget(collapseButton);

        auto *actionRow = new QWidget(transformBox);
        auto *actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(20, 0, 0, 0);
        actionLayout->setSpacing(6);
        auto *moveUpButton = new QPushButton(QStringLiteral("Up"), actionRow);
        auto *moveDownButton = new QPushButton(QStringLiteral("Down"), actionRow);
        auto *duplicateButton = new QPushButton(QStringLiteral("Duplicate"), actionRow);
        auto *removeButton = new QPushButton(QStringLiteral("Remove"), actionRow);
        moveUpButton->setEnabled(transformIndex > 0);
        moveDownButton->setEnabled(transformIndex + 1 < static_cast<int>(transformProps.size()));
        for (auto *button : {moveUpButton, moveDownButton, duplicateButton, removeButton}) {
          button->setCursor(Qt::PointingHandCursor);
          actionLayout->addWidget(button);
        }
        actionLayout->addStretch();
        transformLayout->addWidget(actionRow);

        auto *sectionBody = new QWidget(transformBox);
        auto *sectionBodyLayout = new QVBoxLayout(sectionBody);
        sectionBodyLayout->setContentsMargins(0, 0, 0, 0);
        sectionBodyLayout->setSpacing(5);
        collapseButton->setTarget(sectionBody);
        transformLayout->addWidget(sectionBody);

        QObject::connect(moveUpButton, &QPushButton::clicked, transformBox,
                         [this, layer, transformIndex]() {
                           if (!layer) { return; }
                           ScopedPropertyEditGuard guard(localPropertyEditDepth);
                           layer->setLayerPropertyValue(
                               QStringLiteral("component.cloner.transforms.moveUp"),
                               transformIndex);
                           notifyLayerPropertyAnimationChanged(layer);
                           pendingScrollGroupName = QStringLiteral("Cloner");
                           scheduleRebuild(0);
                         });
        QObject::connect(moveDownButton, &QPushButton::clicked, transformBox,
                         [this, layer, transformIndex]() {
                           if (!layer) { return; }
                           ScopedPropertyEditGuard guard(localPropertyEditDepth);
                           layer->setLayerPropertyValue(
                               QStringLiteral("component.cloner.transforms.moveDown"),
                               transformIndex);
                           notifyLayerPropertyAnimationChanged(layer);
                           pendingScrollGroupName = QStringLiteral("Cloner");
                           scheduleRebuild(0);
                         });
        QObject::connect(duplicateButton, &QPushButton::clicked, transformBox,
                         [this, layer, transformIndex]() {
                           if (!layer) { return; }
                           ScopedPropertyEditGuard guard(localPropertyEditDepth);
                           layer->setLayerPropertyValue(
                               QStringLiteral("component.cloner.transforms.duplicate"),
                               transformIndex);
                           notifyLayerPropertyAnimationChanged(layer);
                           pendingScrollGroupName = QStringLiteral("Cloner");
                           scheduleRebuild(0);
                         });
        QObject::connect(removeButton, &QPushButton::clicked, transformBox,
                         [this, layer, transformIndex]() {
                           if (!layer) { return; }
                           ScopedPropertyEditGuard guard(localPropertyEditDepth);
                           layer->setLayerPropertyValue(
                               QStringLiteral("component.cloner.transforms.remove"),
                               transformIndex);
                           notifyLayerPropertyAnimationChanged(layer);
                           pendingScrollGroupName = QStringLiteral("Cloner");
                           scheduleRebuild(0);
                         });

        std::vector<ArtifactPropertyEditorRowWidget *> transformRows;
        addRowsFromProperties(
            transformBox, sectionBodyLayout, props, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, transformTitle, &propertyEditors, &transformRows,
            decorateLayerRow, updateLayerRowValue);
        if (!transformRows.empty()) {
          alignPropertyRowLabels(transformRows, 132, 184);
        }
        contentLayout->addWidget(transformBox);
      }
    } else if (isSourceReframe) {
      bool sourceCropEnabled = false;
      for (const auto &prop : sortedProps) {
        if (prop &&
            prop->getName().compare(QStringLiteral("sourceCrop.enabled"),
                                    Qt::CaseInsensitive) == 0) {
          sourceCropEnabled = prop->getValue().toBool();
          break;
        }
      }

      if (!sourceCropEnabled) {
        auto *enableButton = new QPushButton(QStringLiteral("Show Crop / Pan"),
                                             group);
        enableButton->setCursor(Qt::PointingHandCursor);
        contentLayout->addWidget(enableButton);
        QObject::connect(enableButton, &QPushButton::clicked, group,
                         [this, layer]() {
                           if (!layer) {
                             return;
                           }
                           ScopedPropertyEditGuard guard(localPropertyEditDepth);
                           const auto sourceSize = layer->sourceSize();
                           const auto cropWidth = layer->getProperty(
                               QStringLiteral("sourceCrop.cropWidth"));
                           const auto cropHeight = layer->getProperty(
                               QStringLiteral("sourceCrop.cropHeight"));
                           const bool needsInitialCrop =
                               sourceSize.width > 0 && sourceSize.height > 0 &&
                               (!cropWidth || !cropHeight ||
                                cropWidth->getValue().toDouble() <= 0.0 ||
                                cropHeight->getValue().toDouble() <= 0.0);
                           layer->setLayerPropertyValue(
                               QStringLiteral("sourceCrop.enabled"), true);
                           if (needsInitialCrop) {
                             layer->setLayerPropertyValue(
                                 QStringLiteral("sourceCrop.cropX"), 0.0);
                             layer->setLayerPropertyValue(
                                 QStringLiteral("sourceCrop.cropY"), 0.0);
                             layer->setLayerPropertyValue(
                                 QStringLiteral("sourceCrop.cropWidth"), sourceSize.width);
                             layer->setLayerPropertyValue(
                                 QStringLiteral("sourceCrop.cropHeight"), sourceSize.height);
                           }
                           notifyLayerPropertyAnimationChanged(layer);
                           rebuildSignature.clear();
                           invalidatePropertyValueCache();
                           pendingScrollGroupName = QStringLiteral("Crop / Pan");
                           scheduleRebuild(0);
                         });

        auto *note = new QLabel(
            QStringLiteral("Enable Crop / Pan to reveal source window and motion controls."),
            group);
        note->setObjectName(QStringLiteral("propertySectionNote"));
        note->setWordWrap(true);
        applyPropertySectionLabel(note, false);
        applyThemeTextPalette(note, 120);
        contentLayout->addWidget(note);

        addedGroupProperties = true;
      } else {
      auto *headerRow = new QWidget(group);
      auto *headerLayout = new QHBoxLayout(headerRow);
      headerLayout->setContentsMargins(0, 0, 0, 0);
      headerLayout->setSpacing(8);
      auto *note = new QLabel(
          QStringLiteral("Crop / Pan views the source, and Layer Transform places it in comp."));
      note->setObjectName(QStringLiteral("propertySectionNote"));
      note->setWordWrap(true);
      applyPropertySectionLabel(note, false);
      applyThemeTextPalette(note, 120);
      headerLayout->addWidget(note, 1);

      auto *resetButton = new QPushButton(QStringLiteral("Reset Crop / Pan"), group);
      resetButton->setCursor(Qt::PointingHandCursor);
      resetButton->setToolTip(QStringLiteral("Restore crop, pan, zoom, rotation, and anchor to defaults."));
      headerLayout->addWidget(resetButton, 0);

      auto *disableButton = new QPushButton(QStringLiteral("Disable Crop / Pan"), group);
      disableButton->setCursor(Qt::PointingHandCursor);
      disableButton->setToolTip(QStringLiteral("Turn off Crop / Pan without changing the current values."));
      headerLayout->addWidget(disableButton, 0);

      contentLayout->addWidget(headerRow);

      QObject::connect(resetButton, &QPushButton::clicked, group, [this, layer]() {
        if (!layer) {
          return;
        }
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        const auto sourceSize = layer->sourceSize();
        const bool hasSourceSize = sourceSize.width > 0 && sourceSize.height > 0;
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), true);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.panX"), 0.0);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.panY"), 0.0);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.zoom"), 1.0);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.rotation"), 0.0);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.anchorX"), 0.5);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.anchorY"), 0.5);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.preserveAspect"), true);
        if (hasSourceSize) {
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"), 0.0);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"), 0.0);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"), sourceSize.width);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"), sourceSize.height);
        }
        notifyLayerPropertyAnimationChanged(layer);
        invalidatePropertyValueCache();
      });

      QObject::connect(disableButton, &QPushButton::clicked, group, [this, layer]() {
        if (!layer) {
          return;
        }
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), false);
        notifyLayerPropertyAnimationChanged(layer);
        rebuildSignature.clear();
        invalidatePropertyValueCache();
        scheduleRebuild(0);
      });

      auto collectProps = [](const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &src,
                             const auto &names) {
        std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> out;
        for (const auto &name : names) {
          const QString target = QString::fromUtf8(name);
          auto it = std::find_if(src.begin(), src.end(),
                                 [&target](const std::shared_ptr<ArtifactCore::AbstractProperty> &prop) {
                                   return prop && prop->getName().compare(target, Qt::CaseInsensitive) == 0;
                                 });
          if (it != src.end()) {
            out.push_back(*it);
          }
        }
        return out;
      };

      const auto windowProps = collectProps(
          sortedProps,
          std::array<const char *, 8>{
              "sourceCrop.enabled", "sourceCrop.cropX", "sourceCrop.cropY",
              "sourceCrop.cropWidth", "sourceCrop.cropHeight",
              "sourceCrop.anchorX", "sourceCrop.anchorY",
              "sourceCrop.preserveAspect"});
      const auto motionProps = collectProps(
          sortedProps,
          std::array<const char *, 4>{
              "sourceCrop.panX", "sourceCrop.panY",
              "sourceCrop.zoom", "sourceCrop.rotation"});

      if (!windowProps.empty()) {
        auto *windowLabel = new QLabel(QStringLiteral("Crop"), group);
        windowLabel->setObjectName(QStringLiteral("propertySectionLabel"));
        applyPropertySectionLabel(windowLabel, true);
        contentLayout->addWidget(windowLabel);

        std::vector<ArtifactPropertyEditorRowWidget *> windowRows;
        addRowsFromProperties(
            group, contentLayout, windowProps, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, groupName, &propertyEditors, &windowRows,
            decorateLayerRow, updateLayerRowValue);
        groupRows.insert(groupRows.end(), windowRows.begin(), windowRows.end());
      }

      if (!motionProps.empty()) {
        auto *motionLabel = new QLabel(QStringLiteral("Pan"), group);
        motionLabel->setObjectName(QStringLiteral("propertySectionLabel"));
        applyPropertySectionLabel(motionLabel, true);
        contentLayout->addWidget(motionLabel);

        std::vector<ArtifactPropertyEditorRowWidget *> motionRows;
        addRowsFromProperties(
            group, contentLayout, motionProps, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, groupName, &propertyEditors, &motionRows,
            decorateLayerRow, updateLayerRowValue);
        groupRows.insert(groupRows.end(), motionRows.begin(), motionRows.end());
      }
      }
    } else {
      addRowsFromProperties(
          group, contentLayout, sortedProps, filterText, commitLayerValue,
          previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
          layer, &addedGroupProperties, groupName, &propertyEditors, &groupRows,
          decorateLayerRow, updateLayerRowValue);
    }

    const bool hasSourceCrop =
        layer && layer->getProperty(QStringLiteral("sourceCrop.enabled")) &&
        layer->getProperty(QStringLiteral("sourceCrop.cropX")) &&
        layer->getProperty(QStringLiteral("sourceCrop.panX"));
    if (!embeddedComponentEditor &&
        groupName.compare(QStringLiteral("Transform"), Qt::CaseInsensitive) == 0 &&
        hasSourceCrop) {
      auto *buttonRow = new QWidget(group);
      auto *buttonLayout = new QHBoxLayout(buttonRow);
      buttonLayout->setContentsMargins(20, 0, 4, 0);
      buttonLayout->setSpacing(0);
      auto *enableButton = new QPushButton(
          layer->getProperty(QStringLiteral("sourceCrop.enabled")) &&
                  layer->getProperty(QStringLiteral("sourceCrop.enabled"))->getValue().toBool()
              ? QStringLiteral("Edit Crop / Pan")
              : QStringLiteral("Add Crop / Pan"),
          buttonRow);
      enableButton->setCursor(Qt::PointingHandCursor);
      buttonLayout->addWidget(enableButton);
      buttonLayout->addStretch();
      contentLayout->addWidget(buttonRow);
      QObject::connect(enableButton, &QPushButton::clicked, group, [this, layer]() {
        if (!layer) {
          return;
        }
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        const auto enabledProperty =
            layer->getProperty(QStringLiteral("sourceCrop.enabled"));
        const bool wasEnabled =
            enabledProperty && enabledProperty->getValue().toBool();
        const auto sourceSize = layer->sourceSize();
        const auto cropWidth =
            layer->getProperty(QStringLiteral("sourceCrop.cropWidth"));
        const auto cropHeight =
            layer->getProperty(QStringLiteral("sourceCrop.cropHeight"));
        const bool needsInitialCrop =
            !wasEnabled && sourceSize.width > 0 && sourceSize.height > 0 &&
            (!cropWidth || !cropHeight ||
             cropWidth->getValue().toDouble() <= 0.0 ||
             cropHeight->getValue().toDouble() <= 0.0);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), true);
        if (needsInitialCrop) {
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropX"), 0.0);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropY"), 0.0);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropWidth"),
                                       sourceSize.width);
          layer->setLayerPropertyValue(QStringLiteral("sourceCrop.cropHeight"),
                                       sourceSize.height);
        }
        notifyLayerPropertyAnimationChanged(layer);
        pendingScrollGroupName = QStringLiteral("Crop / Pan");
        if (!wasEnabled) {
          rebuildSignature.clear();
        }
        invalidatePropertyValueCache();
        scheduleRebuild(0);
      });
      addedGroupProperties = true;
    }

    if (addedGroupProperties) {
      alignPropertyRowLabels(groupRows, 132, 184);
      mainLayout->addWidget(group);
      hasAnyProperties = true;
    } else {
      delete group;
    }
  }

  if (hasFocusedEffect) {
    auto *focusedLabel = new QLabel(
        QStringLiteral("Focused Effect ID: %1").arg(focusedEffectId));
    focusedLabel->setObjectName(QStringLiteral("propertySectionLabel"));
    applyPropertySectionLabel(focusedLabel, true);
    QFont focusedFont = focusedLabel->font();
    focusedFont.setPointSize(11);
    focusedLabel->setFont(focusedFont);
    applyThemeTextPalette(focusedLabel, 110);
    mainLayout->addWidget(focusedLabel);
  }

  for (const auto &effect : effects) {
    if (!effect)
      continue;
    if (hasFocusedEffect && effect->effectID().toQString() != focusedEffectId) {
      continue;
    }

    const auto presentation = describeEffectPresentation(effect);
    QGroupBox *group = new QGroupBox(presentation.headingText);
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);

    auto *stageLabel = new QLabel(presentation.stageNoteText, group);
    stageLabel->setObjectName(QStringLiteral("propertySectionNote"));
    stageLabel->setWordWrap(true);
    applyPresentationToneLabel(stageLabel, presentation.badgeTone, false);
    groupLayout->addWidget(stageLabel);

    ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
    for (const auto &p : effect->getProperties()) {
      propGroup.addProperty(
          std::make_shared<ArtifactCore::AbstractProperty>(p));
    }

    auto sortedProps = propGroup.sortedProperties();
    // Apply favorites filter for effects too when in favorite-only mode
    if (favoriteOnly) {
      const auto favs = loadFavoriteProperties();
      if (!favs.isEmpty()) {
        std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> favFiltered;
        favFiltered.reserve(sortedProps.size());
        for (const auto &p : sortedProps) {
          if (p && favs.contains(p->getName(), Qt::CaseInsensitive)) {
            favFiltered.push_back(p);
          }
        }
        sortedProps.swap(favFiltered);
      } else {
        sortedProps.clear();
      }
    }
    if (sortedProps.empty()) {
      delete group;
      continue;
    }
    bool addedGroupProperties = false;
    std::vector<ArtifactPropertyEditorRowWidget *> effectRows;
    addRowsFromProperties(
        group, groupLayout, sortedProps, filterText,
        [this, layer, effect](const QString &name, const QVariant &value) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          effect->setPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(layer);
        },
        {},
        currentLayerTime,
        {},
        layer,
        &addedGroupProperties, presentation.headingText, &propertyEditors, &effectRows,
        decorateLayerRow);

    if (addedGroupProperties) {
      alignPropertyRowLabels(effectRows, 132, 176);
      mainLayout->addWidget(group);
      hasAnyProperties = true;
    } else {
      delete group;
    }
  }

  if (!hasAnyProperties) {
    QLabel *emptyProps = new QLabel("No editable properties");
    emptyProps->setAlignment(Qt::AlignCenter);
    applyPropertySectionLabel(emptyProps, true);
    mainLayout->addWidget(emptyProps);
  }

  mainLayout->addStretch();
  applyLockState();

  // 再利用されなかったウィジェットを削除
  if (!pendingScrollGroupName.isEmpty()) {
    QTimer::singleShot(0, owner, [this]() { scrollToGroupByName(pendingScrollGroupName); });
    pendingScrollGroupName.clear();
  }

  rebuilding = false;
}

void ArtifactPropertyWidget::Impl::scrollToGroupByName(const QString &groupName) {
  if (groupName.isEmpty() || !owner) {
    return;
  }
  const auto groups = owner->findChildren<QGroupBox *>();
  for (auto *group : groups) {
    if (!group) {
      continue;
    }
    if (group->title().compare(groupName, Qt::CaseInsensitive) == 0) {
      owner->ensureWidgetVisible(group, 16, 16);
      return;
    }
  }
}

} // namespace Artifact
