module;

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QSlider>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

export module Artifact.Widgets.AudioMixerControls;

export namespace Artifact {

namespace detail {

inline constexpr float audioMixerMaxDb = 6.0205999f; // Existing linear gain limit: 2.0.
inline constexpr int audioFaderSteps = 660;

float audioDbFraction(const float db) {
  // Put unity gain at an exact slider step (600), retaining the 2.0 gain ceiling.
  const float step = db <= 0.0f ? (db + 60.0f) * 10.0f
                               : 600.0f + db / audioMixerMaxDb * 60.0f;
  return std::clamp(step / audioFaderSteps, 0.0f, 1.0f);
}

// Share geometry with QSlider so the painted cap is also the native hit target.
class AudioFaderStyle final : public QProxyStyle {
public:
  AudioFaderStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}

  QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                       SubControl subControl, const QWidget *widget = nullptr) const override {
    const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (control == CC_Slider && slider && slider->orientation == Qt::Vertical) {
      if (subControl == SC_SliderGroove) return slider->rect;
      if (subControl == SC_SliderHandle) {
        const int y = QStyle::sliderPositionFromValue(slider->minimum, slider->maximum,
            slider->sliderPosition, std::max(0, slider->rect.height() - 26), slider->upsideDown);
        return QRect(slider->rect.x() + (slider->rect.width() - 30) / 2,
                     slider->rect.y() + y, 30, 26);
      }
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
  }

  int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr) const override {
    if (metric == PM_SliderLength) return 26;
    if (metric == PM_SliderControlThickness) return 30;
    return QProxyStyle::pixelMetric(metric, option, widget);
  }
};

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

class AudioLevelMeterWidget final : public QWidget {
public:
  explicit AudioLevelMeterWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedWidth(34);
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("Stereo level meter"));
    setToolTip(QStringLiteral("L / R signal level and peak hold in dBFS; red indicates clipping."));
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
    const bool clipped = peakLeft >= 0.0f || peakRight >= 0.0f;
    if (qFuzzyCompare(left_ + 61.0f, clampedLeft + 61.0f) &&
        qFuzzyCompare(right_ + 61.0f, clampedRight + 61.0f) &&
        qFuzzyCompare(peakLeft_ + 61.0f, clampedPeakLeft + 61.0f) &&
        qFuzzyCompare(peakRight_ + 61.0f, clampedPeakRight + 61.0f) &&
        clipped_ == clipped) {
      return;
    }
    left_ = clampedLeft;
    right_ = clampedRight;
    peakLeft_ = std::max(clampedPeakLeft, left_);
    peakRight_ = std::max(clampedPeakRight, right_);
    clipped_ = clipped;
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

    if (clipped_) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(235, 77, 73));
      painter.drawRoundedRect(QRectF(4.0, 3.0, width() - 8.0, 4.0), 2.0,
                              2.0);
    }

    const qreal outerMargin = 3.0;
    const qreal gap = 4.0;
    const qreal laneWidth = (width() - (outerMargin * 2.0) - gap) / 2.0;
    drawLane(&painter,
             QRectF(outerMargin, 13.0, laneWidth,
                    height() - 26.0),
             left_, peakLeft_);
    drawLane(&painter,
             QRectF(outerMargin + laneWidth + gap, 13.0, laneWidth,
                    height() - 26.0),
             right_, peakRight_);
    QFont labelFont = font();
    labelFont.setPixelSize(10);
    painter.setFont(labelFont);
    painter.setPen(QColor(185, 188, 193));
    painter.drawText(QRect(2, height() - 12, 14, 12), Qt::AlignCenter, QStringLiteral("L"));
    painter.drawText(QRect(width() - 16, height() - 12, 14, 12), Qt::AlignCenter, QStringLiteral("R"));
  }

private:
  static float meterFraction(const float db) {
    return audioDbFraction(db);
  }

  static void drawLane(QPainter *painter, const QRectF &rect, const float db,
                       const float peakDb) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(22, 28, 34));
    painter->drawRoundedRect(rect, 3.0, 3.0);

    // Fixed dB thresholds: low-level signals must not acquire a red tip.
    constexpr int segments = 40;
    for (int i = 0; i < segments; ++i) {
      const float segmentDb = -60.0f + (i + 1) *
          (60.0f + audioMixerMaxDb) / segments;
      const bool active = db > -60.0f && db >= segmentDb;
      const QColor color = segmentDb >= 0.0f ? QColor(223, 72, 62)
                          : segmentDb >= -6.0f ? QColor(228, 173, 83)
                                               : QColor(86, 190, 100);
      painter->setBrush(active ? color : QColor(29, 33, 35));
      painter->drawRect(QRectF(rect.left(),
          rect.bottom() - (i + 1) * rect.height() / segments,
          rect.width(), std::max(1.0, rect.height() / segments - 1.0)));
    }

    const qreal peakY = rect.bottom() - rect.height() * meterFraction(peakDb);
    if (peakDb > -60.0f) {
      painter->setPen(QPen(peakDb >= 0.0f ? QColor(235, 77, 73)
                                         : QColor(228, 173, 83), 2.0));
      painter->drawLine(QPointF(rect.left(), peakY), QPointF(rect.right(), peakY));
    }
  }

  float left_ = -60.0f;
  float right_ = -60.0f;
  float peakLeft_ = -60.0f;
  float peakRight_ = -60.0f;
  bool clipped_ = false;
};

class AudioDbScaleWidget final : public QWidget {
public:
  explicit AudioDbScaleWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedWidth(32);
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(70, 76, 82), 1.0));
    const QRectF rail(width() - 3.0, 13.0, 1.0, height() - 26.0);
    painter.setBrush(QColor(42, 46, 50));
    painter.drawRoundedRect(rail, 1.0, 1.0);

    const struct Tick {
      int db;
      bool major;
    } ticks[] = {
        {6, true}, {0, true}, {-6, true}, {-12, true}, {-18, false}, {-24, true},
        {-30, false}, {-36, true}, {-42, false}, {-48, true}, {-54, false},
        {-60, true},
    };

    QFont scaleFont = font();
    scaleFont.setPointSize(std::max(8, scaleFont.pointSize() - 1));
    painter.setFont(scaleFont);
    painter.setPen(QColor(177, 183, 188));

    for (const Tick &tick : ticks) {
      const qreal fraction = audioDbFraction(static_cast<float>(tick.db));
      const qreal y = 13.0 + (height() - 26.0) * (1.0 - fraction);
      const qreal tickLength = tick.major ? 7.0 : 4.0;
      painter.setPen(tick.db == 0 ? QColor(228, 173, 83) : QColor(177, 183, 188));
      painter.drawLine(QPointF(width() - tickLength, y), QPointF(width(), y));
      if (tick.major) {
        const QString label =
            tick.db == 0 ? QStringLiteral("0") : QString::number(tick.db);
        painter.drawText(QRect(0, static_cast<int>(y - 7.0), width() - 9, 14),
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
    QColor fill(34, 36, 39);
    QColor border(67, 70, 74);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(bounds, 2.0, 2.0);

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

class AudioPanKnobWidget final : public QWidget {
public:
  explicit AudioPanKnobWidget(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedSize(64, 66);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("Pan"));
    setToolTip(QStringLiteral("Drag up/down or left/right to pan. Shift: fine adjustment. "
                              "Arrow keys: adjust. Home or double-click: center."));
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
    setAccessibleDescription(linked ? QStringLiteral("Stereo-linked pan")
                                    : QStringLiteral("Independent channel pan"));
    update();
  }

protected:
  bool event(QEvent *event) override {
    if (!isEnabled()) return QWidget::event(event);
    if (event->type() == QEvent::MouseButtonDblClick) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        dragPan_ = false;
        setPan(0.0f);
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonPress) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() != Qt::LeftButton) return QWidget::event(event);
      setFocus(Qt::MouseFocusReason);
      dragPan_ = true;
      dragOrigin_ = mouseEvent->position();
      return true;
    }
    if (event->type() == QEvent::MouseMove && dragPan_) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      setPanFromEvent(mouseEvent);
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && dragPan_) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() != Qt::LeftButton) return QWidget::event(event);
      dragPan_ = false;
      setPanFromEvent(mouseEvent);
      return true;
    }
    if (event->type() == QEvent::KeyPress) {
      auto *key = static_cast<QKeyEvent *>(event);
      const float step = key->modifiers().testFlag(Qt::ShiftModifier) ? 0.001f : 0.01f;
      switch (key->key()) {
      case Qt::Key_Left: case Qt::Key_Down: setPan(pan_ - step); return true;
      case Qt::Key_Right: case Qt::Key_Up: setPan(pan_ + step); return true;
      case Qt::Key_Home: setPan(0.0f); return true;
      default: break;
      }
    }
    if (event->type() == QEvent::Wheel) {
      auto *wheel = static_cast<QWheelEvent *>(event);
      if (!hasFocus()) { wheel->ignore(); return false; }
      setPan(pan_ + wheel->angleDelta().y() / 120.0f *
          (wheel->modifiers().testFlag(Qt::ShiftModifier) ? 0.001f : 0.01f));
      wheel->accept();
      return true;
    }
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) update();
    return QWidget::event(event);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center(width() * 0.5, 27.0);
    const qreal radius = 19.0;

    // Pro-Q reference: quiet disc, thin rim, and a value arc outside the face.
    // The bipolar arc grows from center; keep the existing pan interaction.
    painter.setPen(QPen(QColor(17, 18, 20), 1.0));
    painter.setBrush(QColor(28, 29, 32));
    painter.drawEllipse(center, radius, radius);

    painter.setBrush(isEnabled() ? QColor(48, 48, 52) : QColor(38, 39, 42));
    painter.setPen(QPen(isEnabled() ? QColor(102, 101, 108) : QColor(62, 63, 67), 0.8));
    painter.drawEllipse(center, radius - 1.5, radius - 1.5);

    const QRectF arcBounds(center.x() - 22.0, center.y() - 22.0, 44.0, 44.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(68, 68, 74), 1.0));
    painter.drawArc(arcBounds, 202 * 16, -224 * 16);
    painter.setPen(QPen(QColor(128, 128, 134), 1.0));
    painter.drawLine(QPointF(center.x(), center.y() - 25.0),
                     QPointF(center.x(), center.y() - 23.5));
    const QColor amber = isEnabled() ? QColor(228, 173, 83) : QColor(116, 118, 121);
    painter.setPen(QPen(amber, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcBounds,
                    90 * 16, static_cast<int>(-pan_ * 112.0f * 16.0f));
    const qreal indicatorDegrees = -90.0 + static_cast<qreal>(pan_) * 112.0;
    painter.drawLine(radialPoint(center, radius - 5.0, indicatorDegrees),
                     radialPoint(center, radius - 2.0, indicatorDegrees));
    if (isEnabled() && hasFocus()) {
      painter.setPen(QPen(QColor(228, 173, 83, 115), 1.0));
      painter.drawEllipse(center, 25.5, 25.5);
    }

    QFont labelFont = font();
    labelFont.setPointSize(std::max(7, labelFont.pointSize() - 1));
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(amber);
    painter.drawText(QRect(0, 49, width(), 16), Qt::AlignCenter, panText(pan_));
  }

private:
  void setPanFromEvent(QMouseEvent *event) {
    const QPointF delta = event->position() - dragOrigin_;
    dragOrigin_ = event->position();
    const float sensitivity = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.001f : 0.01f;
    setPan(pan_ + static_cast<float>(delta.x() - delta.y()) * sensitivity);
  }

  float pan_ = 0.0f;
  bool linked_ = true;
  bool dragPan_ = false;
  QPointF dragOrigin_;
  std::function<void(float)> panChanged_;
};

class AudioMixerToggleButton final : public QPushButton {
public:
  explicit AudioMixerToggleButton(const QString &text, QWidget *parent = nullptr)
      : QPushButton(text, parent) {
    setCheckable(true);
    setFixedSize(52, 26);
    setCursor(Qt::PointingHandCursor);
    const bool solo = text == QStringLiteral("S");
    setIcon(QIcon(solo ? QStringLiteral(":/icons/Studio/mixer_solo.svg")
                       : QStringLiteral(":/icons/Studio/mixer_mute.svg")));
    setAccessibleName(solo ? QStringLiteral("Solo") : QStringLiteral("Mute"));
    setToolTip(solo ? QStringLiteral("Solo this audio layer")
                    : QStringLiteral("Mute this channel"));
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
    QColor fill = isChecked() ? accentColor_.darker(250) : QColor(38, 40, 43);
    QColor border = (isChecked() || hasFocus()) ? accentColor_ : QColor(74, 77, 81);
    QColor textColor = isChecked() ? accentColor_ : QColor(214, 220, 225);
    if (isDown()) fill = fill.lighter(125);
    if (!isEnabled()) { fill = QColor(34, 36, 38); textColor = QColor(108, 111, 114); }
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill);
    painter.drawRoundedRect(bounds, 4.0, 4.0);

    QFont f = font();
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(textColor);
    icon().paint(&painter, QRect(6, (height() - 16) / 2, 16, 16), Qt::AlignCenter,
                 isEnabled() ? QIcon::Normal : QIcon::Disabled);
    painter.drawText(rect().adjusted(25, 0, -3, 0), Qt::AlignCenter, text());
  }

private:
  QColor accentColor_ = QColor(211, 170, 66);
};

class AudioRoutingButton final : public QPushButton {
public:
  explicit AudioRoutingButton(QWidget *parent = nullptr) : QPushButton(parent) {}

  std::function<void()> invoked;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate = isEnabled() && isDown() &&
        event->button() == Qt::LeftButton && rect().contains(event->position().toPoint());
    QPushButton::mouseReleaseEvent(event);
    if (activate && invoked) {
      invoked();
      event->accept();
    }
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    QPushButton::keyReleaseEvent(event);
    if (isEnabled() && !event->isAutoRepeat() &&
        (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space) && invoked) {
      invoked();
      event->accept();
    }
  }
};

class AudioFaderSlider final : public QSlider {
public:
  explicit AudioFaderSlider(QWidget *parent = nullptr)
      : QSlider(Qt::Vertical, parent) {
    auto *faderStyle = new AudioFaderStyle();
    faderStyle->setParent(this);
    setStyle(faderStyle);
    setRange(0, audioFaderSteps);
    setSingleStep(1);
    setFixedWidth(38);
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("Volume fader"));
    setToolTip(QStringLiteral("Volume in dB. Arrow keys: fine adjustment; Page Up/Down: about 1 dB. "
                              "Double-click: 0 dB. Minimum: silence."));
    setPageStep(10);
    setTracking(true);
  }

  void setAccentColor(const QColor &color) {
    accentColor_ = color;
    update();
  }

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      setValue(600);
      setSliderDown(false);
      event->accept();
      return;
    }
    QSlider::mouseDoubleClickEvent(event);
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!hasFocus()) { event->ignore(); return; }
    QSlider::wheelEvent(event);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRectF handle = style()->subControlRect(QStyle::CC_Slider, &option,
                                                 QStyle::SC_SliderHandle, this);
    const QRectF rail(width() * 0.5 - 3.0, 13.0, 6.0, height() - 26.0);
    painter.setPen(QPen(QColor(22, 25, 28), 1.0));
    painter.setBrush(QColor(38, 43, 47));
    painter.drawRoundedRect(rail, 3.0, 3.0);

    painter.setPen(QPen(QColor(88, 94, 100), 1.0));
    for (const int db : {6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -54, -60}) {
      const qreal y = rail.bottom() - rail.height() * audioDbFraction(static_cast<float>(db));
      const qreal tick = db == 0 ? 12.0 : 5.0;
      painter.setPen(db == 0 ? QColor(228, 173, 83) : QColor(76, 80, 85));
      painter.drawLine(QPointF(rail.left() - tick, y),
                       QPointF(rail.left() - 2.0, y));
      painter.drawLine(QPointF(rail.right() + 2.0, y),
                       QPointF(rail.right() + tick, y));
    }

    QLinearGradient handleGradient(handle.topLeft(), handle.bottomLeft());
    handleGradient.setColorAt(0.0, QColor(211, 214, 218));
    handleGradient.setColorAt(0.18, QColor(155, 159, 164));
    handleGradient.setColorAt(0.52, QColor(191, 194, 198));
    handleGradient.setColorAt(1.0, QColor(105, 109, 115));
    painter.setPen(QPen(hasFocus() ? QColor(228, 173, 83) : QColor(18, 20, 22), 1.0));
    painter.setBrush(handleGradient);
    painter.drawRoundedRect(handle, 3.0, 3.0);
    painter.setPen(QPen(QColor(80, 84, 88), 1.0));
    for (int offset : {-5, 5}) {
      painter.drawLine(QPointF(handle.left() + 4.0, handle.center().y() + offset),
                       QPointF(handle.right() - 4.0, handle.center().y() + offset));
    }
    painter.setPen(QPen(QColor(228, 173, 83), 2.0));
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
    setFixedWidth(12);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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

} // namespace detail

} // namespace Artifact
