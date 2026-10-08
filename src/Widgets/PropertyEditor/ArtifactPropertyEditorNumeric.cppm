module;
#include <QApplication>
#include <QAbstractButton>
#include <QColor>
#include <QDialog>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.RelativeSpinBox;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import FloatColorPickerDialog;
import Utils.Path;
import Memory.SharedPtr;

namespace Artifact {
void installSliderJumpBehavior(QWidget *pickerRoot);
} // namespace Artifact

namespace Artifact::detail {
extern const int kNumericEditorValueWidth;
extern ArtifactNumericEditorLayoutMode g_numericEditorLayoutMode;
void applyPropertyFieldPalette(QWidget *widget, bool elevated);
void applyThemeTextPalette(QWidget *widget, int shade);
std::pair<double, double> resolveFloatSoftRange(
    const ArtifactCore::AbstractProperty &property,
    const ArtifactCore::PropertyMetadata &meta, double hardMin,
    double hardMax);
std::pair<int, int> resolveIntSoftRange(
    const ArtifactCore::AbstractProperty &property,
    const ArtifactCore::PropertyMetadata &meta, int hardMin, int hardMax);
} // namespace Artifact::detail

namespace Artifact {
using namespace detail;

namespace {

// Keep interactive edits visually continuous. Expensive render work is
// coalesced by the composition render controller, so the editor only needs to
// cap input-side traffic to roughly one update per display frame.
constexpr qint64 kSliderPreviewIntervalMs = 16;

bool isScalePercentProperty(const ArtifactCore::AbstractProperty &property) {
  const QString name = property.getName();
  return name.compare(QStringLiteral("reveal.progress"), Qt::CaseInsensitive) == 0 ||
         name.compare(QStringLiteral("transform.scale.x"),
                      Qt::CaseInsensitive) == 0 ||
         name.compare(QStringLiteral("transform.scale.y"),
                      Qt::CaseInsensitive) == 0;
}

double storageToDisplayValue(const double value, const bool displayAsPercent) {
  return displayAsPercent ? value * 100.0 : value;
}

double displayToStorageValue(const double value, const bool displayAsPercent) {
  return displayAsPercent ? value / 100.0 : value;
}

template <typename Number>
std::optional<Number> parseQuickNumericExpression(const QString &text,
                                                  const Number base) {
  const QString normalized = text.trimmed().remove(QLatin1Char(' '));
  bool ok = false;
  const double absolute = normalized.toDouble(&ok);
  if (ok) {
    return static_cast<Number>(absolute);
  }

  static const QRegularExpression relativePattern(
      QStringLiteral(R"(^([+\-*/])((?:\d+(?:\.\d*)?|\.\d+))$)"));
  const auto match = relativePattern.match(normalized);
  if (!match.hasMatch()) {
    return std::nullopt;
  }

  const double operand = match.captured(2).toDouble(&ok);
  if (!ok) {
    return std::nullopt;
  }
  double result = static_cast<double>(base);
  switch (match.captured(1).at(0).toLatin1()) {
  case '+': result += operand; break;
  case '-': result -= operand; break;
  case '*': result *= operand; break;
  case '/':
    if (std::abs(operand) <= std::numeric_limits<double>::epsilon()) {
      return std::nullopt;
    }
    result /= operand;
    break;
  default: return std::nullopt;
  }
  return static_cast<Number>(result);
}

int decimalsForNumericProperty(const ArtifactCore::PropertyMetadata &meta,
                               const ArtifactCore::AbstractProperty &property,
                               const bool displayAsPercent = false) {
  if (meta.step.isValid()) {
    const QString stepText = meta.step.toString();
    const int dot = stepText.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
      const int precision = static_cast<int>(stepText.size()) - dot - 1;
      return std::clamp(displayAsPercent ? std::max(0, precision - 2) : precision,
                        0, 4);
    }
    return 0;
  }

  if (displayAsPercent) {
    return 0;
  }

  if (meta.unit.compare(QStringLiteral("px"), Qt::CaseInsensitive) == 0) {
    return 0;
  }

  const QString name = property.getName();
  if (name.contains(QStringLiteral("opacity"), Qt::CaseInsensitive) ||
      name.contains(QStringLiteral("scale"), Qt::CaseInsensitive)) {
    return 2;
  }
  return 2;
}

class GradingWheelCanvas final : public QWidget {
 public:
  explicit GradingWheelCanvas(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("Color balance wheel"));
    setAccessibleDescription(QStringLiteral(
        "Drag from the center toward a hue to add color balance. The center is neutral."));
  }

  QPointF point() const { return point_; }

  void setPoint(QPointF point) {
    const double radius = std::hypot(point.x(), point.y());
    if (radius > 1.0) {
      point /= radius;
    }
    point_ = point;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double radius = std::max(1.0, std::min(width(), height()) * 0.43);
    const QPointF center = rect().center();
    const double innerRadius = radius * 0.82;
    for (int segment = 0; segment < 144; ++segment) {
      const double a0 = (segment / 144.0) * 6.283185307179586;
      const double a1 = ((segment + 1) / 144.0) * 6.283185307179586;
      const QColor hue = QColor::fromHsvF(1.0 - segment / 144.0, 0.9, 0.95);
      painter.setPen(QPen(hue, std::max(4.0, radius * 0.12), Qt::SolidLine,
                          Qt::FlatCap));
      const QPointF p0(center.x() + std::cos(a0) * (innerRadius + radius * 0.025),
                       center.y() - std::sin(a0) * (innerRadius + radius * 0.025));
      const QPointF p1(center.x() + std::cos(a1) * (innerRadius + radius * 0.025),
                       center.y() - std::sin(a1) * (innerRadius + radius * 0.025));
      painter.drawLine(p0, p1);
    }

    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
    painter.setBrush(palette().color(QPalette::Base));
    painter.drawEllipse(center, innerRadius, innerRadius);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(center.x() - innerRadius, center.y()),
                     QPointF(center.x() + innerRadius, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - innerRadius),
                     QPointF(center.x(), center.y() + innerRadius));

    const QPointF puck(center.x() + point_.x() * innerRadius,
                       center.y() - point_.y() * innerRadius);
    painter.setPen(QPen(palette().color(QPalette::Text), 2.0));
    painter.setBrush(QColor(245, 247, 250, 225));
    painter.drawEllipse(puck, 6.0, 6.0);
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() == Qt::LeftButton) {
      updateFromPosition(event->position());
      event->accept();
      return;
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    if (event->buttons().testFlag(Qt::LeftButton)) {
      updateFromPosition(event->position());
      event->accept();
      return;
    }
    QWidget::mouseMoveEvent(event);
  }

  void keyPressEvent(QKeyEvent* event) override {
    QPointF next = point_;
    const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.01 : 0.05;
    switch (event->key()) {
    case Qt::Key_Left: next.rx() -= step; break;
    case Qt::Key_Right: next.rx() += step; break;
    case Qt::Key_Up: next.ry() += step; break;
    case Qt::Key_Down: next.ry() -= step; break;
    case Qt::Key_Home: next = QPointF(); break;
    default:
      QWidget::keyPressEvent(event);
      return;
    }
    setPoint(next);
    event->accept();
  }

 private:
  void updateFromPosition(const QPointF& position) {
    const double radius = std::max(1.0, std::min(width(), height()) * 0.43 * 0.82);
    const QPointF center = rect().center();
    const QPointF delta(position.x() - center.x(), center.y() - position.y());
    const double length = std::hypot(delta.x(), delta.y());
    const double scale = length > radius ? radius / length : 1.0;
    setPoint(QPointF(delta.x() * scale / radius, delta.y() * scale / radius));
  }

  QPointF point_;
};

class GradingWheelDialog final : public QDialog {
 public:
  explicit GradingWheelDialog(const QString& title, const QPointF& point,
                              QWidget* parent)
      : QDialog(parent) {
    setWindowTitle(title);
    setModal(true);
    resize(300, 350);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto* canvas = new GradingWheelCanvas(this);
    canvas->setPoint(point);
    layout->addWidget(canvas, 1);
    auto* hint = new QLabel(QStringLiteral(
        "Center is neutral; drag toward a hue to shift the balance."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    auto* cancel = new GradingWheelActionButton(QStringLiteral("Cancel"), this);
    auto* apply = new GradingWheelActionButton(QStringLiteral("Apply"), this);
    cancel->setAccessibleName(QStringLiteral("Cancel color balance edit"));
    apply->setAccessibleName(QStringLiteral("Apply color balance edit"));
    cancel->setAction([this]() { reject(); });
    apply->setAction([this]() { accept(); });
    actions->addWidget(cancel);
    actions->addWidget(apply);
    layout->addLayout(actions);
    canvas_ = canvas;
  }

  QPointF point() const { return canvas_ ? canvas_->point() : QPointF(); }

 private:
  class GradingWheelActionButton : public QPushButton {
   public:
    using QPushButton::QPushButton;
    void setAction(std::function<void()> action) {
      action_ = std::move(action);
      setFocusPolicy(Qt::StrongFocus);
    }

   protected:
    void nextCheckState() override {
      if (action_) action_();
    }

   private:
    std::function<void()> action_;
  };

  GradingWheelCanvas* canvas_ = nullptr;
};

class GradingWheelOpenButton final : public QPushButton {
 public:
  using QPushButton::QPushButton;
  void setAction(std::function<void()> action) { action_ = std::move(action); }

 protected:
  void nextCheckState() override {
    if (action_) action_();
  }

 private:
  std::function<void()> action_;
};

} // namespace

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  auto initializing = ArtifactCore::makeShared<bool>(true);
  const bool displayAsPercent = isScalePercentProperty(property);
  setObjectName(QStringLiteral("propertyFloatEditor"));
  setAccessibleName(QStringLiteral("Float property editor"));
  setAccessibleDescription(QStringLiteral("Edit the selected floating-point property"));
  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  spinBox_->setAccessibleName(QStringLiteral("Floating-point property value"));
  spinBox_->setAccessibleDescription(QStringLiteral("Enter the numeric property value"));
  spinBox_->setProperty("displayAsPercent", displayAsPercent);
  if (showSlider) {
    slider_ = new detail::PropertySliderWidget(this);
    slider_->setAccessibleName(QStringLiteral("Floating-point property slider"));
    slider_->setAccessibleDescription(QStringLiteral("Adjust the floating-point property value"));
    applyPropertyFieldPalette(slider_);
  }
  auto previewThrottle = ArtifactCore::makeShared<QElapsedTimer>();
  previewThrottle->start();

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  spinBox_->setMinimumWidth(kNumericEditorValueWidth);
  spinBox_->setMaximumWidth(kNumericEditorValueWidth);
  spinBox_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
    } else {
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
      layout->addWidget(slider_, 3);
    }
  } else {
    layout->addWidget(spinBox_, 1);
  }

  const auto meta = property.metadata();
  const double storedHardMin =
      meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6;
  const double storedHardMax =
      meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6;
  const auto [resolvedSoftMin, resolvedSoftMax] =
      resolveFloatSoftRange(property, meta, storedHardMin, storedHardMax);
  const double hardMin = storageToDisplayValue(storedHardMin, displayAsPercent);
  const double hardMax = storageToDisplayValue(storedHardMax, displayAsPercent);
  softMin_ = storageToDisplayValue(resolvedSoftMin, displayAsPercent);
  softMax_ = storageToDisplayValue(resolvedSoftMax, displayAsPercent);
  if (softMax_ <= softMin_) {
    softMin_ = hardMin;
    softMax_ = hardMax;
  }
  spinBox_->setRange(hardMin, hardMax);
  spinBox_->setValue(
      storageToDisplayValue(property.getValue().toDouble(), displayAsPercent));
  spinBox_->setDecimals(
      decimalsForNumericProperty(meta, property, displayAsPercent));
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(
        storageToDisplayValue(meta.step.toDouble(), displayAsPercent));
  }
  const QString displayUnit =
      displayAsPercent ? QStringLiteral("%") : meta.unit;
  if (!displayUnit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + displayUnit);
  }
  const QString sliderUnit = displayUnit;
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(24);
    slider_->setTracking(true); // ドラッグ中の追従を有効化
    slider_->setValue(floatToSliderPosition(
        storageToDisplayValue(property.getValue().toDouble(), displayAsPercent),
        softMin_, softMax_));
    slider_->setDisplayText(spinBox_->text());
  }

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, sliderUnit, displayAsPercent](const double nextValue) {
                      if (slider_) {
                        const QSignalBlocker blocker(slider_);
                        slider_->setValue(
                            floatToSliderPosition(nextValue, softMin_, softMax_));
                        slider_->setDisplayText(spinBox_->text());
                      }
                     if (spinBox_->hasFocus() && !sliderInteracting_) {
                       previewValue(
                           displayToStorageValue(nextValue, displayAsPercent));
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this, initializing, displayAsPercent]() {
                     if (*initializing) {
                       return;
                     }
                     if (spinBox_->findChild<QLineEdit*>()) {
                       const auto parsed = parseQuickNumericExpression<double>(
                           spinBox_->cleanText(), spinBox_->value());
                       if (parsed.has_value()) {
                         spinBox_->setValue(std::clamp(*parsed, spinBox_->minimum(),
                                                       spinBox_->maximum()));
                       }
                     }
                     commitValue(displayToStorageValue(spinBox_->value(),
                                                       displayAsPercent));
                   });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this,
                     [this, previewThrottle]() {
      sliderInteracting_ = true;
      previewCurrentValue();
      previewThrottle->restart();
    });

    QObject::connect(
        slider_, &QSlider::valueChanged, this,
        [this, sliderUnit, displayAsPercent,
         previewThrottle](const int sliderValue) {
          const double nextValue =
              this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
          const QSignalBlocker blocker(spinBox_);
          spinBox_->setValue(nextValue);
          if (sliderInteracting_ &&
              previewThrottle->elapsed() >= kSliderPreviewIntervalMs) {
            previewThrottle->restart();
            previewValue(
                displayToStorageValue(nextValue, displayAsPercent));
          }
        });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this, initializing]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      if (*initializing) {
        return;
      }
      commitCurrentValue();
    });

    Artifact::installSliderJumpBehavior(this);
  }
  *initializing = false;
}

bool ArtifactFloatPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (!slider_ || watched != slider_) {
    return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
  }

  auto sliderValueForX = [this](const int x) {
    const double ratio =
        static_cast<double>(x) / static_cast<double>(std::max(1, slider_->width()));
    return static_cast<int>(std::round(std::clamp(ratio, 0.0, 1.0) *
                                       static_cast<double>(slider_->maximum())));
  };

  if (event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      sliderDragArmed_ = true;
      sliderDragActive_ = false;
      sliderInteracting_ = true;
      sliderDragStartPos_ = mouseEvent->pos();
      sliderDragStartValue_ = slider_->value();
      slider_->setSliderDown(true);
      return true;
    }
  }
  if (event->type() == QEvent::MouseMove && sliderDragArmed_) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (!(mouseEvent->buttons() & Qt::LeftButton)) {
      sliderDragArmed_ = false;
      sliderDragActive_ = false;
      sliderInteracting_ = false;
      slider_->setSliderDown(false);
      return true;
    }
    if (!sliderDragActive_ &&
        (mouseEvent->pos() - sliderDragStartPos_).manhattanLength() <
            QApplication::startDragDistance()) {
      return true;
    }
    sliderDragActive_ = true;
    slider_->setValue(sliderValueForX(mouseEvent->pos().x()));
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && sliderDragArmed_) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      if (sliderDragActive_) {
        slider_->setValue(sliderValueForX(mouseEvent->pos().x()));
      } else {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(sliderDragStartValue_);
      }
      sliderDragArmed_ = false;
      const bool shouldCommit = sliderDragActive_;
      sliderDragActive_ = false;
      sliderInteracting_ = false;
      slider_->setSliderDown(false);
      if (shouldCommit) {
        commitCurrentValue();
      }
      return true;
    }
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}

int ArtifactFloatPropertyEditor::floatToSliderPosition(double val, double min,
                                                       double max) const {
  if (std::abs(max - min) < 1e-7)
    return 0;
  double ratio = (val - min) / (max - min);
  return static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 10000.0);
}

double ArtifactFloatPropertyEditor::sliderPositionToFloat(int pos, double min,
                                                          double max) const {
  double ratio = static_cast<double>(pos) / 10000.0;
  return min + ratio * (max - min);
}

QVariant ArtifactFloatPropertyEditor::value() const {
  if (!spinBox_) {
    return QVariant();
  }
  return QVariant(displayToStorageValue(
      spinBox_->value(), spinBox_->property("displayAsPercent").toBool()));
}

void ArtifactFloatPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const double nextValue = storageToDisplayValue(
      value.toDouble(), spinBox_->property("displayAsPercent").toBool());
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (slider_) {
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(this->floatToSliderPosition(nextValue, softMin_, softMax_));
    slider_->setDisplayText(spinBox_->text());
  }
}

bool ArtifactFloatPropertyEditor::supportsScrub() const { return true; }

ArtifactPoint2DPropertyEditor::ArtifactPoint2DPropertyEditor(
    const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent) {
  usesColorWheel_ = property.getName().endsWith(QStringLiteral(" Wheel"));
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  if (usesColorWheel_) {
    colorWheelButton_ = new GradingWheelOpenButton(this);
    colorWheelButton_->setText(QStringLiteral("Open color wheel…"));
    colorWheelButton_->setAccessibleName(property.getName());
    colorWheelButton_->setToolTip(QStringLiteral(
        "Open a color-balance wheel. The center is neutral."));
    colorWheelButton_->setMinimumHeight(26);
    applyPropertyFieldPalette(colorWheelButton_, false);
    layout->addWidget(colorWheelButton_, 1);
    setValueFromVariant(property.getValue());
    auto* const openColorWheelButton =
        static_cast<GradingWheelOpenButton*>(colorWheelButton_);
    openColorWheelButton->setAction([this, title = property.getName()]() {
      GradingWheelDialog dialog(title, value().toPointF(), this);
      if (dialog.exec() != QDialog::Accepted) {
        return;
      }
      setValueFromVariant(QVariant::fromValue(dialog.point()));
      commitCurrentValue();
    });
    return;
  }
  xSpinBox_ = new QDoubleSpinBox(this);
  ySpinBox_ = new QDoubleSpinBox(this);
  for (auto* spinBox : {xSpinBox_, ySpinBox_}) {
    spinBox->setDecimals(3);
    spinBox->setRange(-1000000000.0, 1000000000.0);
    spinBox->setSingleStep(1.0);
    spinBox->setKeyboardTracking(false);
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyPropertyFieldPalette(spinBox);
    layout->addWidget(spinBox);
  }
  xSpinBox_->setPrefix(QStringLiteral("X "));
  ySpinBox_->setPrefix(QStringLiteral("Y "));
  setValueFromVariant(property.getValue());
  QObject::connect(xSpinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this]() { previewCurrentValue(); });
  QObject::connect(ySpinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this]() { previewCurrentValue(); });
  QObject::connect(xSpinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitCurrentValue(); });
  QObject::connect(ySpinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitCurrentValue(); });
}

QVariant ArtifactPoint2DPropertyEditor::value() const {
  if (usesColorWheel_) {
    return QVariant::fromValue(QPointF(colorWheelX_, colorWheelY_));
  }
  return QVariant::fromValue(QPointF(xSpinBox_->value(), ySpinBox_->value()));
}

void ArtifactPoint2DPropertyEditor::setValueFromVariant(const QVariant& value) {
  const QPointF point = value.canConvert<QPointF>() ? value.toPointF() : QPointF();
  if (usesColorWheel_) {
    colorWheelX_ = std::round(std::clamp(point.x(), -1.0, 1.0) * 1000.0) / 1000.0;
    colorWheelY_ = std::round(std::clamp(point.y(), -1.0, 1.0) * 1000.0) / 1000.0;
    return;
  }
  QSignalBlocker xBlocker(xSpinBox_);
  QSignalBlocker yBlocker(ySpinBox_);
  xSpinBox_->setValue(point.x());
  ySpinBox_->setValue(point.y());
}

QWidget *ArtifactFloatPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }
  auto *spinBox = static_cast<ArtifactRelativeDoubleSpinBox *>(spinBox_);
  if (auto *lineEdit = spinBox->scrubLineEdit()) {
    return lineEdit;
  }
  return ArtifactAbstractPropertyEditor::scrubTargetWidget();
}

void ArtifactFloatPropertyEditor::scrubByPixels(
    const int deltaPixels, const Qt::KeyboardModifiers modifiers) {
  if (!spinBox_) {
    return;
  }

  double range = std::abs(softMax_ - softMin_);
  if (range < 1e-5)
    range = 100.0;

  double sensitivity = std::max(std::abs(spinBox_->singleStep()), range / 500.0);
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    sensitivity *= 0.1;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    sensitivity *= 5.0;
  }

  const double nextValue =
      spinBox_->value() + static_cast<double>(deltaPixels) * sensitivity;
  spinBox_->setValue(nextValue);
}

ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  auto initializing = ArtifactCore::makeShared<bool>(true);
  setObjectName(QStringLiteral("propertyIntEditor"));
  setAccessibleName(QStringLiteral("Integer property editor"));
  setAccessibleDescription(QStringLiteral("Edit the selected integer property"));
  spinBox_ = new ArtifactRelativeSpinBox(this);
  spinBox_->setAccessibleName(QStringLiteral("Integer property value"));
  spinBox_->setAccessibleDescription(QStringLiteral("Enter the integer property value"));
  if (showSlider) {
    slider_ = new detail::PropertySliderWidget(this);
    slider_->setAccessibleName(QStringLiteral("Integer property slider"));
    slider_->setAccessibleDescription(QStringLiteral("Adjust the integer property value"));
    applyPropertyFieldPalette(slider_);
  }
  auto previewThrottle = ArtifactCore::makeShared<QElapsedTimer>();
  previewThrottle->start();
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  spinBox_->setMinimumWidth(kNumericEditorValueWidth);
  spinBox_->setMaximumWidth(kNumericEditorValueWidth);
  spinBox_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
    } else {
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
      layout->addWidget(slider_, 3);
    }
  } else {
    layout->addWidget(spinBox_, 1);
  }

  const auto meta = property.metadata();
  const int hardMin = meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000;
  const int hardMax = meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000;
  const auto [resolvedSoftMin, resolvedSoftMax] =
      resolveIntSoftRange(property, meta, hardMin, hardMax);
  softMin_ = resolvedSoftMin;
  softMax_ = resolvedSoftMax;
  if (softMax_ <= softMin_) {
    softMin_ = hardMin;
    softMax_ = hardMax;
  }
  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
                     meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
  spinBox_->setValue(property.getValue().toInt());
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toInt());
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  const QString sliderUnit = meta.unit;
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(24);
    slider_->setTracking(true);
    slider_->setValue(
        intToSliderPosition(property.getValue().toInt(), softMin_, softMax_));
    slider_->setDisplayText(spinBox_->text());
  }

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this, sliderUnit](const int nextValue) {
        if (slider_) {
          const QSignalBlocker blocker(slider_);
          slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
          slider_->setDisplayText(spinBox_->text());
        }
        if (spinBox_->hasFocus() && !sliderInteracting_) {
          previewValue(nextValue);
        }
      });
  QObject::connect(spinBox_, &QSpinBox::editingFinished, this,
                   [this, initializing]() {
                     if (*initializing) {
                       return;
                     }
                     if (spinBox_->findChild<QLineEdit*>()) {
                       const auto parsed = parseQuickNumericExpression<int>(
                           spinBox_->cleanText(), spinBox_->value());
                       if (parsed.has_value()) {
                         spinBox_->setValue(std::clamp(*parsed, spinBox_->minimum(),
                                                       spinBox_->maximum()));
                       }
                     }
                     commitValue(spinBox_->value());
                   });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this,
                     [this, previewThrottle]() {
      sliderInteracting_ = true;
      previewCurrentValue();
      previewThrottle->restart();
    });
    QObject::connect(slider_, &QSlider::valueChanged, this,
                     [this, sliderUnit, previewThrottle](const int sliderValue) {
                       const int nextValue =
                           sliderPositionToInt(sliderValue, softMin_, softMax_);
                       const QSignalBlocker blocker(spinBox_);
                       spinBox_->setValue(nextValue);
                       if (sliderInteracting_ &&
                           previewThrottle->elapsed() >=
                               kSliderPreviewIntervalMs) {
                         previewThrottle->restart();
                         previewValue(nextValue);
                       }
                     });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this, initializing]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      if (*initializing) {
        return;
      }
      commitCurrentValue();
    });
    Artifact::installSliderJumpBehavior(this);
  }
  *initializing = false;
}

QVariant ArtifactIntPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactIntPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const int nextValue = value.toInt();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (slider_) {
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
    slider_->setDisplayText(spinBox_->text());
  }
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

QWidget *ArtifactIntPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }
  return spinBox_;
}

} // namespace Artifact
