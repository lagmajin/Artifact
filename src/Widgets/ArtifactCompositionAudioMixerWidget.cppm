module;
#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include <cmath>
#include <QAction>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QMetaObject>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionAudioMixer;

import Artifact.Audio.Mixer;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Service.Effect;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Event.Bus;
import std;

namespace Artifact {

namespace {
struct AudioFxChipInfo {
  QString id;
  QString displayName;
  bool enabled = true;
  bool operator==(const AudioFxChipInfo &other) const {
    return id == other.id && displayName == other.displayName &&
           enabled == other.enabled;
  }
};

void queueMixerRefresh(Artifact::ArtifactCompositionAudioMixerWidget* widget)
{
  if (!widget) {
    return;
  }
  QPointer<Artifact::ArtifactCompositionAudioMixerWidget> safeWidget(widget);
  QMetaObject::invokeMethod(
      widget,
      [safeWidget]() {
        if (safeWidget) {
          safeWidget->refreshFromCurrentComposition();
        }
      },
      Qt::QueuedConnection);
}

float sliderValueToVolume(const int value) {
  return std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 2.0f);
}

int volumeToPercent(const float volume) {
  return static_cast<int>(std::lround(std::clamp(volume, 0.0f, 2.0f) * 100.0f));
}

float volumeToMeterDb(const float volume) {
  if (volume <= 0.0001f) {
    return -60.0f;
  }
  return std::clamp(20.0f * std::log10(volume), -60.0f, 6.02f);
}

QString volumeToDisplayText(const float volume) {
  if (volume <= 0.0001f) {
    return QStringLiteral("-inf dB");
  }
  const float db = volumeToMeterDb(volume);
  const QString dbText = QString::number(db, 'f', 1);
  return (db > 0.0f ? QStringLiteral("+") : QString()) + dbText +
         QStringLiteral(" dB");
}

QColor mixerAccentForName(const QString &name, const bool master = false) {
  if (master) {
    return QColor(211, 170, 66);
  }
  const uint hash = qHash(name);
  QColor color = QColor::fromHsv(static_cast<int>(hash % 360), 125, 172);
  if (color.hsvHue() > 42 && color.hsvHue() < 68) {
    color = QColor::fromHsv(202, 126, 176);
  }
  return color;
}

QString panText(const float pan) {
  if (std::abs(pan) <= 0.01f) {
    return QStringLiteral("C");
  }
  return QStringLiteral("%1%2")
      .arg(static_cast<int>(std::lround(std::abs(pan) * 100.0f)))
      .arg(pan < 0.0f ? QStringLiteral("L") : QStringLiteral("R"));
}

QPointF radialPoint(const QPointF &center, const qreal radius,
                    const qreal degrees) {
  constexpr qreal pi = 3.14159265358979323846;
  const qreal radians = degrees * pi / 180.0;
  return QPointF(center.x() + std::cos(radians) * radius,
                 center.y() + std::sin(radians) * radius);
}

std::vector<AudioFxChipInfo> effectChipInfosForLayer(const AudioMixerChannelStrip::LayerID &layerId) {
  std::vector<AudioFxChipInfo> chips;
  auto *projectService = ArtifactProjectService::instance();
  if (!projectService || layerId.isNil()) {
    return chips;
  }

  auto composition = projectService->currentComposition().lock();
  if (!composition) {
    return chips;
  }

  auto layer = composition->layerById(layerId);
  if (!layer) {
    return chips;
  }

  for (const auto &effect : layer->getEffects()) {
    if (!effect) {
      continue;
    }
    chips.push_back(AudioFxChipInfo{
        effect->effectID().toQString(),
        effect->displayName().toQString().trimmed().isEmpty()
            ? effect->effectID().toQString()
            : effect->displayName().toQString(),
        effect->isEnabled(),
    });
  }
  return chips;
}

class AudioLevelMeterWidget final : public QWidget {
public:
  explicit AudioLevelMeterWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(26, 224);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  void setLevels(const float left, const float right) {
    setLevels(left, right, left, right);
  }

  void setLevels(const float left, const float right,
                 const float peakLeft, const float peakRight) {
    const float clampedLeft = std::clamp(left, -60.0f, 6.02f);
    const float clampedRight = std::clamp(right, -60.0f, 6.02f);
    const float clampedPeakLeft = std::clamp(peakLeft, -60.0f, 6.02f);
    const float clampedPeakRight = std::clamp(peakRight, -60.0f, 6.02f);
    if (qFuzzyCompare(left_ + 61.0f, clampedLeft + 61.0f) &&
        qFuzzyCompare(right_ + 61.0f, clampedRight + 61.0f) &&
        qFuzzyCompare(peakLeft_ + 61.0f, clampedPeakLeft + 61.0f) &&
        qFuzzyCompare(peakRight_ + 61.0f, clampedPeakRight + 61.0f)) {
      return;
    }
    left_ = clampedLeft;
    right_ = clampedRight;
    peakLeft_ = std::max(clampedPeakLeft, left_);
    peakRight_ = std::max(clampedPeakRight, right_);
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QColor(51, 60, 69));
    painter.setBrush(QColor(15, 18, 22));
    painter.drawRoundedRect(bounds, 4.0, 4.0);

    const qreal outerMargin = 1.0;
    const qreal gap = 1.0;
    const qreal laneWidth = (width() - (outerMargin * 2.0) - gap) / 2.0;
    drawLane(&painter,
             QRectF(outerMargin, outerMargin, laneWidth,
                    height() - (outerMargin * 2.0)),
             left_, peakLeft_);
    drawLane(&painter,
             QRectF(outerMargin + laneWidth + gap, outerMargin, laneWidth,
                    height() - (outerMargin * 2.0)),
             right_, peakRight_);
  }

private:
  static float meterFraction(const float db) {
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
  }

  static void drawLane(QPainter *painter, const QRectF &rect, const float db,
                       const float peakDb) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(22, 28, 34));
    painter->drawRoundedRect(rect, 3.0, 3.0);

    QRectF fillRect = rect;
    fillRect.setTop(rect.bottom() - rect.height() * meterFraction(db));
    if (fillRect.height() <= 0.0f) {
      return;
    }

    QLinearGradient gradient(fillRect.bottomLeft(), fillRect.topLeft());
    gradient.setColorAt(0.0, QColor(53, 176, 111));
    gradient.setColorAt(0.68, QColor(218, 187, 74));
    gradient.setColorAt(1.0, QColor(212, 89, 89));
    painter->setBrush(gradient);
    painter->drawRoundedRect(fillRect, 3.0, 3.0);

    const QColor peakColor =
        db > -1.0f ? QColor(235, 92, 84) : QColor(234, 201, 82);
    painter->setPen(QPen(peakColor, 1.2));
    painter->drawLine(QPointF(rect.left() + 2.0, fillRect.top()),
                     QPointF(rect.right() - 2.0, fillRect.top()));

    const qreal peakY = rect.bottom() - rect.height() * meterFraction(peakDb);
    painter->setPen(QPen(QColor(246, 241, 223), 1.0));
    painter->drawLine(QPointF(rect.left() + 1.0, peakY),
                     QPointF(rect.right() - 1.0, peakY));
  }

  float left_ = -60.0f;
  float right_ = -60.0f;
  float peakLeft_ = -60.0f;
  float peakRight_ = -60.0f;
};

class AudioDbScaleWidget final : public QWidget {
public:
  explicit AudioDbScaleWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(24, 224);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(70, 76, 82), 1.0));
    const QRectF rail(width() - 8.0, 8.0, 2.0, height() - 16.0);
    painter.setBrush(QColor(42, 46, 50));
    painter.drawRoundedRect(rail, 1.0, 1.0);

    const struct Tick {
      int db;
      bool major;
    } ticks[] = {
        {0, true}, {-6, false}, {-12, true}, {-18, false}, {-24, true},
        {-30, false}, {-36, true}, {-42, false}, {-48, true}, {-54, false},
        {-60, true},
    };

    QFont scaleFont = font();
    scaleFont.setPointSize(std::max(6, scaleFont.pointSize() - 3));
    scaleFont.setBold(true);
    painter.setFont(scaleFont);
    painter.setPen(QColor(177, 183, 188));

    for (const Tick &tick : ticks) {
      const qreal fraction = std::clamp((60.0 + tick.db) / 60.0, 0.0, 1.0);
      const qreal y = 8.0 + (height() - 16.0) * (1.0 - fraction);
      const qreal tickLength = tick.major ? 7.0 : 4.0;
      painter.drawLine(QPointF(width() - 11.0 - tickLength, y),
                       QPointF(width() - 11.0, y));
      if (tick.major) {
        const QString label =
            tick.db == 0 ? QStringLiteral("0") : QString::number(tick.db);
        painter.drawText(QRect(0, static_cast<int>(y - 6.0), width() - 12, 12),
                         Qt::AlignRight | Qt::AlignVCenter, label);
      }
    }
  }
};

class AudioStatusBadge final : public QLabel {
public:
  explicit AudioStatusBadge(QWidget *parent = nullptr) : QLabel(parent) {
    setAttribute(Qt::WA_TranslucentBackground, true);
  }

  void setBadgeColor(const QColor &color) {
    if (color_ != color) {
      color_ = color;
      update();
    }
  }

  QColor badgeColor() const { return color_; }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (color_.isValid() && color_ != Qt::transparent) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(color_);
      painter.drawEllipse(rect());
    }

    if (!text().isEmpty()) {
      QFont font = this->font();
      font.setBold(true);
      painter.setFont(font);
      painter.setPen(QColor(0, 0, 0));
      painter.drawText(rect(), Qt::AlignCenter, text());
    }
  }

private:
  QColor color_ = Qt::transparent;
};

class AudioBusSlotLabel final : public QLabel {
public:
  explicit AudioBusSlotLabel(const QString &text, QWidget *parent = nullptr)
      : QLabel(text, parent) {
    setFixedHeight(22);
    setMinimumWidth(48);
    setAlignment(Qt::AlignCenter);
  }

  void setSlotColor(const QColor &color) {
    if (slotColor_ == color) {
      return;
    }
    slotColor_ = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QColor fill = slotColor_.isValid() ? slotColor_ : QColor(72, 77, 82);
    QColor border = fill.lighter(118);
    fill.setAlpha(185);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(bounds, 4.0, 4.0);

    QFont font = this->font();
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(235, 238, 240));
    painter.drawText(rect().adjusted(4, 0, -4, 0), Qt::AlignCenter,
                     painter.fontMetrics().elidedText(
                         text(), Qt::ElideRight, width() - 8));
  }

private:
  QColor slotColor_;
};

class AudioFxRackWidget final : public QWidget {
public:
  explicit AudioFxRackWidget(ArtifactCompositionAudioMixerWidget *owner = nullptr,
                             QWidget *parent = nullptr)
      : QWidget(parent), owner_(owner) {
    setFixedHeight(56);
    setMinimumWidth(112);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QStringLiteral("FX rack"));
    setAccessibleDescription(
        QStringLiteral("Effect chips for the current layer. Click + to add, click a chip to edit, or click the count badge for bulk actions."));
    setToolTip(QStringLiteral("Left click + to add an effect. Right click a chip to edit it. Click +N to browse all effects or toggle them in bulk."));
  }

  void setEffects(const std::vector<AudioFxChipInfo> &effects) {
    if (effects_ == effects) {
      return;
    }
    effects_ = effects;
    setHoverTarget(kNoTarget);
    updateHoverTooltip();
    update();
  }

  void setLayerContext(const AudioMixerChannelStrip::LayerID &layerId,
                       const QString &layerName) {
    layerId_ = layerId;
    layerName_ = layerName.trimmed();
    setHoverTarget(kNoTarget);
    updateHoverTooltip();
  }

  bool hasEffects() const {
    return !effects_.empty();
  }

  void openAddEffectMenu(const QPoint &globalPos) {
    showAddEffectMenu(globalPos);
  }

  void openAllEffectsMenu(const QPoint &globalPos) {
    showAllEffectsMenu(globalPos);
  }

  void setAllEffectsEnabled(const bool enabled) {
    auto *effectService = ArtifactEffectService::instance();
    if (!effectService || layerId_.isNil() || effects_.empty()) {
      return;
    }

    bool changed = false;
    for (const auto &chip : effects_) {
      const auto result = effectService->setEffectEnabled(layerId_, chip.id, enabled);
      changed = changed || result.success;
    }
    if (changed) {
      requestRefresh();
    }
  }

  void clearAllEffects() {
    auto *effectService = ArtifactEffectService::instance();
    if (!effectService || layerId_.isNil() || effects_.empty()) {
      return;
    }

    const QString layerName = layerDisplayName();
    const int effectCount = static_cast<int>(effects_.size());
    const auto result = QMessageBox::question(
        this,
        QStringLiteral("Remove All Effects"),
        QStringLiteral("Remove all %1 effects from %2?")
            .arg(effectCount)
            .arg(layerName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (result != QMessageBox::Yes) {
      return;
    }

    bool changed = false;
    const auto chips = effects_;
    for (const auto &chip : chips) {
      const auto result = effectService->removeEffectFromLayer(layerId_, chip.id);
      changed = changed || result.success;
    }
    if (changed) {
      requestRefresh();
    }
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(53, 57, 62), 1.0));
    painter.setBrush(QColor(29, 32, 35));
    painter.drawRoundedRect(bounds, 4.0, 4.0);

    painter.setPen(QColor(150, 160, 170));
    QFont titleFont = font();
    titleFont.setPointSize(std::max(7, titleFont.pointSize() - 3));
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(QRect(6, 2, width() - 12, 12), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("FX"));

    if (!effects_.empty()) {
      const QString countText = effectCountText();
      const QRect countRect = countBadgeRect(countText);
      const bool hoverCount = hoverTarget_ == kCountTarget;
      painter.setPen(QPen(QColor(74, 79, 85), 1.0));
      painter.setBrush(hoverCount ? QColor(56, 60, 65) : QColor(47, 51, 55));
      painter.drawRoundedRect(countRect, 3.0, 3.0);
      QFont countFont = painter.font();
      countFont.setPointSize(std::max(5, countFont.pointSize() - 5));
      countFont.setBold(true);
      painter.setFont(countFont);
      painter.setPen(hoverCount ? QColor(247, 248, 249) : QColor(232, 236, 239));
      painter.drawText(countRect, Qt::AlignCenter, countText);
    }

    const QRect addRect = addButtonRect();
    const bool hoverAdd = hoverTarget_ == kAddTarget;
    painter.setPen(QPen(hoverAdd ? QColor(180, 188, 194) : QColor(96, 103, 110),
                        hoverAdd ? 1.2 : 1.0));
    painter.setBrush(hoverAdd ? QColor(56, 60, 65) : QColor(40, 44, 48));
    painter.drawRoundedRect(addRect, 3.0, 3.0);
    painter.setPen(hoverAdd ? QColor(246, 247, 248) : QColor(220, 227, 232));
    painter.drawText(addRect, Qt::AlignCenter, QStringLiteral("+"));

    const int chipCount = visibleChipCount();
    const int chipY = 16;
    const int chipH = 16;
    const int chipGap = 4;
    const int chipW = std::max(36, (width() - 12 - (chipCount > 0 ? (chipCount - 1) * chipGap : 0)) /
                                      std::max(1, chipCount));
    for (int i = 0; i < chipCount; ++i) {
      const QRect chipRect(6 + i * (chipW + chipGap), chipY, chipW, chipH);
      const auto &chip = effects_.at(i);
      const QString &displayName = chip.displayName;
      const QString typeHint = typeHintForEffectId(chip.id);
      QColor fill = chipColorForName(displayName);
      if (!chip.enabled) {
        fill = QColor(78, 82, 86);
        fill.setAlpha(190);
      }
      const bool hovered = hoverTarget_ == i;
      painter.setPen(QPen(hovered ? fill.lighter(150) : fill.lighter(120),
                          hovered ? 1.4 : 1.0));
      painter.setBrush(fill);
      painter.drawRoundedRect(chipRect, 3.0, 3.0);
      painter.setPen(hovered ? QColor(255, 255, 255)
                             : (chip.enabled ? QColor(20, 22, 24)
                                             : QColor(226, 228, 230)));
      painter.drawText(chipRect.adjusted(5, 0, -5, 0), Qt::AlignCenter,
                       painter.fontMetrics().elidedText(displayName, Qt::ElideRight,
                                                          chipRect.width() - 10));
      if (!typeHint.isEmpty()) {
        QRect hintRect = chipRect.adjusted(chipRect.width() - 16, 2, -2, -2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 70));
        painter.drawRoundedRect(hintRect, 2.0, 2.0);
        painter.setPen(QColor(250, 250, 250));
        QFont hintFont = painter.font();
        hintFont.setPointSize(std::max(5, hintFont.pointSize() - 5));
        hintFont.setBold(true);
        const QFont chipFont = painter.font();
        painter.setFont(hintFont);
        painter.drawText(hintRect, Qt::AlignCenter, typeHint);
        painter.setFont(chipFont);
      }
    }

    if (effects_.empty()) {
      painter.setPen(QColor(126, 133, 140));
      painter.drawText(QRect(6, 18, width() - 12, 20), Qt::AlignCenter,
                       QStringLiteral("None"));
    } else if (effects_.size() > chipCount) {
      const QString extra = QStringLiteral("+%1").arg(static_cast<int>(effects_.size()) - chipCount);
      QRect extraRect = overflowBadgeRect();
      const bool hoverOverflow = hoverTarget_ == kOverflowTarget;
      painter.setPen(QPen(hoverOverflow ? QColor(140, 148, 156)
                                        : QColor(78, 83, 89),
                          hoverOverflow ? 1.2 : 1.0));
      painter.setBrush(hoverOverflow ? QColor(69, 74, 78) : QColor(55, 60, 64));
      painter.drawRoundedRect(extraRect, 3.0, 3.0);
      painter.setPen(hoverOverflow ? QColor(247, 248, 249)
                                   : QColor(220, 224, 228));
      painter.drawText(extraRect, Qt::AlignCenter, extra);
    }
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    updateHoverTooltip();
    update();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    updateHoverTarget(event ? event->position().toPoint() : QPoint(-1, -1));
    QWidget::mouseMoveEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    setHoverTarget(kNoTarget);
    QWidget::leaveEvent(event);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (!event) {
      QWidget::keyPressEvent(event);
      return;
    }

    if (event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Space) {
      const QPoint globalPos = mapToGlobal(rect().center());
      if (hoverTarget_ >= 0 && hoverTarget_ < static_cast<int>(effects_.size())) {
        showEffectMenu(hoverTarget_, globalPos);
      } else if (hoverTarget_ == kCountTarget || hoverTarget_ == kOverflowTarget) {
        showAllEffectsMenu(globalPos);
      } else if (hoverTarget_ == kAddTarget || effects_.empty()) {
        showAddEffectMenu(globalPos);
      } else {
        showAllEffectsMenu(globalPos);
      }
      event->accept();
      return;
    }

    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        hoverTarget_ >= 0 && hoverTarget_ < static_cast<int>(effects_.size())) {
      showEffectMenu(hoverTarget_, mapToGlobal(rect().center()));
      event->accept();
      return;
    }

    QWidget::keyPressEvent(event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event) {
      setFocus(Qt::MouseFocusReason);
    }
    if (!event || event->button() != Qt::LeftButton) {
      if (event && event->button() == Qt::RightButton) {
        if (!effects_.empty() &&
            countBadgeRect(effectCountText()).contains(event->position().toPoint())) {
          showAllEffectsMenu(mapToGlobal(event->position().toPoint()));
          return;
        }
        if (effects_.size() > visibleChipCount() &&
            overflowBadgeRect().contains(event->position().toPoint())) {
          showAllEffectsMenu(mapToGlobal(event->position().toPoint()));
          return;
        }
        const int chipIndex = chipIndexAt(event->position().toPoint());
        if (chipIndex >= 0) {
          showEffectMenu(chipIndex, mapToGlobal(event->position().toPoint()));
          return;
        }
      }
      QWidget::mousePressEvent(event);
      return;
    }

    const QRect addRect = addButtonRect();
    const QRect countRect = countBadgeRect(effectCountText());
    if (!effects_.empty() && countRect.contains(event->position().toPoint())) {
      showAllEffectsMenu(mapToGlobal(event->position().toPoint()));
      return;
    }
    if (effects_.size() > visibleChipCount() && overflowBadgeRect().contains(event->position().toPoint())) {
      showAllEffectsMenu(mapToGlobal(event->position().toPoint()));
      return;
    }
    const int chipIndex = chipIndexAt(event->position().toPoint());
    if (chipIndex >= 0) {
      showEffectMenu(chipIndex, mapToGlobal(event->position().toPoint()));
      return;
    }

    if (!addRect.contains(event->position().toPoint())) {
      QWidget::mousePressEvent(event);
      return;
    }

    showAddEffectMenu(mapToGlobal(addRect.bottomLeft()));
  }

private:
  void updateTooltip() {
    if (effects_.empty()) {
      setToolTip(QStringLiteral("Click + to add an effect."));
      return;
    }

    QStringList names;
    const int previewCount = visibleChipCount();
    for (int i = 0; i < previewCount; ++i) {
      const auto &chip = effects_.at(i);
      names.append(chip.displayName + (chip.enabled ? QString() : QStringLiteral(" [off]")));
    }
    if (effects_.size() > previewCount) {
      names.append(QStringLiteral("+%1 more").arg(static_cast<int>(effects_.size()) - previewCount));
    }
    const QString summary = names.join(QStringLiteral(", "));
    if (effects_.size() > previewCount) {
      setToolTip(QStringLiteral("%1. %2 total, %3 enabled. Left click + to add an effect. Right click a chip to edit it. Click +N to browse all effects or toggle them in bulk.")
                     .arg(summary)
                     .arg(effects_.size())
                     .arg(enabledEffectCount()));
      return;
    }

    setToolTip(QStringLiteral("%1. %2 total, %3 enabled. Right click a chip to edit it. Click + to add another effect.")
                   .arg(summary)
                   .arg(effects_.size())
                   .arg(enabledEffectCount()));
  }

  void updateHoverTooltip() {
    const QString layerName = layerDisplayName();
    if (hoverTarget_ == kAddTarget) {
      setToolTip(QStringLiteral("Add an effect to %1.").arg(layerName));
      return;
    }
    if (hoverTarget_ == kCountTarget) {
      setToolTip(QStringLiteral("Browse all effects on %1 or toggle them in bulk. %2 total, %3 enabled.")
                     .arg(layerName)
                     .arg(effects_.size())
                     .arg(enabledEffectCount()));
      return;
    }
    if (hoverTarget_ == kOverflowTarget) {
      setToolTip(QStringLiteral("Browse all effects on %1 or toggle them in bulk. %2 total, %3 enabled.")
                     .arg(layerName)
                     .arg(effects_.size())
                     .arg(enabledEffectCount()));
      return;
    }
    if (hoverTarget_ >= 0 && hoverTarget_ < static_cast<int>(effects_.size())) {
      const auto &chip = effects_.at(hoverTarget_);
      const QString status = chip.enabled ? QStringLiteral("Enabled")
                                         : QStringLiteral("Disabled");
      setToolTip(QStringLiteral("%1 on %2. Status: %3. Right click to remove, duplicate, toggle, or reorder.")
                     .arg(chip.displayName, layerName, status));
      return;
    }
    updateTooltip();
  }

  void showAddEffectMenu(const QPoint &globalPos) {
    auto *effectService = ArtifactEffectService::instance();
    if (!effectService || layerId_.isNil()) {
      return;
    }

    const auto available = effectService->availableEffects();
    if (available.empty()) {
      QMenu menu(this);
      menu.addAction(QStringLiteral("No effects available"))->setEnabled(false);
      menu.exec(globalPos);
      return;
    }

    auto sortedAvailable = available;
    std::sort(sortedAvailable.begin(), sortedAvailable.end(),
              [](const auto &lhs, const auto &rhs) {
                return lhs.displayName.toCaseFolded() < rhs.displayName.toCaseFolded();
              });

    QMenu menu(this);
    std::map<QString, std::vector<EffectInfo>> grouped;
    for (const auto &effect : sortedAvailable) {
      grouped[effectCategoryForId(effect.id.toString())].push_back(effect);
    }

    const QStringList categoryOrder = {
        QStringLiteral("Color Correction"),
        QStringLiteral("Basic Color"),
        QStringLiteral("Image"),
        QStringLiteral("Distort"),
        QStringLiteral("Material"),
        QStringLiteral("OFX"),
        QStringLiteral("Other"),
    };

    for (const auto &category : categoryOrder) {
      const auto it = grouped.find(category);
      if (it == grouped.end()) {
        continue;
      }
      QMenu *submenu = menu.addMenu(category);
      for (const auto &effect : it->second) {
        QAction *action = submenu->addAction(effect.displayName);
        action->setData(effect.id.toQString());
      }
    }
    for (const auto &[category, effects] : grouped) {
      if (categoryOrder.contains(category)) {
        continue;
      }
      QMenu *submenu = menu.addMenu(category);
      for (const auto &effect : effects) {
        QAction *action = submenu->addAction(effect.displayName);
        action->setData(effect.id.toQString());
      }
    }

    if (QAction *selected = menu.exec(globalPos)) {
      const QString effectId = selected->data().toString().trimmed();
      if (effectId.isEmpty()) {
        return;
      }
      const auto result =
          effectService->addEffectToLayer(layerId_, EffectID(effectId));
      if (result.success) {
        requestRefresh();
      }
    }
  }

  void showEffectMenu(const int index, const QPoint &globalPos) {
    auto *effectService = ArtifactEffectService::instance();
    if (!effectService || layerId_.isNil() || index < 0 || index >= static_cast<int>(effects_.size())) {
      return;
    }

    QMenu menu(this);
    const auto &chip = effects_.at(index);
    QAction *removeAction = menu.addAction(QStringLiteral("Remove"));
    QAction *duplicateAction = menu.addAction(QStringLiteral("Duplicate"));
    menu.addSeparator();
    QAction *toggleAction = menu.addAction(chip.enabled ? QStringLiteral("Disable")
                                                        : QStringLiteral("Enable"));
    QAction *moveUpAction = menu.addAction(QStringLiteral("Move Up"));
    QAction *moveDownAction = menu.addAction(QStringLiteral("Move Down"));
    moveUpAction->setEnabled(index > 0);
    moveDownAction->setEnabled(index + 1 < static_cast<int>(effects_.size()));

    QAction *selected = menu.exec(globalPos);
    if (!selected) {
      return;
    }

    const QString effectId = chip.id;
    if (selected == removeAction) {
      const auto result = effectService->removeEffectFromLayer(layerId_, effectId);
      if (result.success) {
        requestRefresh();
      }
      return;
    }
    if (selected == duplicateAction) {
      const auto result = effectService->duplicateEffect(layerId_, effectId);
      if (result.success) {
        requestRefresh();
      }
      return;
    }
    if (selected == moveUpAction) {
      const auto result = effectService->moveEffect(layerId_, effectId, -1);
      if (result.success) {
        requestRefresh();
      }
      return;
    }
    if (selected == moveDownAction) {
      const auto result = effectService->moveEffect(layerId_, effectId, +1);
      if (result.success) {
        requestRefresh();
      }
      return;
    }
    if (selected == toggleAction) {
      const auto result =
          effectService->setEffectEnabled(layerId_, effectId, !chip.enabled);
      if (result.success) {
        requestRefresh();
      }
      return;
    }
  }

  void showAllEffectsMenu(const QPoint &globalPos) {
    auto *effectService = ArtifactEffectService::instance();
    if (!effectService || layerId_.isNil() || effects_.empty()) {
      return;
    }

    QMenu menu(this);
    const QString title = QStringLiteral("Effects (%1/%2 enabled)")
                              .arg(enabledEffectCount())
                              .arg(static_cast<int>(effects_.size()));
    menu.addSection(title);
    QAction *enableAllAction = menu.addAction(QStringLiteral("Enable All"));
    QAction *disableAllAction = menu.addAction(QStringLiteral("Disable All"));
    QAction *removeAllAction = menu.addAction(QStringLiteral("Remove All"));
    menu.addSeparator();
    for (const auto &chip : effects_) {
      const QString suffix = chip.enabled ? QString() : QStringLiteral(" [off]");
      QAction *action = menu.addAction(chip.displayName + suffix);
      action->setData(chip.id);
    }

    if (QAction *selected = menu.exec(globalPos)) {
      if (selected == enableAllAction || selected == disableAllAction) {
        const bool enabled = selected == enableAllAction;
        bool changed = false;
        for (const auto &chip : effects_) {
          const auto result = effectService->setEffectEnabled(layerId_, chip.id, enabled);
          changed = changed || result.success;
        }
        if (changed) {
          requestRefresh();
        }
        return;
      }
      if (selected == removeAllAction) {
        clearAllEffects();
        return;
      }
      const QString effectId = selected->data().toString().trimmed();
      if (effectId.isEmpty()) {
        return;
      }
      for (int i = 0; i < static_cast<int>(effects_.size()); ++i) {
        if (effects_.at(i).id == effectId) {
          showEffectMenu(i, globalPos);
          break;
        }
      }
    }
  }

  int chipIndexAt(const QPoint &pos) const {
    const int count = visibleChipCount();
    for (int i = 0; i < count; ++i) {
      if (chipRectAt(i, count).contains(pos)) {
        return i;
      }
    }
    return -1;
  }

  QRect chipRectAt(const int index, const int visibleChipCount) const {
    const int chipY = 16;
    const int chipH = 16;
    const int chipGap = 4;
    const int chipW = std::max(36, (width() - 12 - (visibleChipCount > 0 ? (visibleChipCount - 1) * chipGap : 0)) /
                                      std::max(1, visibleChipCount));
    return QRect(6 + index * (chipW + chipGap), chipY, chipW, chipH);
  }

  QRect overflowBadgeRect() const {
    return QRect(width() - 28, 34, 22, 14);
  }

  QString effectCountText() const {
    return QStringLiteral("%1/%2")
        .arg(enabledEffectCount())
        .arg(static_cast<int>(effects_.size()));
  }

  int enabledEffectCount() const {
    return static_cast<int>(std::count_if(
        effects_.begin(), effects_.end(),
        [](const AudioFxChipInfo &chip) { return chip.enabled; }));
  }

  QRect countBadgeRect(const QString &countText) const {
    QFont countFont = font();
    countFont.setPointSize(std::max(5, countFont.pointSize() - 5));
    countFont.setBold(true);
    const QFontMetrics metrics(countFont);
    const int textWidth = metrics.horizontalAdvance(countText) + 10;
    const QRect addRect = addButtonRect();
    return QRect(std::max(6, addRect.left() - 4 - textWidth), 4, textWidth, 14);
  }

  QRect addButtonRect() const {
    return QRect(width() - 22, 4, 14, 14);
  }

  int visibleChipCount() const {
    if (effects_.empty()) {
      return 0;
    }
    return std::min(static_cast<int>(effects_.size()), width() >= 128 ? 3 : 2);
  }

  void updateHoverTarget(const QPoint &pos) {
    int target = kNoTarget;
    if (!effects_.empty() &&
        countBadgeRect(effectCountText()).contains(pos)) {
      target = kCountTarget;
    } else if (effects_.size() > visibleChipCount() &&
        overflowBadgeRect().contains(pos)) {
      target = kOverflowTarget;
    } else if (const int chipIndex = chipIndexAt(pos); chipIndex >= 0) {
      target = chipIndex;
    } else if (addButtonRect().contains(pos)) {
      target = kAddTarget;
    }
    setHoverTarget(target);
  }

  void setHoverTarget(const int target) {
    if (hoverTarget_ == target) {
      return;
    }
    hoverTarget_ = target;
    updateHoverTooltip();
    update();
  }

  QString layerDisplayName() const {
    if (!layerName_.trimmed().isEmpty()) {
      return layerName_;
    }
    if (layerId_.isNil()) {
      return QStringLiteral("this layer");
    }
    return QStringLiteral("this layer");
  }

  void requestRefresh() {
    queueMixerRefresh(owner_);
  }

  static QColor chipColorForName(const QString &name) {
    const uint hash = qHash(name);
    QColor color = QColor::fromHsv(static_cast<int>(hash % 360), 116, 156);
    if (color.hsvHue() > 35 && color.hsvHue() < 72) {
      color = QColor::fromHsv(198, 106, 158);
    }
    return color;
  }

  static QString effectCategoryForId(const QString &effectId) {
    if (effectId.startsWith(QStringLiteral("ofx."))) {
      return QStringLiteral("OFX");
    }
    if (effectId.startsWith(QStringLiteral("effect.colorcorrection."))) {
      return QStringLiteral("Color Correction");
    }
    if (effectId == QStringLiteral("brightness") ||
        effectId == QStringLiteral("hue_saturation") ||
        effectId == QStringLiteral("exposure")) {
      return QStringLiteral("Basic Color");
    }
    if (effectId == QStringLiteral("chroma_key") ||
        effectId == QStringLiteral("drop_shadow") ||
        effectId == QStringLiteral("glow") ||
        effectId == QStringLiteral("edge_bloom") ||
        effectId == QStringLiteral("chromatic_glow") ||
        effectId == QStringLiteral("reactive_glow") ||
        effectId == QStringLiteral("liquid_glow") ||
        effectId == QStringLiteral("residual_glow") ||
        effectId == QStringLiteral("effect.blur.gaussian") ||
        effectId == QStringLiteral("blur") ||
        effectId == QStringLiteral("directional_glow")) {
      return QStringLiteral("Image");
    }
    if (effectId == QStringLiteral("lift_gamma_gain")) {
      return QStringLiteral("Basic Color");
    }
    if (effectId == QStringLiteral("pbr_material")) {
      return QStringLiteral("Material");
    }
    if (effectId == QStringLiteral("wave") ||
        effectId == QStringLiteral("spherize") ||
        effectId == QStringLiteral("lens_distortion") ||
        effectId == QStringLiteral("twist") ||
        effectId == QStringLiteral("bend")) {
      return QStringLiteral("Distort");
    }
    return QStringLiteral("Other");
  }

  static QString typeHintForEffectId(const QString &effectId) {
    if (effectId.startsWith(QStringLiteral("effect.colorcorrection."))) {
      return QStringLiteral("C");
    }
    if (effectId == QStringLiteral("effect.blur.gaussian")) {
      return QStringLiteral("G");
    }
    if (effectId == QStringLiteral("blur")) {
      return QStringLiteral("B");
    }
    if (effectId == QStringLiteral("edge_bloom")) {
      return QStringLiteral("E");
    }
    if (effectId == QStringLiteral("chromatic_glow")) {
      return QStringLiteral("C");
    }
    if (effectId == QStringLiteral("reactive_glow")) {
      return QStringLiteral("R");
    }
    if (effectId == QStringLiteral("liquid_glow")) {
      return QStringLiteral("L");
    }
    if (effectId == QStringLiteral("residual_glow")) {
      return QStringLiteral("A");
    }
    if (effectId.startsWith(QStringLiteral("ofx."))) {
      return QStringLiteral("O");
    }
    if (effectId == QStringLiteral("twist") ||
        effectId == QStringLiteral("bend") ||
        effectId == QStringLiteral("wave") ||
        effectId == QStringLiteral("spherize") ||
        effectId == QStringLiteral("lens_distortion")) {
      return QStringLiteral("D");
    }
    if (effectId == QStringLiteral("pbr_material")) {
      return QStringLiteral("M");
    }
    return QString();
  }

  std::vector<AudioFxChipInfo> effects_;
  AudioMixerChannelStrip::LayerID layerId_;
  QString layerName_;
  ArtifactCompositionAudioMixerWidget *owner_ = nullptr;
  int hoverTarget_ = kNoTarget;

  static constexpr int kNoTarget = -1;
  static constexpr int kAddTarget = -2;
  static constexpr int kOverflowTarget = -3;
  static constexpr int kCountTarget = -4;
};

class AudioPanKnobWidget final : public QWidget {
public:
  explicit AudioPanKnobWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(54, 54);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  void setPan(const float pan) {
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    if (qFuzzyCompare(pan_ + 2.0f, clamped + 2.0f)) {
      return;
    }
    pan_ = clamped;
    if (panChanged_) {
      panChanged_(pan_);
    }
    update();
  }

  void setPanFromStrip(const float pan) {
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    if (qFuzzyCompare(pan_ + 2.0f, clamped + 2.0f)) {
      return;
    }
    pan_ = clamped;
    update();
  }

  void setPanChangedCallback(std::function<void(float)> callback) {
    panChanged_ = std::move(callback);
  }

  void setLinked(const bool linked) {
    if (linked_ == linked) {
      return;
    }
    linked_ = linked;
    update();
  }

protected:
  bool event(QEvent *event) override {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      dragPan_ = true;
      setPanFromEvent(mouseEvent);
      return true;
    }
    if (event->type() == QEvent::MouseMove && dragPan_) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      setPanFromEvent(mouseEvent);
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && dragPan_) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      dragPan_ = false;
      setPanFromEvent(mouseEvent);
      return true;
    }
    return QWidget::event(event);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center(width() * 0.5, 24.0);
    const qreal radius = 15.0;

    painter.setPen(QPen(QColor(21, 23, 25), 2.0));
    painter.setBrush(QColor(54, 57, 60));
    painter.drawEllipse(center, radius, radius);

    QLinearGradient knobLight(center.x(), center.y() - radius,
                              center.x(), center.y() + radius);
    knobLight.setColorAt(0.0, QColor(92, 96, 99));
    knobLight.setColorAt(1.0, QColor(38, 40, 43));
    painter.setBrush(knobLight);
    painter.setPen(QPen(QColor(118, 122, 126), 1.0));
    painter.drawEllipse(center, radius - 2.0, radius - 2.0);

    painter.setPen(QPen(QColor(28, 30, 32), 2.0));
    painter.drawArc(QRectF(center.x() - 22.0, center.y() - 22.0, 44.0, 44.0),
                    218 * 16, -256 * 16);
    painter.setPen(QPen(QColor(214, 181, 80), 2.2));
    const qreal indicatorDegrees = -90.0 + static_cast<qreal>(pan_) * 112.0;
    painter.drawLine(center, radialPoint(center, radius - 3.0, indicatorDegrees));

    QFont labelFont = font();
    labelFont.setPointSize(std::max(7, labelFont.pointSize() - 1));
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor(189, 198, 205));
    painter.drawText(QRect(0, 39, width(), 14), Qt::AlignCenter, panText(pan_));

    if (linked_) {
      QRectF badgeRect(13.0, 2.0, 28.0, 11.0);
      painter.setPen(QPen(QColor(82, 91, 98), 1.0));
      painter.setBrush(QColor(35, 39, 43));
      painter.drawRoundedRect(badgeRect, 3.0, 3.0);
      painter.setPen(QColor(190, 198, 203));
      QFont badgeFont = font();
      badgeFont.setPointSize(std::max(6, badgeFont.pointSize() - 4));
      badgeFont.setBold(true);
      painter.setFont(badgeFont);
      painter.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("LINK"));
    }
  }

private:
  void setPanFromEvent(QMouseEvent *event) {
    const QPointF center(width() * 0.5, 24.0);
    const float radius = 15.0f;
    const float delta = static_cast<float>((event->pos().x() - center.x()) / radius);
    const float clamped = std::clamp(delta, -1.0f, 1.0f);
    setPan(clamped);
  }

  float pan_ = 0.0f;
  bool linked_ = true;
  bool dragPan_ = false;
  std::function<void(float)> panChanged_;
};

class AudioMixerToggleButton final : public QPushButton {
public:
  explicit AudioMixerToggleButton(const QString &text, QWidget *parent = nullptr)
      : QPushButton(text, parent) {
    setCheckable(true);
    setFixedSize(30, 24);
    setCursor(Qt::PointingHandCursor);
  }

  void setAccentColor(const QColor &color) {
    accentColor_ = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QColor fill = isChecked() ? accentColor_ : QColor(50, 54, 58);
    QColor border = isChecked() ? accentColor_.lighter(128) : QColor(84, 88, 94);
    QColor textColor = isChecked() ? QColor(18, 20, 22) : QColor(214, 220, 225);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(bounds, 4.0, 4.0);

    QFont f = font();
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(textColor);
    painter.drawText(rect(), Qt::AlignCenter, text());
  }

private:
  QColor accentColor_ = QColor(211, 170, 66);
};

class AudioFaderSlider final : public QSlider {
public:
  explicit AudioFaderSlider(QWidget *parent = nullptr)
      : QSlider(Qt::Vertical, parent) {
    setRange(0, 200);
    setSingleStep(1);
    setFixedSize(34, 224);
    setPageStep(10);
    setTracking(true);
  }

  void setAccentColor(const QColor &color) {
    accentColor_ = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rail(width() * 0.5 - 3.0, 8.0, 6.0, height() - 16.0);
    painter.setPen(QPen(QColor(22, 25, 28), 1.0));
    painter.setBrush(QColor(38, 43, 47));
    painter.drawRoundedRect(rail, 3.0, 3.0);

    painter.setPen(QPen(QColor(88, 94, 100), 1.0));
    for (int i = 0; i <= 10; ++i) {
      const qreal y = rail.bottom() - (rail.height() * i / 10.0);
      const qreal tick = (i % 5 == 0) ? 9.0 : 5.0;
      painter.drawLine(QPointF(rail.left() - tick, y),
                       QPointF(rail.left() - 2.0, y));
      painter.drawLine(QPointF(rail.right() + 2.0, y),
                       QPointF(rail.right() + tick, y));
    }

    const qreal normalized =
        (value() - minimum()) /
        static_cast<qreal>(std::max(1, maximum() - minimum()));
    const qreal handleY = rail.bottom() - rail.height() * normalized;
    const QRectF handle(width() * 0.5 - 13.0, handleY - 11.0, 26.0, 22.0);
    QLinearGradient handleGradient(handle.topLeft(), handle.bottomLeft());
    handleGradient.setColorAt(0.0, accentColor_.lighter(142));
    handleGradient.setColorAt(1.0, accentColor_.darker(122));
    painter.setPen(QPen(QColor(18, 20, 22), 1.0));
    painter.setBrush(handleGradient);
    painter.drawRoundedRect(handle, 3.0, 3.0);
    painter.setPen(QPen(QColor(255, 255, 255, 80), 1.0));
    painter.drawLine(QPointF(handle.left() + 4.0, handle.center().y()),
                     QPointF(handle.right() - 4.0, handle.center().y()));
  }

private:
  QColor accentColor_ = QColor(211, 170, 66);
};

class AudioStripSeparatorWidget final : public QWidget {
public:
  explicit AudioStripSeparatorWidget(QWidget *parent = nullptr)
      : QWidget(parent) {
    setFixedSize(12, 466);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal centerX = width() * 0.5;
    QLinearGradient line(QPointF(centerX, 0.0), QPointF(centerX, height()));
    line.setColorAt(0.0, QColor(0, 0, 0, 0));
    line.setColorAt(0.5, QColor(86, 93, 99));
    line.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setPen(QPen(QBrush(line), 2.0));
    painter.drawLine(QPointF(centerX, 7.0), QPointF(centerX, height() - 7.0));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(41, 46, 50));
    painter.drawEllipse(QPointF(centerX, 12.0), 1.7, 1.7);
    painter.drawEllipse(QPointF(centerX, height() - 12.0), 1.7, 1.7);
  }
};

class AudioMixerStripRow final : public QFrame {
public:
  explicit AudioMixerStripRow(AudioMixerChannelStrip *strip,
                              QWidget *parent = nullptr,
                              ArtifactCompositionAudioMixerWidget *ownerWidget = nullptr)
      : QFrame(parent), strip_(strip) {
    setObjectName(QStringLiteral("AudioMixerStripCard"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    setFixedWidth(140);
    setMinimumHeight(466);
    accentColor_ = mixerAccentForName(strip ? strip->layerName() : QString());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 34);
    layout->setSpacing(6);

    statusBadge_ = new AudioStatusBadge(this);
    statusBadge_->setFixedSize(22, 22);
    statusBadge_->setAlignment(Qt::AlignCenter);

    nameLabel_ = new QLabel(this);
    nameLabel_->setAlignment(Qt::AlignCenter);
    nameLabel_->setWordWrap(true);
    nameLabel_->setFixedHeight(34);
    QFont nameFont = nameLabel_->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() > 0 ? nameFont.pointSize() : 10);
    nameLabel_->setFont(nameFont);
    QPalette namePalette = nameLabel_->palette();
    namePalette.setColor(QPalette::WindowText, QColor(230, 230, 230));
    nameLabel_->setPalette(namePalette);

    meterWidget_ = new AudioLevelMeterWidget(this);

    fxRack_ = new AudioFxRackWidget(ownerWidget, this);
    fxRack_->setLayerContext(
        strip ? strip->layerId() : AudioMixerChannelStrip::LayerID(),
        strip ? strip->layerName() : QString());
    outputSlot_ = new AudioBusSlotLabel(QStringLiteral("Master"), this);
    panKnob_ = new AudioPanKnobWidget(this);

    volumeSlider_ = new AudioFaderSlider(this);
    volumeSlider_->setAccentColor(accentColor_);
    scaleWidget_ = new AudioDbScaleWidget(this);

    volumeValueLabel_ = new QLabel(this);
    volumeValueLabel_->setAlignment(Qt::AlignCenter);
    QFont valueFont = volumeValueLabel_->font();
    valueFont.setBold(true);
    volumeValueLabel_->setFont(valueFont);
    QPalette valuePalette = volumeValueLabel_->palette();
    valuePalette.setColor(QPalette::WindowText, QColor(143, 166, 191));
    volumeValueLabel_->setPalette(valuePalette);

    muteButton_ = new AudioMixerToggleButton(QStringLiteral("M"), this);
    muteButton_->setAccentColor(QColor(219, 78, 63));

    soloButton_ = new AudioMixerToggleButton(QStringLiteral("S"), this);
    soloButton_->setAccentColor(QColor(219, 180, 68));

    auto *faderLayout = new QHBoxLayout();
    faderLayout->setContentsMargins(2, 0, 2, 0);
    faderLayout->setSpacing(5);
    faderLayout->addStretch(1);
    faderLayout->addWidget(scaleWidget_, 0, Qt::AlignBottom);
    faderLayout->addWidget(meterWidget_, 0, Qt::AlignBottom);
    faderLayout->addWidget(volumeSlider_, 0, Qt::AlignBottom);
    faderLayout->addStretch(1);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    buttonLayout->addWidget(muteButton_, 1);
    buttonLayout->addWidget(soloButton_, 1);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(statusBadge_, 0);
    headerLayout->addWidget(nameLabel_, 1);
    layout->addLayout(headerLayout, 0);
    layout->addWidget(sectionLabel(QStringLiteral("FX")), 0);
    fxSummaryLabel_ = new QLabel(this);
    fxSummaryLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    fxSummaryLabel_->setFixedHeight(18);
    QFont fxSummaryFont = fxSummaryLabel_->font();
    fxSummaryFont.setPointSize(std::max(7, fxSummaryFont.pointSize() - 2));
    fxSummaryLabel_->setFont(fxSummaryFont);
    QPalette fxSummaryPalette = fxSummaryLabel_->palette();
    fxSummaryPalette.setColor(QPalette::WindowText, QColor(142, 154, 164));
    fxSummaryLabel_->setPalette(fxSummaryPalette);
    fxSummaryLabel_->setAutoFillBackground(true);
    fxSummaryLabel_->setFrameShape(QFrame::StyledPanel);
    fxSummaryLabel_->setFrameShadow(QFrame::Plain);
    fxSummaryLabel_->setLineWidth(1);
    fxSummaryLabel_->setMargin(2);
    fxSummaryLabel_->setFocusPolicy(Qt::StrongFocus);
    fxSummaryLabel_->setAccessibleName(QStringLiteral("FX summary"));
    fxSummaryLabel_->setAccessibleDescription(
        QStringLiteral("Shows the current effect count and opens the FX menu."));
    fxSummaryLabel_->setCursor(Qt::PointingHandCursor);
    fxSummaryLabel_->installEventFilter(this);
    setFxSummaryInteractiveState(false);
    layout->addWidget(fxSummaryLabel_, 0);
    layout->addWidget(fxRack_, 0);
    layout->addWidget(sectionLabel(QStringLiteral("Output")), 0);
    layout->addWidget(outputSlot_, 0);
    layout->addWidget(panKnob_, 0, Qt::AlignCenter);
    layout->addLayout(faderLayout, 1);
    layout->addWidget(volumeValueLabel_, 0);
    layout->addLayout(buttonLayout, 0);

    volumeCommitTimer_ = new QTimer(this);
    volumeCommitTimer_->setSingleShot(true);
    volumeCommitTimer_->setInterval(16);
    QObject::connect(volumeCommitTimer_, &QTimer::timeout, this,
                     [this]() { commitPendingVolume(); });

    QObject::connect(volumeSlider_, &QSlider::sliderPressed, this, [this]() {
      draggingVolume_ = true;
      pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
    });
    QObject::connect(volumeSlider_, &QSlider::sliderReleased, this, [this]() {
      draggingVolume_ = false;
      pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
      commitPendingVolume();
      syncFromStrip();
    });
    QObject::connect(volumeSlider_, &QSlider::valueChanged, this,
                     [this](const int value) {
                       pendingVolume_ = sliderValueToVolume(value);
                       updateVolumePresentation(pendingVolume_);
                       if (!draggingVolume_) {
                         commitPendingVolume();
                         return;
                       }
                       volumeCommitTimer_->start();
                     });
    QObject::connect(muteButton_, &QPushButton::toggled, this,
                     [this](const bool checked) {
                       if (strip_) {
                         strip_->setMuted(checked);
                       }
                     });
    QObject::connect(soloButton_, &QPushButton::toggled, this,
                     [this](const bool checked) {
                       if (strip_) {
                         strip_->setSolo(checked);
                       }
                     });
    if (panKnob_) {
      panKnob_->setPanChangedCallback([this](const float pan) {
        if (strip_) {
          strip_->setPan(pan);
        }
      });
    }

    QObject::connect(strip_, &AudioMixerChannelStrip::volumeChanged, this,
                     [this](const float) { syncFromStrip(); });
    QObject::connect(strip_, &AudioMixerChannelStrip::panChanged, this,
                     [this](const float) { syncFromStrip(); });
    QObject::connect(strip_, &AudioMixerChannelStrip::muteChanged, this,
                     [this](const bool) { syncFromStrip(); });
    QObject::connect(strip_, &AudioMixerChannelStrip::soloChanged, this,
                     [this](const bool) { syncFromStrip(); });
    QObject::connect(strip_, &AudioMixerChannelStrip::levelChanged, this,
                     [this](const float left, const float right) {
                       if (meterWidget_) {
                         meterWidget_->setLevels(left, right);
                       }
                     });

    syncFromStrip();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient bg(bounds.topLeft(), bounds.bottomLeft());
    bg.setColorAt(0.0, QColor(55, 60, 64));
    bg.setColorAt(0.58, QColor(39, 43, 47));
    bg.setColorAt(1.0, QColor(29, 32, 36));
    painter.setPen(QPen(QColor(19, 22, 25), 1.0));
    painter.setBrush(bg);
    painter.drawRoundedRect(bounds, 5.0, 5.0);

    QColor side = accentColor_;
    side.setAlpha(190);
    painter.setPen(Qt::NoPen);
    painter.setBrush(side);
    painter.drawRoundedRect(QRectF(1.0, 1.0, 4.0, bounds.height() - 2.0),
                            2.0, 2.0);

    QRectF footer(bounds.left() + 5.0, bounds.bottom() - 25.0,
                  bounds.width() - 10.0, 21.0);
    painter.setBrush(accentColor_.darker(104));
    painter.setPen(QPen(accentColor_.lighter(125), 1.0));
    painter.drawRoundedRect(footer, 3.0, 3.0);
    painter.setPen(QColor(20, 22, 24));
    QFont footerFont = font();
    footerFont.setBold(true);
    painter.setFont(footerFont);
    painter.drawText(footer.adjusted(6.0, 0.0, -6.0, 0.0),
                     Qt::AlignCenter,
                     painter.fontMetrics().elidedText(
                         strip_ ? strip_->layerName() : QStringLiteral("Bus"),
                         Qt::ElideRight, static_cast<int>(footer.width()) - 12));
  }

private:
  QLabel *sectionLabel(const QString &text) {
    auto *label = new QLabel(text, this);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setFixedHeight(14);
    QFont font = label->font();
    font.setPointSize(std::max(7, font.pointSize() - 2));
    font.setBold(true);
    label->setFont(font);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, QColor(152, 163, 172));
    label->setPalette(pal);
    return label;
  }

  void syncFromStrip() {
    if (!strip_) {
      return;
    }

    // 状態badge更新
    QString statusText;
    QColor statusColor = Qt::transparent;
    if (strip_->isMuted()) {
      statusText = "M";
      statusColor = QColor(220, 60, 60);
    } else if (strip_->isSolo()) {
      statusText = "S";
      statusColor = QColor(255, 255, 0);
    } else {
      statusText = "";
    }
    statusBadge_->setText(statusText);
    statusBadge_->setBadgeColor(statusColor);

    const QSignalBlocker volumeBlocker(volumeSlider_);
    const QSignalBlocker muteBlocker(muteButton_);
    const QSignalBlocker soloBlocker(soloButton_);

    nameLabel_->setText(strip_->layerName().trimmed().isEmpty()
                            ? QStringLiteral("Audio Layer")
                            : strip_->layerName());
    accentColor_ = mixerAccentForName(nameLabel_->text());
    const auto fxEffects = effectChipInfosForLayer(strip_->layerId());
    if (fxRack_) {
      fxRack_->setLayerContext(strip_->layerId(), strip_->layerName());
      fxRack_->setEffects(fxEffects);
    }
    if (fxSummaryLabel_) {
      fxSummaryLabel_->setAccessibleName(
          QStringLiteral("FX summary for %1").arg(strip_->layerName().trimmed().isEmpty()
                                                      ? QStringLiteral("Audio Layer")
                                                      : strip_->layerName()));
      if (fxEffects.empty()) {
        fxSummaryLabel_->setText(QStringLiteral("No effects"));
        fxSummaryLabel_->setToolTip(QStringLiteral("No effects on this layer. Click, right click, or press Enter/Space on this row to add one."));
        fxSummaryLabel_->setAccessibleDescription(
            QStringLiteral("No effects on this layer. Activate to add an effect."));
      } else {
        const int enabledCount = static_cast<int>(std::count_if(
            fxEffects.begin(), fxEffects.end(),
            [](const AudioFxChipInfo &chip) { return chip.enabled; }));
        QStringList previewNames;
        const int previewCount = std::min(static_cast<int>(fxEffects.size()), 2);
        for (int i = 0; i < previewCount; ++i) {
          previewNames.append(fxEffects.at(i).displayName);
        }
        if (fxEffects.size() > previewCount) {
          previewNames.append(QStringLiteral("+%1")
                                  .arg(static_cast<int>(fxEffects.size()) - previewCount));
        }
        fxSummaryLabel_->setText(QStringLiteral("%1 · %2 enabled")
                                     .arg(previewNames.join(QStringLiteral(", ")))
                                     .arg(enabledCount));
        QStringList fullNames;
        for (const auto &chip : fxEffects) {
          fullNames.append(chip.displayName + (chip.enabled ? QString() : QStringLiteral(" [off]")));
        }
        fxSummaryLabel_->setToolTip(QStringLiteral("%1 total, %2 enabled. Click this row to browse effects, or right click / press Enter / Space for add, bulk toggle, or remove all after confirmation. %3")
                                       .arg(static_cast<int>(fxEffects.size()))
                                       .arg(enabledCount)
                                       .arg(fullNames.join(QStringLiteral(", "))));
        fxSummaryLabel_->setAccessibleDescription(
            QStringLiteral("%1 effects on this layer, %2 enabled. Activate to browse or edit them, or use the menu for add, toggle, or remove all.")
                .arg(static_cast<int>(fxEffects.size()))
                .arg(enabledCount));
      }
    }
    if (outputSlot_) {
      outputSlot_->setText(QStringLiteral("Master"));
      outputSlot_->setSlotColor(QColor(75, 78, 82));
    }
    if (panKnob_) {
      panKnob_->setPanFromStrip(strip_->pan());
      panKnob_->setLinked(strip_->isStereoLinked());
    }
    if (volumeSlider_) {
      volumeSlider_->setAccentColor(accentColor_);
    }
    update();
    const float stripVolume = strip_->volume();
    if (!volumeSlider_->isSliderDown()) {
      volumeSlider_->setValue(volumeToPercent(stripVolume));
    }
    updateVolumePresentation(stripVolume);
    muteButton_->setChecked(strip_->isMuted());
    soloButton_->setChecked(strip_->isSolo());
    meterWidget_->setLevels(strip_->leftLevel(), strip_->rightLevel(),
                            strip_->peakLeft(), strip_->peakRight());
  }

  void updateVolumePresentation(const float volume) {
    volumeValueLabel_->setText(volumeToDisplayText(volume));
    if (draggingVolume_ && meterWidget_) {
      const float db = volumeToMeterDb(volume);
      meterWidget_->setLevels(db, db);
    }
  }

  void commitPendingVolume() {
    if (!strip_) {
      return;
    }
    strip_->setVolume(pendingVolume_);
  }

  void setFxSummaryInteractiveState(const bool active) {
    if (!fxSummaryLabel_) {
      return;
    }

    QPalette palette = fxSummaryLabel_->palette();
    palette.setColor(QPalette::Window,
                     active ? QColor(43, 47, 52) : QColor(35, 38, 42));
    palette.setColor(QPalette::WindowText,
                     active ? QColor(224, 231, 237) : QColor(142, 154, 164));
    fxSummaryLabel_->setPalette(palette);
    fxSummaryLabel_->setLineWidth(active ? 2 : 1);
    fxSummaryLabel_->setFrameShadow(active ? QFrame::Raised : QFrame::Plain);
    fxSummaryLabel_->update();
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == fxSummaryLabel_ && event) {
      if (event->type() == QEvent::Enter || event->type() == QEvent::FocusIn) {
        setFxSummaryInteractiveState(true);
      } else if (event->type() == QEvent::Leave || event->type() == QEvent::FocusOut) {
        setFxSummaryInteractiveState(false);
      }
      if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent && fxRack_ &&
            (keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter ||
             keyEvent->key() == Qt::Key_Space)) {
          const QPoint globalPos = fxSummaryLabel_->mapToGlobal(
              fxSummaryLabel_->rect().center());
          if (fxRack_->hasEffects()) {
            fxRack_->openAllEffectsMenu(globalPos);
          } else {
            fxRack_->openAddEffectMenu(globalPos);
          }
          return true;
        }
      }
      if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && fxRack_) {
          fxSummaryLabel_->setFocus(Qt::MouseFocusReason);
          const QPoint globalPos = fxSummaryLabel_->mapToGlobal(mouseEvent->pos());
          if (mouseEvent->button() == Qt::LeftButton) {
            if (fxRack_->hasEffects()) {
              fxRack_->openAllEffectsMenu(globalPos);
            } else {
              fxRack_->openAddEffectMenu(globalPos);
            }
            return true;
          }
          if (mouseEvent->button() == Qt::RightButton) {
            QMenu menu(fxSummaryLabel_);
            QAction *addEffectAction = menu.addAction(QStringLiteral("Add Effect"));
            QAction *enableAllAction = nullptr;
            QAction *disableAllAction = nullptr;
            QAction *removeAllAction = nullptr;
            QAction *browseEffectsAction = nullptr;
            if (fxRack_->hasEffects()) {
              menu.addSeparator();
              enableAllAction = menu.addAction(QStringLiteral("Enable All"));
              disableAllAction = menu.addAction(QStringLiteral("Disable All"));
              removeAllAction = menu.addAction(QStringLiteral("Remove All"));
              menu.addSeparator();
              browseEffectsAction = menu.addAction(QStringLiteral("Browse Effects"));
            }
            QAction *selected = menu.exec(globalPos);
            if (selected == addEffectAction) {
              fxRack_->openAddEffectMenu(globalPos);
              return true;
            }
            if (selected == enableAllAction) {
              fxRack_->setAllEffectsEnabled(true);
              return true;
            }
            if (selected == disableAllAction) {
              fxRack_->setAllEffectsEnabled(false);
              return true;
            }
            if (selected == removeAllAction) {
              fxRack_->clearAllEffects();
              return true;
            }
            if (selected == browseEffectsAction) {
              fxRack_->openAllEffectsMenu(globalPos);
              return true;
            }
            return true;
          }
        }
      }
      if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
          fxSummaryLabel_->setFocus(Qt::MouseFocusReason);
          return true;
        }
      }
    }
    return QObject::eventFilter(watched, event);
  }

  AudioMixerChannelStrip *strip_ = nullptr;
  AudioStatusBadge *statusBadge_ = nullptr;
  QLabel *nameLabel_ = nullptr;
  QLabel *fxSummaryLabel_ = nullptr;
  AudioFxRackWidget *fxRack_ = nullptr;
  AudioBusSlotLabel *outputSlot_ = nullptr;
  AudioPanKnobWidget *panKnob_ = nullptr;
  AudioDbScaleWidget *scaleWidget_ = nullptr;
  AudioLevelMeterWidget *meterWidget_ = nullptr;
  AudioFaderSlider *volumeSlider_ = nullptr;
  QLabel *volumeValueLabel_ = nullptr;
  AudioMixerToggleButton *muteButton_ = nullptr;
  AudioMixerToggleButton *soloButton_ = nullptr;
  QTimer *volumeCommitTimer_ = nullptr;
  QColor accentColor_;
  float pendingVolume_ = 1.0f;
  bool draggingVolume_ = false;
};

class AudioMixerMasterRow final : public QFrame {
public:
  explicit AudioMixerMasterRow(AudioMixerMasterBus *masterBus,
                               QWidget *parent = nullptr)
      : QFrame(parent), masterBus_(masterBus) {
    setObjectName(QStringLiteral("AudioMixerMasterCard"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    setFixedWidth(146);
    setMinimumHeight(466);
    accentColor_ = mixerAccentForName(QStringLiteral("Master"), true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 34);
    layout->setSpacing(6);

    statusBadge_ = new AudioStatusBadge(this);
    statusBadge_->setFixedSize(22, 22);
    statusBadge_->setAlignment(Qt::AlignCenter);

    nameLabel_ = new QLabel(QStringLiteral("Master"), this);
    nameLabel_->setAlignment(Qt::AlignCenter);
    nameLabel_->setWordWrap(true);
    nameLabel_->setFixedHeight(36);
    QFont nameFont = nameLabel_->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() > 0 ? nameFont.pointSize() : 10);
    nameLabel_->setFont(nameFont);
    QPalette namePalette = nameLabel_->palette();
    namePalette.setColor(QPalette::WindowText, QColor(240, 243, 246));
    nameLabel_->setPalette(namePalette);

    meterWidget_ = new AudioLevelMeterWidget(this);
    scaleWidget_ = new AudioDbScaleWidget(this);

    busSlot_ = new AudioBusSlotLabel(QStringLiteral("Stereo Out"), this);
    busSlot_->setSlotColor(accentColor_.darker(112));
    panKnob_ = new AudioPanKnobWidget(this);

    volumeSlider_ = new AudioFaderSlider(this);
    volumeSlider_->setAccentColor(accentColor_);

    volumeValueLabel_ = new QLabel(this);
    volumeValueLabel_->setAlignment(Qt::AlignCenter);
    QFont valueFont = volumeValueLabel_->font();
    valueFont.setBold(true);
    volumeValueLabel_->setFont(valueFont);
    QPalette valuePalette = volumeValueLabel_->palette();
    valuePalette.setColor(QPalette::WindowText, QColor(155, 192, 227));
    volumeValueLabel_->setPalette(valuePalette);

    muteButton_ = new AudioMixerToggleButton(QStringLiteral("M"), this);
    muteButton_->setAccentColor(QColor(219, 78, 63));

    auto *faderLayout = new QHBoxLayout();
    faderLayout->setContentsMargins(2, 0, 2, 0);
    faderLayout->setSpacing(8);
    faderLayout->addStretch(1);
    faderLayout->addWidget(scaleWidget_, 0, Qt::AlignBottom);
    faderLayout->addWidget(meterWidget_, 0, Qt::AlignBottom);
    faderLayout->addWidget(volumeSlider_, 0, Qt::AlignBottom);
    faderLayout->addStretch(1);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(statusBadge_, 0);
    headerLayout->addWidget(nameLabel_, 1);
    layout->addLayout(headerLayout, 0);
    layout->addWidget(sectionLabel(QStringLiteral("Bus")), 0);
    layout->addWidget(busSlot_, 0);
    layout->addWidget(sectionLabel(QStringLiteral("Pan")), 0);
    layout->addWidget(panKnob_, 0, Qt::AlignCenter);
    layout->addLayout(faderLayout, 1);
    layout->addWidget(volumeValueLabel_, 0);
    layout->addWidget(muteButton_, 0, Qt::AlignCenter);

    volumeCommitTimer_ = new QTimer(this);
    volumeCommitTimer_->setSingleShot(true);
    volumeCommitTimer_->setInterval(16);
    QObject::connect(volumeCommitTimer_, &QTimer::timeout, this,
                     [this]() { commitPendingVolume(); });

    QObject::connect(volumeSlider_, &QSlider::sliderPressed, this, [this]() {
      draggingVolume_ = true;
      pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
    });
    QObject::connect(volumeSlider_, &QSlider::sliderReleased, this, [this]() {
      draggingVolume_ = false;
      pendingVolume_ = sliderValueToVolume(volumeSlider_->value());
      commitPendingVolume();
      syncFromMaster();
    });
    QObject::connect(volumeSlider_, &QSlider::valueChanged, this,
                     [this](const int value) {
                       pendingVolume_ = sliderValueToVolume(value);
                       updateVolumePresentation(pendingVolume_);
                       if (!draggingVolume_) {
                         commitPendingVolume();
                         return;
                       }
                       volumeCommitTimer_->start();
                     });
    QObject::connect(muteButton_, &QPushButton::toggled, this,
                     [this](const bool checked) {
                       if (masterBus_) {
                         masterBus_->setMuted(checked);
                       }
                     });
    QObject::connect(masterBus_, &AudioMixerMasterBus::volumeChanged, this,
                     [this](const float) { syncFromMaster(); });
    QObject::connect(masterBus_, &AudioMixerMasterBus::muteChanged, this,
                     [this](const bool) { syncFromMaster(); });
    QObject::connect(masterBus_, &AudioMixerMasterBus::levelChanged, this,
                     [this](const float left, const float right) {
                       if (meterWidget_) {
                         meterWidget_->setLevels(left, right);
                       }
                     });

    syncFromMaster();
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient bg(bounds.topLeft(), bounds.bottomLeft());
    bg.setColorAt(0.0, QColor(61, 56, 43));
    bg.setColorAt(0.58, QColor(44, 43, 40));
    bg.setColorAt(1.0, QColor(31, 32, 33));
    painter.setPen(QPen(QColor(19, 22, 25), 1.0));
    painter.setBrush(bg);
    painter.drawRoundedRect(bounds, 5.0, 5.0);

    QColor side = accentColor_;
    side.setAlpha(210);
    painter.setPen(Qt::NoPen);
    painter.setBrush(side);
    painter.drawRoundedRect(QRectF(1.0, 1.0, 4.0, bounds.height() - 2.0),
                            2.0, 2.0);

    QRectF footer(bounds.left() + 5.0, bounds.bottom() - 25.0,
                  bounds.width() - 10.0, 21.0);
    painter.setBrush(accentColor_);
    painter.setPen(QPen(accentColor_.lighter(125), 1.0));
    painter.drawRoundedRect(footer, 3.0, 3.0);
    painter.setPen(QColor(20, 22, 24));
    QFont footerFont = font();
    footerFont.setBold(true);
    painter.setFont(footerFont);
    painter.drawText(footer.adjusted(6.0, 0.0, -6.0, 0.0),
                     Qt::AlignCenter, QStringLiteral("MASTER OUT"));
  }

private:
  QLabel *sectionLabel(const QString &text) {
    auto *label = new QLabel(text, this);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setFixedHeight(14);
    QFont font = label->font();
    font.setPointSize(std::max(7, font.pointSize() - 2));
    font.setBold(true);
    label->setFont(font);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, QColor(164, 168, 170));
    label->setPalette(pal);
    return label;
  }

  void syncFromMaster() {
    if (!masterBus_) {
      return;
    }

    const QSignalBlocker volumeBlocker(volumeSlider_);
    const QSignalBlocker muteBlocker(muteButton_);

    const float masterVolume = masterBus_->volume();
    if (!volumeSlider_->isSliderDown()) {
      volumeSlider_->setValue(volumeToPercent(masterVolume));
    }
    updateVolumePresentation(masterVolume);
    muteButton_->setChecked(masterBus_->isMuted());
    meterWidget_->setLevels(masterBus_->leftLevel(), masterBus_->rightLevel(),
                            masterBus_->leftLevel(), masterBus_->rightLevel());
    if (panKnob_) {
      panKnob_->setPan(0.0f);
      panKnob_->setLinked(true);
    }
  }

  void updateVolumePresentation(const float volume) {
    volumeValueLabel_->setText(volumeToDisplayText(volume));
    if (draggingVolume_ && meterWidget_) {
      const float db = volumeToMeterDb(volume);
      meterWidget_->setLevels(db, db);
    }
  }

  void commitPendingVolume() {
    if (!masterBus_) {
      return;
    }
    masterBus_->setVolume(pendingVolume_);
  }

  AudioMixerMasterBus *masterBus_ = nullptr;
  AudioStatusBadge *statusBadge_ = nullptr;
  QLabel *nameLabel_ = nullptr;
  AudioBusSlotLabel *busSlot_ = nullptr;
  AudioPanKnobWidget *panKnob_ = nullptr;
  AudioDbScaleWidget *scaleWidget_ = nullptr;
  AudioLevelMeterWidget *meterWidget_ = nullptr;
  AudioFaderSlider *volumeSlider_ = nullptr;
  QLabel *volumeValueLabel_ = nullptr;
  AudioMixerToggleButton *muteButton_ = nullptr;
  QTimer *volumeCommitTimer_ = nullptr;
  QColor accentColor_;
  float pendingVolume_ = 1.0f;
  bool draggingVolume_ = false;
};
} // namespace

W_OBJECT_IMPL(ArtifactCompositionAudioMixerWidget)

class ArtifactCompositionAudioMixerWidget::Impl {
public:
  AudioMixer *mixer_ = nullptr;
  QWidget *contentWidget_ = nullptr;
  QHBoxLayout *contentLayout_ = nullptr;
  QLabel *summaryLabel_ = nullptr;
  QLabel *emptyLabel_ = nullptr;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void clearRows() {
    if (!contentLayout_) {
      return;
    }
    while (QLayoutItem *item = contentLayout_->takeAt(0)) {
      if (QWidget *widget = item->widget()) {
        widget->deleteLater();
      }
      delete item;
    }
  }
};

ArtifactCompositionAudioMixerWidget::ArtifactCompositionAudioMixerWidget(
    QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);

  impl_->mixer_ = new AudioMixer(this);

  if (auto *playbackService = ArtifactPlaybackService::instance()) {
    if (auto *masterBus = impl_->mixer_->masterBus()) {
      QObject::connect(masterBus, &AudioMixerMasterBus::volumeChanged, this,
                       [playbackService](const float volume) {
                         playbackService->setAudioMasterVolume(volume);
                       });
      QObject::connect(masterBus, &AudioMixerMasterBus::muteChanged, this,
                       [playbackService](const bool muted) {
                         playbackService->setAudioMasterMuted(muted);
                       });
      playbackService->setAudioMasterVolume(masterBus->volume());
      playbackService->setAudioMasterMuted(masterBus->isMuted());
    }

    QObject::connect(
        playbackService, &ArtifactPlaybackService::audioLevelChanged, this,
        [this](float leftRms, float rightRms, float leftPeak, float rightPeak) {
          Q_UNUSED(leftPeak);
          Q_UNUSED(rightPeak);
          impl_->mixer_->updatePlaybackLevels(leftRms, rightRms);
        });
  }

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto *header = new QWidget(this);
  header->setAutoFillBackground(true);
  {
    QPalette headerPalette = header->palette();
    headerPalette.setColor(QPalette::Window, QColor(31, 34, 37));
    header->setPalette(headerPalette);
  }
  auto *headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(12, 9, 12, 9);
  headerLayout->setSpacing(1);

  auto *titleLabel = new QLabel(QStringLiteral("Audio Mixer"), header);
  auto *subtitleLabel = new QLabel(
      QStringLiteral("Master bus and current composition audio layers"),
      header);
  {
    QPalette titlePalette = titleLabel->palette();
    titlePalette.setColor(QPalette::WindowText, QColor(240, 243, 246));
    titleLabel->setPalette(titlePalette);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() > 0 ? titleFont.pointSize() + 1
                                                     : 11);
    titleLabel->setFont(titleFont);

    QPalette subtitlePalette = subtitleLabel->palette();
    subtitlePalette.setColor(QPalette::WindowText, QColor(166, 179, 195));
    subtitleLabel->setPalette(subtitlePalette);
  }

  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(subtitleLabel);
  impl_->summaryLabel_ = new QLabel(header);
  {
    impl_->summaryLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    impl_->summaryLabel_->setSizePolicy(QSizePolicy::Expanding,
                                        QSizePolicy::Preferred);
    QPalette summaryPalette = impl_->summaryLabel_->palette();
    summaryPalette.setColor(QPalette::WindowText, QColor(207, 214, 221));
    impl_->summaryLabel_->setPalette(summaryPalette);
    QFont summaryFont = impl_->summaryLabel_->font();
    summaryFont.setBold(true);
    summaryFont.setPointSize(summaryFont.pointSize() > 0 ? summaryFont.pointSize() - 1
                                                         : 9);
    impl_->summaryLabel_->setFont(summaryFont);
  }
  headerLayout->addWidget(impl_->summaryLabel_, 0, Qt::AlignRight);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->viewport()->setAutoFillBackground(true);
  {
    QPalette scrollPalette = scrollArea->viewport()->palette();
    scrollPalette.setColor(QPalette::Window, QColor(22, 25, 28));
    scrollArea->viewport()->setPalette(scrollPalette);
  }

  impl_->contentWidget_ = new QWidget(scrollArea);
  impl_->contentWidget_->setAutoFillBackground(true);
  {
    QPalette contentPalette = impl_->contentWidget_->palette();
    contentPalette.setColor(QPalette::Window, QColor(22, 25, 28));
    impl_->contentWidget_->setPalette(contentPalette);
  }
  impl_->contentLayout_ = new QHBoxLayout(impl_->contentWidget_);
  impl_->contentLayout_->setContentsMargins(10, 10, 10, 10);
  impl_->contentLayout_->setSpacing(6);
  impl_->contentLayout_->addStretch();
  scrollArea->setWidget(impl_->contentWidget_);

  rootLayout->addWidget(header);
  rootLayout->addWidget(scrollArea, 1);

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent &) {
            queueMixerRefresh(this);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent &) {
            queueMixerRefresh(this);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent &) {
            queueMixerRefresh(this);
          }));

  refreshFromCurrentComposition();
}

ArtifactCompositionAudioMixerWidget::~ArtifactCompositionAudioMixerWidget() {
  delete impl_;
}

void ArtifactCompositionAudioMixerWidget::refreshFromCurrentComposition() {
  ArtifactCompositionPtr composition;
  if (auto *service = ArtifactProjectService::instance()) {
    composition = service->currentComposition().lock();
  }

  impl_->mixer_->syncFromComposition(composition);
  impl_->clearRows();
  const auto strips = impl_->mixer_->allChannelStrips();
  int mutedCount = 0;
  int soloCount = 0;
  int fxCount = 0;
  for (auto *strip : strips) {
    if (!strip) {
      continue;
    }
    if (strip->isMuted()) {
      ++mutedCount;
    }
    if (strip->isSolo()) {
      ++soloCount;
    }
    const auto effectChain = strip->effectChain();
    fxCount += effectChain.size();
  }
  if (impl_->summaryLabel_) {
    if (strips.isEmpty()) {
      impl_->summaryLabel_->setText(QStringLiteral("Unavailable"));
    } else {
      impl_->summaryLabel_->setText(QStringLiteral("%1 layers · %2 FX · %3 solo · %4 mute")
                                        .arg(strips.size())
                                        .arg(fxCount)
                                        .arg(soloCount)
                                        .arg(mutedCount));
    }
  }

  if (strips.isEmpty()) {
    impl_->emptyLabel_ =
        new QLabel(QStringLiteral("Audio Mixer is unavailable until the composition has audio layers"),
                   impl_->contentWidget_);
    impl_->emptyLabel_->setAlignment(Qt::AlignCenter);
    impl_->emptyLabel_->setWordWrap(true);
    impl_->emptyLabel_->setMinimumWidth(260);
    impl_->emptyLabel_->setMaximumWidth(420);
    impl_->emptyLabel_->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
    impl_->emptyLabel_->setEnabled(false);
    impl_->emptyLabel_->setAutoFillBackground(true);
    QPalette emptyPalette = impl_->emptyLabel_->palette();
    emptyPalette.setColor(QPalette::WindowText, QColor(145, 155, 165));
    emptyPalette.setColor(QPalette::Window, QColor(22, 27, 32));
    emptyPalette.setColor(QPalette::Base, QColor(22, 27, 32));
    emptyPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                          QColor(108, 117, 126));
    emptyPalette.setColor(QPalette::Disabled, QPalette::Window,
                          QColor(20, 23, 26));
    impl_->emptyLabel_->setPalette(emptyPalette);
    impl_->contentLayout_->addStretch();
    impl_->contentLayout_->addWidget(impl_->emptyLabel_);
    impl_->contentLayout_->addStretch();
  } else {
    if (auto *masterBus = impl_->mixer_->masterBus()) {
      impl_->contentLayout_->addWidget(
          new AudioMixerMasterRow(masterBus, impl_->contentWidget_));
    }
    impl_->contentLayout_->addWidget(
        new AudioStripSeparatorWidget(impl_->contentWidget_));
    for (int i = 0; i < strips.size(); ++i) {
      auto *strip = strips.at(i);
      impl_->contentLayout_->addWidget(
          new AudioMixerStripRow(strip, impl_->contentWidget_, this));
      if (i + 1 < strips.size()) {
        impl_->contentLayout_->addWidget(
            new AudioStripSeparatorWidget(impl_->contentWidget_));
      }
    }
  }
  if (!strips.isEmpty()) {
    impl_->contentLayout_->addStretch();
  }
}

} // namespace Artifact
