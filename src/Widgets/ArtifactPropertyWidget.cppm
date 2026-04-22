module;

#include <QColor>
#include <QCursor>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMetaObject>
#include <QPalette>
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
import Artifact.Composition.Abstract;
import Property;
import Property.Abstract;
import Property.Group;
import Undo.UndoManager;
import Artifact.Effect.Abstract;
import Artifact.Application.Manager;
import Utils.String.UniString;
import Widgets.Utils.CSS;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Widgets.PropertyEditor;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;
import Time.Rational;

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
  int localPropertyEditDepth = 0;
  QHash<QString, ArtifactPropertyEditorRowWidget *> propertyEditors;
  QString rebuildSignature;
  qint64 lastPropertyUpdateFramePosition = std::numeric_limits<qint64>::min();
  int64_t lastPropertyUpdateFps = -1;

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

  const QString key = property.getName();
  const QString friendly = humanizePropertyLabel(key);
  return key.contains(query, Qt::CaseInsensitive) ||
         friendly.contains(query, Qt::CaseInsensitive);
}

void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr &layer);

void notifyProjectIfLayerNameChanged(const ArtifactAbstractLayerPtr &layer,
                                     const QString &propertyName) {
  if (propertyName.compare(QStringLiteral("layer.name"), Qt::CaseInsensitive) !=
      0) {
    return;
  }
  notifyLayerPropertyAnimationChanged(layer);
}

void notifyProjectIfTimelinePropertyChanged(const ArtifactAbstractLayerPtr &layer,
                                            const QString &propertyName) {
  const bool isTimelineProperty =
      propertyName.compare(QStringLiteral("time.inPoint"),
                           Qt::CaseInsensitive) == 0 ||
      propertyName.compare(QStringLiteral("time.outPoint"),
                           Qt::CaseInsensitive) == 0 ||
      propertyName.compare(QStringLiteral("time.startTime"),
                           Qt::CaseInsensitive) == 0;
  if (!isTimelineProperty) {
    return;
  }
  notifyLayerPropertyAnimationChanged(layer);
}

bool shouldHideInspectorProperty(const QString &propertyName) {
  return propertyName.compare(QStringLiteral("layer.visible"),
                              Qt::CaseInsensitive) == 0 ||
         propertyName.compare(QStringLiteral("layer.locked"),
                              Qt::CaseInsensitive) == 0 ||
         propertyName.compare(QStringLiteral("layer.guide"),
                              Qt::CaseInsensitive) == 0 ||
         propertyName.compare(QStringLiteral("layer.solo"),
                              Qt::CaseInsensitive) == 0;
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

ArtifactPropertyEditorRowWidget *createPropertyRow(
    QWidget *parent,
    const std::shared_ptr<ArtifactCore::AbstractProperty> &propertyPtr,
    const std::function<void(const QString &, const QVariant &)> &commitValue,
    const std::function<void(const QString &, const QVariant &)> &previewValue =
        {},
    const std::function<void(const QString &)> &keyframeChanged = {},
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

  const auto meta = property.metadata();
  const QString labelText = meta.displayLabel.isEmpty()
                                ? humanizePropertyLabel(property.getName())
                                : meta.displayLabel;
  auto *row = new ArtifactPropertyEditorRowWidget(labelText, editor,
                                                  property.getName(), parent);

  auto *playback = ArtifactPlaybackService::instance();
  const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
  const int64_t fps_val =
      static_cast<int64_t>(std::round(frameRate.framerate()));

  const auto applyPreviewValue =
      [handler = previewValue ? previewValue : commitValue, propertyPtr,
        propertyName = property.getName(), row, rowValueChanged](const QVariant &value) {
         if (propertyPtr) {
           propertyPtr->setValue(value);
         }
         if (rowValueChanged) {
           rowValueChanged(row, propertyPtr, value);
         }
        handler(propertyName, value);
  };
  const auto applyCommitValue =
      [commitValue, propertyPtr, playback, fps_val,
       propertyName = property.getName(), row, rowValueChanged,
       keyframeChanged](const QVariant &value) {
        if (propertyPtr) {
          propertyPtr->setValue(value);
        }
        if (rowValueChanged) {
          rowValueChanged(row, propertyPtr, value);
        }
        if (propertyPtr && playback) {
          const auto nowPos = playback->currentFrame();
          const auto nowTime = RationalTime(nowPos.framePosition(), fps_val);
          const bool hasAnyKeyframes = !propertyPtr->getKeyFrames().empty();
          if (hasAnyKeyframes || propertyPtr->hasKeyFrameAt(nowTime)) {
            propertyPtr->setAnimatable(true);
            propertyPtr->addKeyFrame(nowTime, value);
            row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
            row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
            if (keyframeChanged) {
              keyframeChanged(propertyName);
            }
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

    // 現時点でのキーフレーム状態を反映
    if (playback) {
      const auto now =
          RationalTime(playback->currentFrame().framePosition(), fps_val);
      row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
      const QVariant animatedValue = propertyPtr->interpolateValue(now);
      if (animatedValue.isValid()) {
        editor->setValueFromVariant(animatedValue);
      }
    }

    // キーフレームトグル (◆ボタン)
    row->setKeyframeHandler(
        [propertyPtr, playback, row, editor, fps_val, keyframeChanged,
         propertyName](bool checked) {
          if (!playback || !propertyPtr)
            return;
          const auto nowPos = playback->currentFrame();
          const auto nowTime = RationalTime(nowPos.framePosition(), fps_val);

          if (checked) {
            propertyPtr->setAnimatable(true);
            propertyPtr->addKeyFrame(nowTime, editor->value());
          } else {
            propertyPtr->removeKeyFrame(nowTime);
          }
          // 状態を再反映
          row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
          row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
          if (keyframeChanged) {
            keyframeChanged(propertyName);
          }
        });

    // ナビゲーション (◀ ▶ボタン)
    row->setNavigationHandler([propertyPtr, playback, fps_val](int direction) {
      if (!playback || !propertyPtr)
        return;
      const auto track = propertyPtr->getKeyFrames();
      if (track.empty())
        return;

      const auto nowTime =
          RationalTime(playback->currentFrame().framePosition(), fps_val);
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
    row->setExpressionHandler([propertyName = property.getName(), editor]() {
      auto *copilot = new ArtifactExpressionCopilotWidget();
      copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
      copilot->setWindowTitle(
          QStringLiteral("Expression Copilot: %1").arg(propertyName));
      if (editor) {
        copilot->setExpressionText(editor->value().toString());
      }
      copilot->setAttribute(Qt::WA_DeleteOnClose);
      copilot->move(QCursor::pos() - QPoint(150, 200));
      copilot->show();
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
    const std::function<void(const QString &)> &keyframeChanged,
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
                              keyframeChanged, rowValueChanged)) {
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
    impl_->updatePropertyValues();
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
  if (auto *playback = ArtifactPlaybackService::instance()) {
    QObject::connect(playback, &ArtifactPlaybackService::frameChanged, this,
                     [this, playback]() {
                       if (isVisible()) {
                         // [Optimization] If playing, only update if it's the
                         // first frame of playback or not playing.
                         // High-frequency UI updates during playback can cause
                         // significant lag.
                         if (!playback->isPlaying()) {
                           impl_->updatePropertyValues();
                         }
                       } else {
                         impl_->needsRebuildWhenVisible = true;
                       }
                     });
    QObject::connect(playback, &ArtifactPlaybackService::playbackStateChanged,
                     this, [this](PlaybackState state) {
                       if (state != PlaybackState::Playing) {
                         impl_->updatePropertyValues();
                       }
                     });
  }
  if (auto *projectService = ArtifactProjectService::instance()) {
    QObject::connect(projectService, &ArtifactProjectService::projectChanged,
                     this, [this]() {
                       if (impl_->currentLayer) {
                         impl_->scheduleRebuild();
                       }
                     });
  }
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

void ArtifactPropertyWidget::Impl::updatePropertyValues() {
  if (!currentLayer || rebuilding)
    return;

  auto *playback = ArtifactPlaybackService::instance();
  const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
  const int64_t fps_val =
      static_cast<int64_t>(std::round(frameRate.framerate()));
  const qint64 framePosition =
      playback ? static_cast<qint64>(playback->currentFrame().framePosition())
               : 0;
  if (framePosition == lastPropertyUpdateFramePosition &&
      fps_val == lastPropertyUpdateFps) {
    applyLockState();
    return;
  }
  lastPropertyUpdateFramePosition = framePosition;
  lastPropertyUpdateFps = fps_val;
  const auto now = RationalTime(framePosition, fps_val);

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

    // キーフレーム状態（◆ボタン）の更新
    row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
    row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
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
    QLabel *emptyLabel = new QLabel("No layer selected");
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
      "layer.opacity"};

  const ArtifactAbstractLayerPtr layer = currentLayer;
  registerCurrentLayerPropertySnapshot(layer, focusedEffectId);

  const auto notifyLayerKeyframeChanged = [this, layer](const QString &) {
    if (layer) {
      notifyLayerPropertyAnimationChanged(layer);
    }
  };

  const auto layerGroups = layer ? layer->getLayerPropertyGroups()
                                 : std::vector<ArtifactCore::PropertyGroup>{};
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
                notifyProjectIfLayerNameChanged(layer, name);
                notifyProjectIfTimelinePropertyChanged(layer, name);
              }
            })) {
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
          }
          notifyProjectIfLayerNameChanged(layer, name);
          notifyProjectIfTimelinePropertyChanged(layer, name);
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
          }
          notifyProjectIfLayerNameChanged(layer, name);
          notifyProjectIfTimelinePropertyChanged(layer, name);
        }
      },
      notifyLayerKeyframeChanged,
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

    auto *effectLabel = new QLabel(
        QStringLiteral("Effect: %1").arg(effect->displayName().toQString()),
        summaryGroup);
    effectLabel->setObjectName(QStringLiteral("propertySectionLabel"));
    applyPropertySectionLabel(effectLabel, true);
    summaryLayout->addWidget(effectLabel);

    std::vector<ArtifactPropertyEditorRowWidget *> effectSummaryRows;
    addRowsFromProperties(
        summaryGroup, summaryLayout, effectSummary, filterText,
        [this, effect](const QString &name, const QVariant &value) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          effect->setPropertyValue(name, value);
        },
        {},
        {},
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
    QGroupBox *group = new QGroupBox(
        groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name());
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);

    auto sortedProps = inspectorProperties(groupDef.sortedProperties());
    bool addedGroupProperties = false;
    std::vector<ArtifactPropertyEditorRowWidget *> groupRows;
    auto groupPreviewOpacity = std::make_shared<std::optional<float>>();
    addRowsFromProperties(
        group, groupLayout, sortedProps, filterText,
        [this, layer, groupPreviewOpacity](const QString &name, const QVariant &value) {
          if (layer) {
            ScopedPropertyEditGuard guard(localPropertyEditDepth);
            if (name.compare(QStringLiteral("layer.opacity"),
                             Qt::CaseInsensitive) == 0) {
              const float newOpacity = std::clamp(value.toFloat(), 0.0f, 1.0f);
              const float oldOpacity =
                  groupPreviewOpacity->value_or(layer->opacity());
              groupPreviewOpacity->reset();
              if (std::abs(oldOpacity - newOpacity) > 0.0001f) {
                auto *cmd = new ChangeLayerOpacityCommand(layer,
                                                          oldOpacity, newOpacity);
                UndoManager::instance()->push(
                    std::unique_ptr<ChangeLayerOpacityCommand>(cmd));
              }
            } else {
              layer->setLayerPropertyValue(name, value);
            }
            notifyProjectIfLayerNameChanged(layer, name);
            notifyProjectIfTimelinePropertyChanged(layer, name);
          }
        },
        [this, layer, groupPreviewOpacity](const QString &name, const QVariant &value) {
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
            }
            notifyProjectIfLayerNameChanged(layer, name);
            notifyProjectIfTimelinePropertyChanged(layer, name);
          }
        },
        notifyLayerKeyframeChanged,
        &addedGroupProperties, &propertyEditors, &groupRows,
        decorateLayerRow, updateLayerRowValue);
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

    QString stageName = "Unknown";
    switch (effect->pipelineStage()) {
    case EffectPipelineStage::Generator:
      stageName = "[Generator]";
      break;
    case EffectPipelineStage::GeometryTransform:
      stageName = "[Geo Transform]";
      break;
    case EffectPipelineStage::MaterialRender:
      stageName = "[Material]";
      break;
    case EffectPipelineStage::Rasterizer:
      stageName = "[Rasterizer]";
      break;
    case EffectPipelineStage::LayerTransform:
      stageName = "[Layer Transform]";
      break;
    }

    QGroupBox *group = new QGroupBox(
        QString("%1 %2").arg(stageName).arg(effect->displayName().toQString()));
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(10, 8, 10, 8);
    groupLayout->setSpacing(5);
    applyPropertySectionBox(group);
    applyThemeTextPalette(group, 120);

    ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
    for (const auto &p : effect->getProperties()) {
      propGroup.addProperty(
          std::make_shared<ArtifactCore::AbstractProperty>(p));
    }

    auto sortedProps = propGroup.sortedProperties();
    bool addedGroupProperties = false;
    std::vector<ArtifactPropertyEditorRowWidget *> effectRows;
    addRowsFromProperties(
        group, groupLayout, sortedProps, filterText,
        [this, effect](const QString &name, const QVariant &value) {
          ScopedPropertyEditGuard guard(localPropertyEditDepth);
          effect->setPropertyValue(name, value);
        },
        {},
        {},
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
