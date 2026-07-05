module;
#include <QAbstractButton>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.RelativeSpinBox;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import FloatColorPickerDialog;
import Utils.Path;

namespace Artifact {
QVariant getPropertyDefaultValue(const ArtifactCore::AbstractProperty &property);
void installSliderJumpBehavior(QWidget *pickerRoot);
} // namespace Artifact

namespace Artifact::detail {
extern const int kNumericEditorValueWidth;
extern ArtifactNumericEditorLayoutMode g_numericEditorLayoutMode;
void applyPropertyFieldPalette(QWidget *widget, bool elevated);
void applyPropertyButtonPalette(QAbstractButton *button, bool accent);
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

bool isScalePercentProperty(const ArtifactCore::AbstractProperty &property) {
  const QString name = property.getName();
  return name.compare(QStringLiteral("transform.scale.x"),
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

} // namespace

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  auto initializing = std::make_shared<bool>(true);
  const bool displayAsPercent = isScalePercentProperty(property);
  setObjectName(QStringLiteral("propertyFloatEditor"));
  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  spinBox_->setProperty("displayAsPercent", displayAsPercent);
  if (showSlider) {
    slider_ = new QSlider(Qt::Horizontal, this);
    applyPropertyFieldPalette(slider_);
  }
  QPushButton *resetButton = nullptr;
  if (::Artifact::artifactShouldShowPropertyResetButtons()) {
    resetButton = new QPushButton(QStringLiteral("⟲"), this);
    resetButton->setObjectName(QStringLiteral("propertyResetButton"));
    resetButton->setFixedSize(20, 20);
    resetButton->setToolTip(QStringLiteral("Reset to default"));
    resetButton->setFlat(true);
    applyPropertyButtonPalette(resetButton);

    QObject::connect(resetButton, &QPushButton::clicked, this,
                     [this, property, initializing]() {
                       if (*initializing) {
                         return;
                       }
                       const QVariant defaultValue =
                           getPropertyDefaultValue(property);
                       const double defaultNumericValue = storageToDisplayValue(
                           defaultValue.toDouble(),
                           spinBox_ != nullptr &&
                               spinBox_->property("displayAsPercent").toBool());
                       if (spinBox_) {
                         spinBox_->setValue(defaultNumericValue);
                       }
                        if (slider_) {
                          const QSignalBlocker blocker(slider_);
                          slider_->setValue(floatToSliderPosition(
                              defaultNumericValue, softMin_, softMax_));
                        }
                        commitCurrentValue();
                      });
  }

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
  }

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, sliderUnit, displayAsPercent](const double nextValue) {
                      if (slider_) {
                        const QSignalBlocker blocker(slider_);
                        slider_->setValue(
                            floatToSliderPosition(nextValue, softMin_, softMax_));
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
                     commitValue(displayToStorageValue(spinBox_->value(),
                                                       displayAsPercent));
                   });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this, [this]() {
      sliderInteracting_ = true;
      previewCurrentValue();
    });

    QObject::connect(
        slider_, &QSlider::valueChanged, this,
        [this, sliderUnit, displayAsPercent](const int sliderValue) {
          const double nextValue =
              this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
          const QSignalBlocker blocker(spinBox_);
          spinBox_->setValue(nextValue);
          if (sliderInteracting_) {
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
  }
}

bool ArtifactFloatPropertyEditor::supportsScrub() const { return true; }

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
  auto initializing = std::make_shared<bool>(true);
  setObjectName(QStringLiteral("propertyIntEditor"));
  spinBox_ = new ArtifactRelativeSpinBox(this);
  if (showSlider) {
    slider_ = new QSlider(Qt::Horizontal, this);
    applyPropertyFieldPalette(slider_);
  }
  QPushButton *resetButton = nullptr;
  if (::Artifact::artifactShouldShowPropertyResetButtons()) {
    resetButton = new QPushButton(QStringLiteral("⟲"), this);
    resetButton->setObjectName(QStringLiteral("propertyResetButton"));
    resetButton->setFixedSize(20, 20);
    resetButton->setToolTip(QStringLiteral("Reset to default"));
    resetButton->setFlat(true);
    applyPropertyButtonPalette(resetButton);
  }
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

  // ✅ リセットボタンを一番右に追加
  if (resetButton) {
    layout->addWidget(resetButton, 0);
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
  }

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this, sliderUnit](const int nextValue) {
        if (slider_) {
          const QSignalBlocker blocker(slider_);
          slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
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
                     commitValue(spinBox_->value());
                   });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this, [this]() {
      sliderInteracting_ = true;
      previewCurrentValue();
    });
    QObject::connect(slider_, &QSlider::valueChanged, this,
                     [this, sliderUnit](const int sliderValue) {
                       const int nextValue =
                           sliderPositionToInt(sliderValue, softMin_, softMax_);
                       const QSignalBlocker blocker(spinBox_);
                       spinBox_->setValue(nextValue);
                       if (sliderInteracting_) {
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
