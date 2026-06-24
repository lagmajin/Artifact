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
#include <QMenu>
#include <QMetaObject>
#include <QMouseEvent>
#include <QDir>
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

namespace Artifact {

namespace {
constexpr int kPropertyLabelMinWidth = 132;
constexpr int kPropertyLabelMaxWidth = 184;

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

QColor themeColor(const QString &value, const QColor &fallback) {
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

QColor blendColor(const QColor &a, const QColor &b, const qreal t) {
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                          a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                          a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                          a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

ArtifactTimelineWidget *activeTimelineWidget(QWidget *root) {
  if (!root) {
    return nullptr;
  }
  const auto widgets = root->window()->findChildren<ArtifactTimelineWidget *>();
  for (auto *widget : widgets) {
    if (widget && widget->hasFocus()) {
      return widget;
    }
  }
  for (auto *widget : widgets) {
    if (widget && widget->isVisible()) {
      return widget;
    }
  }
  return widgets.isEmpty() ? nullptr : widgets.front();
}

std::shared_ptr<ArtifactCore::AbstractProperty>
findPropertyByName(const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
                   const QString &name) {
  auto it = std::find_if(
      properties.begin(), properties.end(),
      [&name](const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
        return property &&
               property->getName().compare(name, Qt::CaseInsensitive) == 0;
      });
  return it != properties.end() ? *it : nullptr;
}

bool boolPropertyValue(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
    const QString &name, const bool fallback = false) {
  if (const auto property = findPropertyByName(properties, name)) {
    return property->getValue().toBool();
  }
  return fallback;
}

int intPropertyValue(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties,
    const QString &name, const int fallback = 0) {
  if (const auto property = findPropertyByName(properties, name)) {
    return property->getValue().toInt();
  }
  return fallback;
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filteredGroupProperties(
    const ArtifactAbstractLayerPtr &layer, const QString &groupName,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> &properties) {
  const QString normalizedGroup = groupName.trimmed();
  const auto layerBool = [&layer](const QString &name, const bool fallback = false) {
    if (!layer) {
      return fallback;
    }
    const auto property = layer->getProperty(name);
    return property ? property->getValue().toBool() : fallback;
  };
  const auto layerInt = [&layer](const QString &name, const int fallback = 0) {
    if (!layer) {
      return fallback;
    }
    const auto property = layer->getProperty(name);
    return property ? property->getValue().toInt() : fallback;
  };
  const auto groupBool = [&properties, &layerBool](const QString &name,
                                                   const bool fallback = false) {
    if (const auto property = findPropertyByName(properties, name)) {
      return property->getValue().toBool();
    }
    return layerBool(name, fallback);
  };
  const auto groupInt = [&properties, &layerInt](const QString &name,
                                                 const int fallback = 0) {
    if (const auto property = findPropertyByName(properties, name)) {
      return property->getValue().toInt();
    }
    return layerInt(name, fallback);
  };
  if (normalizedGroup.compare(QStringLiteral("Cloner"), Qt::CaseInsensitive) == 0) {
    const int mode = groupInt(QStringLiteral("component.cloner.mode"), 0);
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
    filtered.reserve(properties.size());
    for (const auto &property : properties) {
      if (!property) {
        continue;
      }
      const QString name = property->getName();
      const bool isLinearOnly =
          name == QStringLiteral("component.cloner.cloneCount") ||
          name == QStringLiteral("component.cloner.offsetX") ||
          name == QStringLiteral("component.cloner.offsetY") ||
          name == QStringLiteral("component.cloner.offsetZ");
      const bool isGridOnly =
          name == QStringLiteral("component.cloner.columns") ||
          name == QStringLiteral("component.cloner.rows") ||
          name == QStringLiteral("component.cloner.depth") ||
          name == QStringLiteral("component.cloner.spacingX") ||
          name == QStringLiteral("component.cloner.spacingY") ||
          name == QStringLiteral("component.cloner.spacingZ");
      const bool isRadialOnly =
          name == QStringLiteral("component.cloner.radialCount") ||
          name == QStringLiteral("component.cloner.radius") ||
          name == QStringLiteral("component.cloner.startAngle") ||
          name == QStringLiteral("component.cloner.endAngle");

      if ((mode == 0 && (isGridOnly || isRadialOnly)) ||
          (mode == 1 && (isLinearOnly || isRadialOnly)) ||
          (mode == 2 && (isLinearOnly || isGridOnly))) {
        continue;
      }
      filtered.push_back(property);
    }
    return filtered;
  }

  if (normalizedGroup.compare(QStringLiteral("Solid"), Qt::CaseInsensitive) == 0) {
    const int fillType = groupInt(QStringLiteral("solid.fillType"),
                                 static_cast<int>(ArtifactSolidFillType::Solid));
    if (fillType != static_cast<int>(ArtifactSolidFillType::LinearGradient)) {
      std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
      filtered.reserve(properties.size());
      for (const auto &property : properties) {
        if (!property) {
          continue;
        }
        const QString name = property->getName();
        const bool isGradientOnly =
            name == QStringLiteral("solid.gradientStartColor") ||
            name == QStringLiteral("solid.gradientEndColor") ||
            name == QStringLiteral("solid.gradientAngleDegrees") ||
            name == QStringLiteral("solid.gradientReverse") ||
            name == QStringLiteral("solid.gradientCenterX") ||
            name == QStringLiteral("solid.gradientCenterY") ||
            name == QStringLiteral("solid.gradientScale") ||
            name == QStringLiteral("solid.gradientOffset");
        if (isGradientOnly) {
          continue;
        }
        filtered.push_back(property);
      }
      return filtered;
    }
  }

  return properties;
}

int64_t playbackFrameRateValue(ArtifactPlaybackService *playback) {
  const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
  return std::max<int64_t>(
      1, static_cast<int64_t>(std::llround(frameRate.framerate())));
}

int64_t compositionFrameRateValue(
    const ArtifactAbstractComposition *composition) {
  if (!composition) {
    return 30;
  }
  return std::max<int64_t>(
      1, static_cast<int64_t>(
             std::llround(composition->frameRate().framerate())));
}

RationalTime currentPlaybackTime(ArtifactPlaybackService *playback) {
  const auto frame = playback ? playback->currentFrame() : FramePosition(0);
  return RationalTime(frame.framePosition(), playbackFrameRateValue(playback));
}

RationalTime currentPlaybackTime(ArtifactPlaybackService *playback,
                                 const ArtifactAbstractLayerPtr &layer) {
  if (layer) {
    if (auto *composition = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
      return RationalTime(composition->framePosition().framePosition(),
                          compositionFrameRateValue(composition));
    }
  }
  return currentPlaybackTime(playback);
}

void applyThemeTextPalette(QWidget *widget, int shade = 100) {
  if (!widget) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  QPalette pal = widget->palette();
  const QColor textColor(
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC"))));
  pal.setColor(QPalette::WindowText, textColor.darker(shade));
  pal.setColor(QPalette::Text, textColor.darker(shade));
  widget->setPalette(pal);
}

void applyPropertyPanelPalette(QWidget *widget, const bool elevated = false) {
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

  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setAutoFillBackground(true);
  QPalette pal = widget->palette();
  const QColor window =
      blendColor(background, surface, elevated ? 0.72 : 0.58);
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, surface);
  pal.setColor(QPalette::AlternateBase, blendColor(window, surface, 0.16));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, window);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Light, accent.lighter(120));
  widget->setPalette(pal);
}

void registerCurrentLayerPropertySnapshot(const ArtifactAbstractLayerPtr& layer,
                                          const QString& focusedEffectId)
{
  auto& registry = ArtifactCore::globalPropertyRegistry();
  registry.clear();

  if (!layer) {
    return;
  }

  const QString layerOwnerPath = QStringLiteral("layer:%1").arg(layer->id().toString());
  const auto layerGroups = layer->getLayerPropertyGroups();
  for (const auto& groupDef : layerGroups) {
    const QString groupName = groupDef.name().trimmed().isEmpty()
        ? QStringLiteral("Layer")
        : groupDef.name().trimmed();
    const QString ownerPath = ArtifactCore::propertyPathJoin(layerOwnerPath, groupName);
    registry.registerOwnerSnapshot(ownerPath,
                                   groupName,
                                   QStringLiteral("LayerGroup"),
                                   groupDef,
                                   true);
  }

  const auto effects = layer->getEffects();
  for (const auto& effect : effects) {
    if (!effect) {
      continue;
    }
    if (!focusedEffectId.trimmed().isEmpty() &&
        effect->effectID().toQString() != focusedEffectId.trimmed()) {
      continue;
    }

    ArtifactCore::PropertyGroup effectGroup(effect->displayName().toQString());
    for (const auto& property : effect->getProperties()) {
      effectGroup.addProperty(std::make_shared<ArtifactCore::AbstractProperty>(property));
    }

    const QString ownerPath = ArtifactCore::propertyPathJoin(
        layerOwnerPath,
        QStringLiteral("effect:%1").arg(effect->effectID().toQString()));
    registry.registerOwnerSnapshot(ownerPath,
                                   effect->displayName().toQString(),
                                   QStringLiteral("EffectGroup"),
                                   effectGroup,
                                   true);
  }
}

void applyPropertySearchPalette(QLineEdit *edit) {
  applyPropertyPanelPalette(edit, true);
  if (!edit) {
    return;
  }
  QFont font = edit->font();
  font.setPointSize(font.pointSize() > 0 ? std::max(font.pointSize(), 10) : 10);
  edit->setFont(font);
}

void applyPropertySectionLabel(QLabel *label, const bool prominent = false) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? accent : text);
  label->setPalette(pal);
}

void applyPresentationToneLabel(QLabel *label,
                                LayerPresentationBadgeTone tone,
                                const bool prominent = false) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  QColor toneColor = text;
  switch (tone) {
  case LayerPresentationBadgeTone::Container:
    toneColor = accent;
    break;
  case LayerPresentationBadgeTone::Media:
    toneColor = blendColor(text, accent, 0.22);
    break;
  case LayerPresentationBadgeTone::Motion:
    toneColor = accent.lighter(115);
    break;
  case LayerPresentationBadgeTone::Special:
    toneColor = accent.darker(110);
    break;
  case LayerPresentationBadgeTone::Neutral:
  default:
    toneColor = text;
    break;
  }
  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? toneColor : toneColor.darker(110));
  label->setPalette(pal);
}

void applyPropertySectionBox(QGroupBox *box) {
  if (!box) {
    return;
  }
  applyPropertyPanelPalette(box, true);
  QFont font = box->font();
  font.setPointSize(10);
  font.setWeight(QFont::DemiBold);
  box->setFont(font);
  box->setFlat(false);
}

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
    descriptor.stageText = QStringLiteral("Effect");
    descriptor.stageNoteText = QStringLiteral("Stage: Unknown");
    descriptor.badgeTone = LayerPresentationBadgeTone::Neutral;
    break;
  }

  descriptor.headingText =
      QStringLiteral("%1 · %2")
          .arg(descriptor.stageText,
               effect->displayName().toQString());
  return descriptor;
}

void clearLayoutRecursive(QLayout *layout) {
  if (!layout) {
    return;
  }
  while (QLayoutItem *item = layout->takeAt(0)) {
    if (QLayout *childLayout = item->layout()) {
      clearLayoutRecursive(childLayout);
    }
    if (QWidget *widget = item->widget()) {
      widget->hide();
      widget->deleteLater();
    }
    delete item;
  }
}
} // namespace

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
  QHash<QString, ArtifactPropertyEditorRowWidget *> propertyEditors;
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
    updateValuesTimer->start(updateValuesDebounceMs);
  }

  QString computeRebuildSignature() const;
  void invalidatePropertyValueCache();
  void rebuildUI();
  void scrollToGroupByName(const QString &groupName);
  void updatePropertyValues();
  void applyLockState();
  ArtifactPropertyEditorRowWidget* activeExpressionRow() const;
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

namespace {

class ScopedPropertyEditGuard final {
public:
  explicit ScopedPropertyEditGuard(int &depth) : depth_(depth) { ++depth_; }
  ~ScopedPropertyEditGuard() { --depth_; }

private:
  int &depth_;
};

QString humanizePropertyLabel(QString name) {
  static const QHash<QString, QString> explicitLabels = {
      {QStringLiteral("transform.position.x"), QStringLiteral("Position X")},
      {QStringLiteral("transform.position.y"), QStringLiteral("Position Y")},
      {QStringLiteral("transform.scale.x"), QStringLiteral("Scale X")},
      {QStringLiteral("transform.scale.y"), QStringLiteral("Scale Y")},
      {QStringLiteral("transform.rotation"), QStringLiteral("Rotation")},
      {QStringLiteral("transform.anchor.x"), QStringLiteral("Anchor X")},
      {QStringLiteral("transform.anchor.y"), QStringLiteral("Anchor Y")},
      {QStringLiteral("layer.opacity"), QStringLiteral("Opacity")},
      {QStringLiteral("time.inPoint"), QStringLiteral("In Point")},
      {QStringLiteral("time.outPoint"), QStringLiteral("Out Point")},
      {QStringLiteral("time.startTime"), QStringLiteral("Start Time")}};
  if (const auto it = explicitLabels.constFind(name);
      it != explicitLabels.constEnd()) {
    return it.value();
  }

  const auto parts = name.split(QLatin1Char('.'), Qt::SkipEmptyParts);
  if (!parts.isEmpty()) {
    const bool isMaskPath =
        parts.front().compare(QStringLiteral("mask"), Qt::CaseInsensitive) == 0;
    const bool isRotoPath =
        parts.front().compare(QStringLiteral("roto"), Qt::CaseInsensitive) == 0;
    if (isMaskPath || isRotoPath) {
      const QString entryLabel =
          isRotoPath ? QStringLiteral("Roto") : QStringLiteral("Mask");
      if (parts.size() == 3 &&
          parts[2].compare(QStringLiteral("enabled"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("%1 %2 / Enabled")
            .arg(entryLabel)
            .arg(parts[1].toInt() + 1);
      }
      if (parts.size() == 5 &&
          parts[2].compare(QStringLiteral("path"), Qt::CaseInsensitive) == 0) {
        const QString pathLabel = QStringLiteral("%1 %2 / Path %3")
                                      .arg(entryLabel)
                                      .arg(parts[1].toInt() + 1)
                                      .arg(parts[3].toInt() + 1);
        const QString field = parts[4];
        if (field.compare(QStringLiteral("closed"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Closed");
        }
        if (field.compare(QStringLiteral("opacity"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Opacity");
        }
        if (field.compare(QStringLiteral("feather"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather");
        }
        if (field.compare(QStringLiteral("featherHorizontal"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather H");
        }
        if (field.compare(QStringLiteral("featherVertical"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather V");
        }
        if (field.compare(QStringLiteral("featherInner"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather Inner");
        }
        if (field.compare(QStringLiteral("featherOuter"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Feather Outer");
        }
        if (field.compare(QStringLiteral("expansion"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Expansion");
        }
        if (field.compare(QStringLiteral("inverted"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Inverted");
        }
        if (field.compare(QStringLiteral("mode"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Mode");
        }
        if (field.compare(QStringLiteral("name"), Qt::CaseInsensitive) == 0) {
          return pathLabel + QStringLiteral(" / Name");
        }
      }
    }
  }

  const int dot = name.lastIndexOf('.');
  if (dot >= 0 && dot + 1 < name.size()) {
    name = name.mid(dot + 1);
  }

  QString out;
  out.reserve(name.size() * 2);
  for (int i = 0; i < name.size(); ++i) {
    const QChar ch = name.at(i);
    if (ch == '_' || ch == '-') {
      out += ' ';
      continue;
    }
    if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
      out += ' ';
    }
    out += ch;
  }

  bool cap = true;
  for (int i = 0; i < out.size(); ++i) {
    if (out.at(i).isSpace()) {
      cap = true;
      continue;
    }
    if (cap) {
      out[i] = out.at(i).toUpper();
      cap = false;
    }
  }
  return out;
}

bool propertyMatchesFilter(const ArtifactCore::AbstractProperty &property,
                           const QString &filterText) {
  const QString query = filterText.trimmed();
  if (query.isEmpty()) {
    return true;
  }

  const QStringList alternatives = query.split(QLatin1Char('|'),
                                               Qt::SkipEmptyParts);
  if (alternatives.size() > 1) {
    for (const QString &alternative : alternatives) {
      if (propertyMatchesFilter(property, alternative.trimmed())) {
        return true;
      }
    }
    return false;
  }

  const QString key = property.getName();
  const QString friendly = humanizePropertyLabel(key);
  return key.contains(query, Qt::CaseInsensitive) ||
         friendly.contains(query, Qt::CaseInsensitive);
}

void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer);

bool shouldHideInspectorProperty(const QString &propertyName) {
  return false;
}

void applyInspectorPropertyPresentation(
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
  if (!property) {
    return;
  }
  const QString propertyName = property->getName();
  if (propertyName.compare(QStringLiteral("transform.scale.x"),
                           Qt::CaseInsensitive) == 0 ||
      propertyName.compare(QStringLiteral("transform.scale.y"),
                           Qt::CaseInsensitive) == 0) {
    property->setUnit(QStringLiteral("x"));
  }
}

QString scaleSupplementaryText(const ArtifactAbstractLayerPtr &layer,
                               const QString &propertyName,
                               const QVariant &value) {
  if (!layer) {
    return {};
  }

  const auto sourceSize = layer->sourceSize();
  const bool isScaleX =
      propertyName.compare(QStringLiteral("transform.scale.x"),
                           Qt::CaseInsensitive) == 0;
  const bool isScaleY =
      propertyName.compare(QStringLiteral("transform.scale.y"),
                           Qt::CaseInsensitive) == 0;
  if (!isScaleX && !isScaleY) {
    return {};
  }

  const int baseSize = isScaleX ? sourceSize.width : sourceSize.height;
  if (baseSize <= 0) {
    return {};
  }

  const double scale = value.toDouble();
  const int actualSize = std::max(0, static_cast<int>(std::lround(
                                    static_cast<double>(baseSize) * scale)));
  return QStringLiteral("%1 px → %2 px")
      .arg(baseSize)
      .arg(actualSize);
}

void updateScaleSupplementaryText(
    ArtifactPropertyEditorRowWidget *row, const ArtifactAbstractLayerPtr &layer,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
    const QVariant &value) {
  if (!row || !property) {
    return;
  }
  row->setSupplementaryText(
      scaleSupplementaryText(layer, property->getName(), value));
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
inspectorProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties) {
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> filtered;
  filtered.reserve(properties.size());
  for (const auto &property : properties) {
    if (!property || shouldHideInspectorProperty(property->getName())) {
      continue;
    }
    applyInspectorPropertyPresentation(property);
    filtered.push_back(property);
  }
  return filtered;
}

// --- Favorites ---
namespace {
QStringList loadFavoriteProperties() {
  QSettings settings(QStringLiteral("Artifact"),
                     QStringLiteral("PropertyFavorites"));
  return settings.value(QStringLiteral("favorites")).toStringList();
}

void saveFavoriteProperties(const QStringList &favorites) {
  QSettings settings(QStringLiteral("Artifact"),
                     QStringLiteral("PropertyFavorites"));
  settings.setValue(QStringLiteral("favorites"), favorites);
}

bool isFavorite(const QString &propertyPath) {
  return loadFavoriteProperties().contains(propertyPath,
                                           Qt::CaseInsensitive);
}

void toggleFavorite(const QString &propertyPath) {
  auto favs = loadFavoriteProperties();
  const int idx = favs.indexOf(propertyPath, Qt::CaseInsensitive);
  if (idx >= 0) {
    favs.removeAt(idx);
  } else {
    favs.push_back(propertyPath);
  }
  saveFavoriteProperties(favs);
}
} // anonymous namespace

void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer) {
  if (!layer) {
    return;
  }
  auto *composition =
      static_cast<ArtifactAbstractComposition *>(layer->composition());
  layer->changed();
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      composition ? composition->id().toString() : QString{},
      layer->id().toString(), LayerChangedEvent::ChangeType::Modified});
}

void launchExpressionCopilot(
    QWidget *parent, const QString &propertyName,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const QString &initialExpression,
    const ArtifactAbstractLayerPtr &layer,
    const RationalTime &currentTime,
    const std::function<void(const QString &)> &applyHandler = {}) {
  auto *copilot = new ArtifactExpressionCopilotWidget(parent);
  copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
  copilot->setWindowTitle(
      QStringLiteral("Expression Copilot: %1").arg(propertyName));
  copilot->setExpressionText(initialExpression);
  if (layer) {
    auto *composition =
        static_cast<ArtifactAbstractComposition *>(layer->composition());
    QStringList layerNames;
    int currentLayerIndex = -1;
    if (composition) {
      const auto layers = composition->allLayerRef();
      layerNames.reserve(layers.size());
      for (int i = 0; i < layers.size(); ++i) {
        const auto &candidate = layers.at(i);
        if (!candidate) {
          continue;
        }
        layerNames.push_back(candidate->layerName());
        if (candidate->id() == layer->id()) {
          currentLayerIndex = i;
        }
      }
    }
    copilot->setPreviewContext(
        composition ? composition->settings().compositionName().toQString()
                    : QString(),
        composition ? composition->settings().compositionSize() : QSize(),
        layerNames,
        currentLayerIndex,
        layer->layerName(),
        propertyPtr ? propertyPtr->getValue() : QVariant(),
        currentTime.toDouble());
  } else {
    copilot->clearPreviewContext();
  }
  if (propertyPtr && applyHandler) {
    copilot->setApplyHandler([propertyPtr, applyHandler](const QString &expr) {
      propertyPtr->setExpression(expr);
      applyHandler(expr);
    });
  }
  copilot->setAttribute(Qt::WA_DeleteOnClose);
  copilot->move(QCursor::pos() - QPoint(150, 200));
  copilot->show();
}

ArtifactPropertyEditorRowWidget *createPropertyRow(
    QWidget *parent,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue =
        {},
    const std::function<RationalTime()> &currentTimeProvider = {},
    const std::function<void(const QString &)> &keyframeChanged = {},
    const ArtifactAbstractLayerPtr &layer = {},
    const std::function<void(
        ArtifactPropertyEditorRowWidget *,
        const std::shared_ptr<ArtifactCore::AbstractProperty> &,
        const QVariant &)> &rowValueChanged = {}) {
  if (!propertyPtr)
    return nullptr;
  const auto &property = *propertyPtr;

  auto *editor = createPropertyEditorWidget(property, parent);
  if (!editor) {
    return nullptr;
  }

  if (auto *colorEditor = qobject_cast<ArtifactTextAnimatorColorEditor *>(editor)) {
    if (layer) {
      if (auto *textLayer = dynamic_cast<Artifact::ArtifactTextLayer *>(layer.get())) {
        colorEditor->setLayer(textLayer);
      }
    }
  }

  const auto meta = property.metadata();
  const QString labelText = meta.displayLabel.isEmpty()
                                ? humanizePropertyLabel(property.getName())
                                : meta.displayLabel;
  auto *row = new ArtifactPropertyEditorRowWidget(labelText, editor,
                                                  property.getName(), parent);

  auto *playback = ArtifactPlaybackService::instance();

  const auto applyPreviewValue =
      [handler = previewValue ? previewValue : commitValue, propertyPtr,
        playback, currentTimeProvider, keyframeChanged,
        propertyName = property.getName(), row, rowValueChanged, layer](const QVariant &value) {
         if (propertyPtr) {
           propertyPtr->setValue(value);
           if (row && row->isKeyframeModeEnabled() &&
               propertyPtr && (playback || currentTimeProvider)) {
             const auto nowTime = currentTimeProvider
                                      ? currentTimeProvider()
                                      : currentPlaybackTime(playback);
             propertyPtr->setAnimatable(true);
             propertyPtr->addKeyFrame(nowTime, value);
             row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
             row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
             if (keyframeChanged) {
               keyframeChanged(propertyName);
             }
           }
         }
         if (rowValueChanged) {
           rowValueChanged(row, propertyPtr, value);
         }
        handler(propertyName, value);
  };
  const auto applyCommitValue =
      [commitValue, propertyPtr, playback, currentTimeProvider,
       propertyName = property.getName(), row, rowValueChanged,
       keyframeChanged, layer](const QVariant &value) {
        if (propertyPtr) {
          propertyPtr->setValue(value);
          if (row && layer) {
            if (auto *timeline = activeTimelineWidget(row)) {
              timeline->applyValueToSelectedKeyframeArea(layer->id(), propertyName, value);
            }
          }
        }
        if (rowValueChanged) {
          rowValueChanged(row, propertyPtr, value);
        }
        if (row && row->isKeyframeModeEnabled() &&
            propertyPtr && (playback || currentTimeProvider)) {
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);
          propertyPtr->setAnimatable(true);
          propertyPtr->addKeyFrame(nowTime, value);
          row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
          row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        }
        commitValue(propertyName, value);
      };
  editor->setPreviewHandler(applyPreviewValue);
  editor->setCommitHandler(applyCommitValue);

  if (!meta.tooltip.isEmpty()) {
    row->setEditorToolTip(meta.tooltip);
  }

  const QVariant defaultValue = property.getDefaultValue();
  // ✅
  // 設定で有効化されている場合、またはデフォルト値が存在する場合にリセットボタンを表示
  const bool showResetButton =
      Artifact::artifactShouldShowPropertyResetButtons() ||
      defaultValue.isValid();
  row->setShowResetButton(showResetButton);
  if (defaultValue.isValid()) {
    row->setResetHandler([editor, defaultValue]() {
      editor->setValueFromVariant(defaultValue);
      editor->commitCurrentValue();
    });
  }

  const bool animatable = property.isAnimatable();
  row->setShowKeyframeButton(animatable);
  row->setNavigationEnabled(false);
  if (animatable) {
    const QString propertyName = property.getName();
    const auto track = propertyPtr->getKeyFrames();
    row->setNavigationEnabled(!track.empty());
    if (playback || currentTimeProvider) {
      const auto now = currentTimeProvider ? currentTimeProvider()
                                           : currentPlaybackTime(playback);
      row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
      row->setKeyframeModeEnabled(propertyPtr->hasKeyFrameAt(now));
      const QVariant animatedValue = propertyPtr->interpolateValue(now);
      if (animatedValue.isValid()) {
        editor->setValueFromVariant(animatedValue);
      }
    } else {
      row->setKeyframeChecked(!track.empty());
      row->setKeyframeModeEnabled(!track.empty());
    }

    // キーフレームトグル (◆ボタン)
    row->setKeyframeHandler(
        [propertyPtr, playback, row, editor, keyframeChanged,
         currentTimeProvider,
         propertyName](bool checked) {
          if (!propertyPtr)
            return;
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);

          if (checked) {
            propertyPtr->setAnimatable(true);
            propertyPtr->addKeyFrame(nowTime, editor->value());
          } else {
            propertyPtr->removeKeyFrame(nowTime);
          }
          const bool hasAnyKeyframes = !propertyPtr->getKeyFrames().empty();
          row->setKeyframeModeEnabled(checked);
          row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
          row->setNavigationEnabled(hasAnyKeyframes);
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        });

    row->setKeyframeAnchorHandler([propertyPtr, keyframeChanged, propertyName](
                                      ArtifactCore::KeyFrame::Anchor anchor) {
      if (!propertyPtr) {
        return;
      }
      const auto keyframes = propertyPtr->getKeyFrames();
      for (const auto &keyframe : keyframes) {
        propertyPtr->setKeyFrameAnchorAt(keyframe.time, anchor);
      }
      if (keyframeChanged) {
        keyframeChanged(propertyName);
      }
    });

    row->setKeyframeColorLabelHandler(
        [propertyPtr, keyframeChanged, propertyName](
            ArtifactCore::KeyFrame::ColorLabel label) {
          if (!propertyPtr) {
            return;
          }
          const auto keyframes = propertyPtr->getKeyFrames();
          for (const auto &keyframe : keyframes) {
            propertyPtr->setKeyFrameColorLabelAt(keyframe.time, label);
          }
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        });

    // ナビゲーション (◀ ▶ボタン)
    row->setNavigationHandler([propertyPtr, playback, currentTimeProvider](
                                  int direction) {
      if (!playback || !propertyPtr)
        return;
      auto track = propertyPtr->getKeyFrames();
      if (track.empty())
        return;
      std::sort(track.begin(), track.end(),
                [](const auto &lhs, const auto &rhs) {
                  return lhs.time < rhs.time;
                });

      const auto nowTime = currentTimeProvider
                               ? currentTimeProvider()
                               : currentPlaybackTime(playback);
      const int64_t fps_val = std::max<int64_t>(1, nowTime.scale());
      if (direction > 0) {
        // 次のキーフレームへ
        for (const auto &kf : track) {
          if (kf.time > nowTime) {
            playback->goToFrame(
                FramePosition(static_cast<int>(kf.time.rescaledTo(fps_val))));
            break;
          }
        }
      } else {
        // 前のキーフレームへ
        for (auto it = track.rbegin(); it != track.rend(); ++it) {
          if (it->time < nowTime) {
            playback->goToFrame(
                FramePosition(static_cast<int>(it->time.rescaledTo(fps_val))));
            break;
          }
        }
      }
    });
  }

  const bool showExpressionButton = animatable;
  row->setShowExpressionButton(showExpressionButton);
  if (showExpressionButton) {
    const QString initialExpression = propertyPtr && propertyPtr->hasExpression()
        ? propertyPtr->getExpression()
        : (editor ? editor->value().toString() : QString{});
    row->setExpressionHandler(
        [parent, propertyName = property.getName(), propertyPtr, layer, keyframeChanged,
         initialExpression, currentTimeProvider, playback]() {
          const auto nowTime = currentTimeProvider
                                   ? currentTimeProvider()
                                   : currentPlaybackTime(playback);
          launchExpressionCopilot(
              parent,
              propertyName,
              propertyPtr,
              initialExpression,
              layer,
              nowTime,
              [layer, keyframeChanged, propertyName](const QString &) {
                if (keyframeChanged) {
                  keyframeChanged(propertyName);
                }
                if (layer) {
                  notifyLayerPropertyAnimationChanged(layer);
                }
              });
        });
  }

  return row;
}

void alignPropertyRowLabels(
    const std::vector<ArtifactPropertyEditorRowWidget *> &rows,
    const int minimumLabelWidth = kPropertyLabelMinWidth,
    const int maximumLabelWidth = kPropertyLabelMaxWidth) {
  int labelWidth = minimumLabelWidth;
  for (auto *row : rows) {
    if (!row || !row->label()) {
      continue;
    }
    labelWidth = std::max(labelWidth, row->label()->sizeHint().width());
  }
  labelWidth = std::clamp(labelWidth, minimumLabelWidth, maximumLabelWidth);

  for (auto *row : rows) {
    if (!row || !row->label()) {
      continue;
    }
    row->label()->setMinimumWidth(labelWidth);
    row->label()->setMaximumWidth(labelWidth);
    row->label()->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->label()->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  }
}

void addRowsFromProperties(
    QWidget *parent, QVBoxLayout *layout,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    const QString &filterText,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue,
    const std::function<RationalTime()> &currentTimeProvider,
    const std::function<void(const QString &)> &keyframeChanged,
    const ArtifactAbstractLayerPtr &layer,
    bool *addedAny,
    QHash<QString, ArtifactPropertyEditorRowWidget *> *registry = nullptr,
    QSet<QString> *reusedKeys = nullptr,
    std::vector<ArtifactPropertyEditorRowWidget *> *collectedRows = nullptr,
    const std::function<void(ArtifactPropertyEditorRowWidget *,
                             const std::shared_ptr<ArtifactCore::AbstractProperty> &)>
        &decorateRow = {},
    const std::function<void(
        ArtifactPropertyEditorRowWidget *,
        const std::shared_ptr<ArtifactCore::AbstractProperty> &,
        const QVariant &)> &rowValueChanged = {}) {
  for (const auto &ptr : properties) {
    if (!ptr || !propertyMatchesFilter(*ptr, filterText)) {
      continue;
    }
    if (auto *row =
            createPropertyRow(parent, ptr, commitValue, previewValue,
                              currentTimeProvider,
                              keyframeChanged, layer, rowValueChanged)) {
      if (decorateRow) {
        decorateRow(row, ptr);
      }
      layout->addWidget(row);
      if (registry) {
        const QString key = ptr->getName();
        registry->insert(key, row);
        if (reusedKeys) {
          reusedKeys->insert(key);
        }
      }
      if (collectedRows) {
        collectedRows->push_back(row);
      }
      if (addedAny) {
        *addedAny = true;
      }
    }
  }
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
prioritizedSummaryProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
        &properties,
    const std::unordered_set<std::string> &preferredKeys,
    const std::size_t maxCount) {
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> preferred;
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> fallback;
  preferred.reserve(properties.size());
  fallback.reserve(properties.size());

  for (const auto &property : properties) {
    if (!property) {
      continue;
    }
    const auto key = property->getName().toStdString();
    if (preferredKeys.contains(key)) {
      preferred.push_back(property);
    } else {
      fallback.push_back(property);
    }
  }

  preferred.insert(preferred.end(), fallback.begin(), fallback.end());
  if (preferred.size() > maxCount) {
    preferred.resize(maxCount);
  }
  return preferred;
}

} // namespace

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
  auto propertyPtr = impl_->currentLayer->getProperty(propertyName);
  if (!propertyPtr) {
    const auto effects = impl_->currentLayer->getEffects();
    for (const auto &effect : effects) {
      if (!effect) {
        continue;
      }
      if (!impl_->focusedEffectId.isEmpty() &&
          effect->effectID().toQString() != impl_->focusedEffectId) {
        continue;
      }
      for (const auto &property : effect->getProperties()) {
        if (property.getName().compare(propertyName, Qt::CaseInsensitive) == 0) {
          propertyPtr = std::make_shared<ArtifactCore::AbstractProperty>(property);
          break;
        }
      }
      if (propertyPtr) {
        break;
      }
    }
  }
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
  auto propertyPtr = impl_->currentLayer->getProperty(propertyName);
  if (!propertyPtr) {
    const auto effects = impl_->currentLayer->getEffects();
    for (const auto &effect : effects) {
      if (!effect) {
        continue;
      }
      if (!impl_->focusedEffectId.isEmpty() &&
          effect->effectID().toQString() != impl_->focusedEffectId) {
        continue;
      }
      for (const auto &property : effect->getProperties()) {
        if (property.getName().compare(propertyName, Qt::CaseInsensitive) == 0) {
          propertyPtr = std::make_shared<ArtifactCore::AbstractProperty>(property);
          break;
        }
      }
      if (propertyPtr) {
        break;
      }
    }
  }
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
  auto propertyPtr = impl_->currentLayer->getProperty(propertyName);
  if (!propertyPtr) {
    const auto effects = impl_->currentLayer->getEffects();
    for (const auto &effect : effects) {
      if (!effect) {
        continue;
      }
      if (!impl_->focusedEffectId.isEmpty() &&
          effect->effectID().toQString() != impl_->focusedEffectId) {
        continue;
      }
      for (const auto &property : effect->getProperties()) {
        if (property.getName().compare(propertyName, Qt::CaseInsensitive) == 0) {
          propertyPtr = std::make_shared<ArtifactCore::AbstractProperty>(property);
          break;
        }
      }
      if (propertyPtr) {
        break;
      }
    }
  }
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

bool ArtifactPropertyWidget::saveActiveExpressionPreset() {
  if (!impl_) {
    return false;
  }

  auto *row = impl_->activeExpressionRow();
  if (!row || !impl_->currentLayer) {
    return false;
  }

  const QString propertyName = row->propertyName();
  auto propertyPtr = impl_->currentLayer->getProperty(propertyName);
  if (!propertyPtr) {
    const auto effects = impl_->currentLayer->getEffects();
    for (const auto &effect : effects) {
      if (!effect) {
        continue;
      }
      if (!impl_->focusedEffectId.isEmpty() &&
          effect->effectID().toQString() != impl_->focusedEffectId) {
        continue;
      }
      for (const auto &property : effect->getProperties()) {
        if (property.getName().compare(propertyName, Qt::CaseInsensitive) == 0) {
          propertyPtr = std::make_shared<ArtifactCore::AbstractProperty>(property);
          break;
        }
      }
      if (propertyPtr) {
        break;
      }
    }
  }
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
  auto propertyPtr = impl_->currentLayer->getProperty(propertyName);
  if (!propertyPtr) {
    const auto effects = impl_->currentLayer->getEffects();
    for (const auto &effect : effects) {
      if (!effect) {
        continue;
      }
      if (!impl_->focusedEffectId.isEmpty() &&
          effect->effectID().toQString() != impl_->focusedEffectId) {
        continue;
      }
      for (const auto &property : effect->getProperties()) {
        if (property.getName().compare(propertyName, Qt::CaseInsensitive) == 0) {
          propertyPtr = std::make_shared<ArtifactCore::AbstractProperty>(property);
          break;
        }
      }
      if (propertyPtr) {
        break;
      }
    }
  }
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
    const QString &propName = it.key();
    auto *row = it.value();
    if (!row)
      continue;

    auto propertyPtr = currentLayer->getProperty(propName);
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
    row->setNavigationEnabled(hasAnyKeyframes);
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
    const bool isLockRow = it.key().compare(QStringLiteral("layer.locked"),
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
    auto property = currentLayer->getProperty(it.key());
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

  // 既存ウィジェットを全て非表示にする（破棄しない）
  QSet<QString> reusedKeys;
  for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    if (it.value()) {
      it.value()->hide();
      // いったん親から切り離して、group box の破棄に巻き込まれないようにする
      it.value()->setParent(nullptr);
    }
  }

  clearLayoutRecursive(mainLayout);

  if (!currentLayer && compositionEffects.empty()) {
    QLabel *emptyLabel = new QLabel("Select a layer or composition effect to edit properties");
    emptyLabel->setObjectName(QStringLiteral("propertyEmptyLabel"));
    emptyLabel->setAlignment(Qt::AlignCenter);
    applyPropertySectionLabel(emptyLabel, true);
    mainLayout->addWidget(emptyLabel);
    rebuilding = false;
    return;
  }

  // ウィジェット再利用ヘルパー
  auto getOrCreateRow =
      [this, &reusedKeys](const QString &key,
                          auto createFn) -> ArtifactPropertyEditorRowWidget * {
    auto it = propertyEditors.find(key);
    if (it != propertyEditors.end() && it.value()) {
      reusedKeys.insert(key);
      it.value()->show();
      return it.value();
    }
    auto *row = createFn();
    if (row) {
      propertyEditors.insert(key, row);
      reusedKeys.insert(key);
    }
    return row;
  };

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
  mainLayout->addWidget(searchEdit);

  // Favorite-only toggle
  auto *favRow = new QWidget();
  favRow->setObjectName(QStringLiteral("favoriteToggleRow"));
  auto *favLayout = new QHBoxLayout(favRow);
  favLayout->setContentsMargins(4, 2, 4, 2);
  auto *favToggle = new QPushButton(favoriteOnly ? QStringLiteral("★ Only")
                                                  : QStringLiteral("☆ Only"));
  favToggle->setCheckable(true);
  favToggle->setChecked(favoriteOnly);
  favToggle->setObjectName(QStringLiteral("favoriteToggleButton"));
  favToggle->setFlat(true);
  favToggle->setCursor(Qt::PointingHandCursor);
  favToggle->setToolTip(QStringLiteral("Show only favorite properties"));
  favLayout->addWidget(favToggle);
  favLayout->addStretch();
   QObject::connect(favToggle, &QPushButton::toggled, owner,
                    [this](bool checked) {
                      if (owner) {
                        owner->setFavoriteOnly(checked);
                      }
                    });
  mainLayout->addWidget(favRow);

  // Multi-selection badge
  if (targetLayers.size() > 1) {
    auto *multiBadge = new QLabel(
        QStringLiteral("✏️ Editing %1 layers").arg(targetLayers.size()));
    multiBadge->setObjectName(QStringLiteral("multiLayerBadge"));
    multiBadge->setAlignment(Qt::AlignCenter);
    multiBadge->setStyleSheet(QStringLiteral(
        "background: #3C5B76; color: #E3E7EC; padding: 4px 8px; "
        "border-radius: 4px; font-weight: bold; font-size: 11px;"));
    mainLayout->addWidget(multiBadge);
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
          &addedGroupProperties, &propertyEditors, &reusedKeys, &effectRows,
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

    QStringList toRemove;
    for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
      if (!reusedKeys.contains(it.key())) {
        if (it.value()) {
          it.value()->deleteLater();
        }
        toRemove.append(it.key());
      }
    }
    for (const auto &key : toRemove) {
      propertyEditors.remove(key);
    }

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

  for (const auto &groupDef : layerGroups) {
    const QString groupName =
        groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name();
    const bool isSourceReframe = groupName.compare(QStringLiteral("Source Reframe"),
                                                   Qt::CaseInsensitive) == 0;
    auto sortedProps =
        filteredGroupProperties(layer, groupName,
                                inspectorProperties(groupDef.sortedProperties()));
    // Apply favorites filter when in favorite-only mode
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
      continue;
    }

    QGroupBox *group = new QGroupBox(
        isSourceReframe ? QStringLiteral("Crop / Pan") : groupName);
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);
    QWidget *sectionBody = group;
    CollapsibleSectionButton *collapseButton = nullptr;
    if (groupName.compare(QStringLiteral("Initial"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("Rig"), Qt::CaseInsensitive) == 0 ||
        groupName.compare(QStringLiteral("Rig Controls"), Qt::CaseInsensitive) == 0) {
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

    if (isSourceReframe) {
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
                           layer->setLayerPropertyValue(
                               QStringLiteral("sourceCrop.enabled"), true);
                           notifyLayerPropertyAnimationChanged(layer);
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
      });

      QObject::connect(disableButton, &QPushButton::clicked, group, [this, layer]() {
        if (!layer) {
          return;
        }
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), false);
        notifyLayerPropertyAnimationChanged(layer);
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
            layer, &addedGroupProperties, &propertyEditors, &reusedKeys,
            &windowRows,
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
            layer, &addedGroupProperties, &propertyEditors, &reusedKeys,
            &motionRows,
            decorateLayerRow, updateLayerRowValue);
        groupRows.insert(groupRows.end(), motionRows.begin(), motionRows.end());
      }
      }
    } else {
      addRowsFromProperties(
          group, contentLayout, sortedProps, filterText, commitLayerValue,
          previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
          layer, &addedGroupProperties, &propertyEditors, &reusedKeys,
          &groupRows,
          decorateLayerRow, updateLayerRowValue);
    }

    const bool hasSourceCrop =
        layer && layer->getProperty(QStringLiteral("sourceCrop.enabled")) &&
        layer->getProperty(QStringLiteral("sourceCrop.cropX")) &&
        layer->getProperty(QStringLiteral("sourceCrop.panX"));
    if (groupName.compare(QStringLiteral("Transform"), Qt::CaseInsensitive) == 0 &&
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
        layer->setLayerPropertyValue(QStringLiteral("sourceCrop.enabled"), true);
        notifyLayerPropertyAnimationChanged(layer);
        pendingScrollGroupName = QStringLiteral("Crop / Pan");
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
        &addedGroupProperties, &propertyEditors, &reusedKeys, &effectRows,
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
  QStringList toRemove;
  for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
    if (!reusedKeys.contains(it.key())) {
      if (it.value()) {
        it.value()->deleteLater();
      }
      toRemove.append(it.key());
    }
  }
  for (const auto &key : toRemove) {
    propertyEditors.remove(key);
  }

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


