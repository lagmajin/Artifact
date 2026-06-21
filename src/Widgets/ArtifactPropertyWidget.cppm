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
#include <QDir>
#include <QPalette>
#include <QPushButton>
#include <QStandardPaths>
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
import Artifact.Composition.Abstract;
import Property;
import Property.Abstract;
import Property.Group;
import Undo.UndoManager;
import Artifact.Effect.Abstract;
import Artifact.Application.Manager;
import Utils.String.UniString;
import Widgets.Utils.CSS;
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
  bool isPlaying = false;
  int localPropertyEditDepth = 0;
  QHash<QString, ArtifactPropertyEditorRowWidget *> propertyEditors;
  QString rebuildSignature;
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
        propertyName = property.getName(), row, rowValueChanged](const QVariant &value) {
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
       keyframeChanged](const QVariant &value) {
        if (propertyPtr) {
          propertyPtr->setValue(value);
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
        registry->insert(ptr->getName(), row);
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

  signature += QStringLiteral("valueColumnFirst:");
  signature += valueColumnFirst ? QStringLiteral("1") : QStringLiteral("0");
  signature += QLatin1Char('\n');

  signature += QStringLiteral("sliderBeforeValue:");
  signature += sliderBeforeValue ? QStringLiteral("1") : QStringLiteral("0");
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
  if (impl_->currentLayer == layer) {
    // Same layer object – still refresh values in case they changed externally
    impl_->invalidatePropertyValueCache();
    impl_->scheduleUpdateValues();
    return;
  }

  if (impl_->currentLayerChangedConnection) {
    QObject::disconnect(impl_->currentLayerChangedConnection);
  }

  impl_->currentLayer = layer;
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
                  impl_->scheduleUpdateValues();
                });
  }

  updateProperties();
}

void ArtifactPropertyWidget::setFocusedEffectId(const QString &effectId) {
  if (impl_->focusedEffectId == effectId)
    return;
  impl_->focusedEffectId = effectId;
  updateProperties();
}

void ArtifactPropertyWidget::clear() {
  impl_->currentLayer = nullptr;
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

  if (!currentLayer) {
    QLabel *emptyLabel = new QLabel("Select a layer to edit properties");
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

  bool hasAnyProperties = false;

  auto *summaryGroup = new QGroupBox(QStringLiteral("Summary"));
  auto *summaryLayout = new QVBoxLayout(summaryGroup);
  summaryLayout->setContentsMargins(10, 8, 10, 8);
  summaryLayout->setSpacing(5);

  const std::unordered_set<std::string> keyLayerProperties = {
      "layer.name",        "transform.position.x", "transform.position.y",
      "transform.scale.x", "transform.scale.y",    "transform.rotation",
      "transform.initialRotation", "source.width", "source.height",
      "layer.opacity"};

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
  if (layer) {
    const auto presentation = describeLayerPresentation(layer);
    summaryGroup->setTitle(presentation.propertySummaryTitle);
    if (!presentation.capabilitySummaryText.isEmpty()) {
      auto *capabilityLabel = new QLabel(
          QStringLiteral("Capability: %1").arg(presentation.capabilitySummaryText),
          summaryGroup);
      capabilityLabel->setObjectName(QStringLiteral("propertySectionNote"));
      capabilityLabel->setWordWrap(true);
      applyPresentationToneLabel(capabilityLabel, presentation.badgeTone, false);
      summaryLayout->addWidget(capabilityLabel);
    }
  }
  const auto ownerSnapshots =
      ArtifactCore::PropertyRegistryReadOnlyAdapter::queryAllOwners();
  if (!ownerSnapshots.isEmpty()) {
    QString ownerSummaryText = QStringLiteral("Snapshot owners:");
    int validOwnerCount = 0;
    for (const auto &ownerSnapshot : ownerSnapshots) {
      if (!ownerSnapshot.isValid) {
        continue;
      }
      ++validOwnerCount;
      ownerSummaryText += QLatin1Char('\n');
      ownerSummaryText += QStringLiteral("* ");
      ownerSummaryText += ownerSnapshot.displayName.isEmpty()
                              ? ownerSnapshot.ownerPath
                              : ownerSnapshot.displayName;
      ownerSummaryText += QStringLiteral(" (");
      ownerSummaryText += QString::number(ownerSnapshot.propertyCount);
      ownerSummaryText += QStringLiteral(" properties)");
    }
    if (validOwnerCount > 0) {
      auto *ownerLabel = new QLabel(ownerSummaryText, summaryGroup);
      ownerLabel->setObjectName(QStringLiteral("propertySectionNote"));
      ownerLabel->setWordWrap(true);
      applyPropertySectionLabel(ownerLabel, false);
      summaryLayout->addWidget(ownerLabel);
    }
  }
  std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
      layerSummaryProperties;
  for (const auto &groupDef : layerGroups) {
    auto sortedProps = inspectorProperties(groupDef.sortedProperties());
    auto picked =
        prioritizedSummaryProperties(sortedProps, keyLayerProperties, 6);
    layerSummaryProperties.insert(layerSummaryProperties.end(), picked.begin(),
                                  picked.end());
  }

  auto removePropertyByName = [&layerSummaryProperties](
                                  const QString &propertyName) {
    auto it = std::find_if(
        layerSummaryProperties.begin(), layerSummaryProperties.end(),
        [&propertyName](
            const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
          return property && property->getName().compare(
                                 propertyName, Qt::CaseInsensitive) == 0;
        });
    if (it == layerSummaryProperties.end()) {
      return std::shared_ptr<ArtifactCore::AbstractProperty>();
    }
    auto property = *it;
    layerSummaryProperties.erase(it);
    return property;
  };

  bool hasSummaryProperties = false;
  std::vector<ArtifactPropertyEditorRowWidget *> summaryRows;
  applyPropertySectionBox(summaryGroup);
  if (auto layerNameProperty =
          removePropertyByName(QStringLiteral("layer.name"))) {
    if (auto *row = createPropertyRow(
            summaryGroup, layerNameProperty,
            [this, layer](const QString &name, const QVariant &value) {
              if (layer) {
                ScopedPropertyEditGuard guard(localPropertyEditDepth);
                layer->setLayerPropertyValue(name, value);
                notifyLayerPropertyAnimationChanged(layer);
              }
            },
            {}, currentLayerTime, notifyLayerKeyframeChanged, layer)) {
      summaryLayout->addWidget(row);
      propertyEditors.insert(layerNameProperty->getName(), row);
      summaryRows.push_back(row);
      hasSummaryProperties = true;
    }
  }

  auto summaryPreviewOpacity = std::make_shared<std::optional<float>>();
  const auto decorateLayerRow =
      [layer](ArtifactPropertyEditorRowWidget *row,
              const std::shared_ptr<ArtifactCore::AbstractProperty> &property) {
        if (!row || !property) {
          return;
        }
        updateScaleSupplementaryText(row, layer, property,
                                     property->getValue());
      };
  const auto updateLayerRowValue =
      [layer](ArtifactPropertyEditorRowWidget *row,
              const std::shared_ptr<ArtifactCore::AbstractProperty> &property,
              const QVariant &value) {
        updateScaleSupplementaryText(row, layer, property, value);
      };
  addRowsFromProperties(
      summaryGroup, summaryLayout, layerSummaryProperties, filterText,
      [this, layer, summaryPreviewOpacity](const QString &name, const QVariant &value) {
        if (layer) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          if (name.compare(QStringLiteral("layer.opacity"),
                           Qt::CaseInsensitive) == 0) {
            const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
            const float oldOpacity =
                summaryPreviewOpacity->value_or(layer->opacity());
            summaryPreviewOpacity->reset();
            if (std::abs(oldOpacity - newOpacity) > 0.0001f) {
              auto *cmd = new ChangeLayerOpacityCommand(layer, oldOpacity,
                                                        newOpacity);
              UndoManager::instance()->push(
                  std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
            }
          } else {
            layer->setLayerPropertyValue(name, value);
            notifyLayerPropertyAnimationChanged(layer);
          }
        }
      },
      [this, layer, summaryPreviewOpacity](const QString &name, const QVariant &value) {
        if (layer) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          if (name.compare(QStringLiteral("layer.opacity"),
                           Qt::CaseInsensitive) == 0) {
            const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
            if (!summaryPreviewOpacity->has_value()) {
              *summaryPreviewOpacity = layer->opacity();
            }
            layer->setOpacity(newOpacity);
          } else {
            layer->setLayerPropertyValue(name, value);
            notifyLayerPropertyAnimationChanged(layer);
          }
        }
      },
      currentLayerTime,
      notifyLayerKeyframeChanged,
      layer,
      &hasSummaryProperties, &propertyEditors, &summaryRows,
      decorateLayerRow, updateLayerRowValue);

  const auto effects = layer->getEffects();
  const bool hasFocusedEffect = !focusedEffectId.trimmed().isEmpty();
  for (const auto &effect : effects) {
    if (!effect) {
      continue;
    }
    if (hasFocusedEffect && effect->effectID().toQString() != focusedEffectId) {
      continue;
    }

    ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
    for (const auto &property : effect->getProperties()) {
      propGroup.addProperty(
          std::make_shared<ArtifactCore::AbstractProperty>(property));
    }

    auto effectSummary =
        prioritizedSummaryProperties(propGroup.sortedProperties(), {}, 3);
    if (effectSummary.empty()) {
      continue;
    }

    auto *effectLabel = new QLabel(effect->displayName().toQString(), summaryGroup);
    effectLabel->setObjectName(QStringLiteral("propertySectionLabel"));
    applyPropertySectionLabel(effectLabel, false);
    summaryLayout->addWidget(effectLabel);

    std::vector<ArtifactPropertyEditorRowWidget *> effectSummaryRows;
    addRowsFromProperties(
        summaryGroup, summaryLayout, effectSummary, filterText,
        [this, layer, effect](const QString &name, const QVariant &value) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          effect->setPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(layer);
        },
        {},
        currentLayerTime,
        {},
        layer,
        &hasSummaryProperties, &propertyEditors, &effectSummaryRows,
        {});
    alignPropertyRowLabels(effectSummaryRows, 132, 176);
  }

  if (hasSummaryProperties) {
    alignPropertyRowLabels(summaryRows, 132, 176);
    mainLayout->addWidget(summaryGroup);
    hasAnyProperties = true;
  } else {
    delete summaryGroup;
  }

  for (const auto &groupDef : layerGroups) {
    const QString groupName =
        groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name();
    const bool isSourceReframe = groupName.compare(QStringLiteral("Source Reframe"),
                                                   Qt::CaseInsensitive) == 0;
    auto sortedProps = inspectorProperties(groupDef.sortedProperties());
    if (sortedProps.empty()) {
      continue;
    }

    QGroupBox *group = new QGroupBox(
        isSourceReframe ? QStringLiteral("Pan / Crop") : groupName);
    if (groupName.compare(QStringLiteral("Initial"), Qt::CaseInsensitive) == 0) {
      group->setCheckable(true);
      group->setChecked(true);
    }
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);
    bool addedGroupProperties = false;
    std::vector<ArtifactPropertyEditorRowWidget *> groupRows;
    auto groupPreviewOpacity = std::make_shared<std::optional<float>>();
    auto commitLayerValue = [this, layer, groupPreviewOpacity](
                                const QString &name, const QVariant &value) {
      if (layer) {
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        if (name.compare(QStringLiteral("layer.opacity"),
                         Qt::CaseInsensitive) == 0) {
          const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
          const float oldOpacity =
              groupPreviewOpacity->value_or(layer->opacity());
          groupPreviewOpacity->reset();
          if (std::abs(oldOpacity - newOpacity) > 0.0001f) {
            auto *cmd = new ChangeLayerOpacityCommand(layer, oldOpacity,
                                                      newOpacity);
            UndoManager::instance()->push(
                std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
          }
        } else {
          layer->setLayerPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(layer);
        }
      }
    };
    auto previewLayerValue = [this, layer, groupPreviewOpacity](
                                 const QString &name, const QVariant &value) {
      if (layer) {
        ScopedPropertyEditGuard guard(localPropertyEditDepth);
        if (name.compare(QStringLiteral("layer.opacity"),
                         Qt::CaseInsensitive) == 0) {
          const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
          if (!groupPreviewOpacity->has_value()) {
            *groupPreviewOpacity = layer->opacity();
          }
          layer->setOpacity(newOpacity);
        } else {
          layer->setLayerPropertyValue(name, value);
          notifyLayerPropertyAnimationChanged(layer);
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
        auto *enableButton = new QPushButton(QStringLiteral("Show Pan / Crop"),
                                             group);
        enableButton->setCursor(Qt::PointingHandCursor);
        groupLayout->addWidget(enableButton);
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
            QStringLiteral("Enable Pan / Crop to reveal source window and motion controls."),
            group);
        note->setObjectName(QStringLiteral("propertySectionNote"));
        note->setWordWrap(true);
        applyPropertySectionLabel(note, false);
        applyThemeTextPalette(note, 120);
        groupLayout->addWidget(note);

        addedGroupProperties = true;
      } else {
      auto *note = new QLabel(
          QStringLiteral("Source reframe behaves like Pan/Crop: the window looks into the source, then layer transform places it in comp."));
      note->setObjectName(QStringLiteral("propertySectionNote"));
      note->setWordWrap(true);
      applyPropertySectionLabel(note, false);
      applyThemeTextPalette(note, 120);
      groupLayout->addWidget(note);

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
        auto *windowLabel = new QLabel(QStringLiteral("Window"), group);
        windowLabel->setObjectName(QStringLiteral("propertySectionLabel"));
        applyPropertySectionLabel(windowLabel, true);
        groupLayout->addWidget(windowLabel);

        std::vector<ArtifactPropertyEditorRowWidget *> windowRows;
        addRowsFromProperties(
            group, groupLayout, windowProps, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, &propertyEditors, &windowRows,
            decorateLayerRow, updateLayerRowValue);
        groupRows.insert(groupRows.end(), windowRows.begin(), windowRows.end());
      }

      if (!motionProps.empty()) {
        auto *motionLabel = new QLabel(QStringLiteral("Motion"), group);
        motionLabel->setObjectName(QStringLiteral("propertySectionLabel"));
        applyPropertySectionLabel(motionLabel, true);
        groupLayout->addWidget(motionLabel);

        std::vector<ArtifactPropertyEditorRowWidget *> motionRows;
        addRowsFromProperties(
            group, groupLayout, motionProps, filterText, commitLayerValue,
            previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
            layer, &addedGroupProperties, &propertyEditors, &motionRows,
            decorateLayerRow, updateLayerRowValue);
        groupRows.insert(groupRows.end(), motionRows.begin(), motionRows.end());
      }
      }
    } else {
      addRowsFromProperties(
          group, groupLayout, sortedProps, filterText, commitLayerValue,
          previewLayerValue, currentLayerTime, notifyLayerKeyframeChanged,
          layer, &addedGroupProperties, &propertyEditors, &groupRows,
          decorateLayerRow, updateLayerRowValue);
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
        &addedGroupProperties, &propertyEditors, &effectRows);

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

  rebuilding = false;
}

} // namespace Artifact
