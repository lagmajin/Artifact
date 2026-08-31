module;

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
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
  bool clipped_ = false;
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

class AudioRoutingButton final : public QPushButton {
public:
  explicit AudioRoutingButton(QWidget *parent = nullptr) : QPushButton(parent) {}

  std::function<void()> invoked;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton && invoked) {
      invoked();
      event->accept();
      return;
    }
    QPushButton::mouseReleaseEvent(event);
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space) && invoked) {
      invoked();
      event->accept();
      return;
    }
    QPushButton::keyReleaseEvent(event);
  }
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

} // namespace detail

} // namespace Artifact
