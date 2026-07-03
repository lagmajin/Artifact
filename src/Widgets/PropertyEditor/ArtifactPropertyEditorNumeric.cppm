module;
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
import FloatColorPickerDialog;
import Utils.Path;

namespace Artifact {
using namespace detail;

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFloatEditor"));
  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  if (showSlider) {
    slider_ = new PropertySliderWidget(this);
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
                     [this, property]() {
                       const QVariant defaultValue =
                           getPropertyDefaultValue(property);
                       const double defaultNumericValue = defaultValue.toDouble();
                       if (spinBox_) {
                         spinBox_->setValue(defaultNumericValue);
                       }
                       if (slider_) {
                         const QSignalBlocker blocker(slider_);
                         slider_->setValue(floatToSliderPosition(
                             defaultNumericValue, softMin_, softMax_));
                         if (auto *propertySlider =
                                 static_cast<PropertySliderWidget *>(slider_)) {
                           propertySlider->setDisplayText(
                               formatNumericSliderText(
                                   defaultNumericValue,
                                   spinBox_ ? spinBox_->suffix().trimmed()
                                            : QString(),
                                   3));
                         }
                       }
                       if (auto *propertyKnob =
                               static_cast<PropertyNumericKnobWidget *>(knob_)) {
                         propertyKnob->setValue(defaultNumericValue);
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
  const double hardMin =
      meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6;
  const double hardMax = meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6;
  const auto [resolvedSoftMin, resolvedSoftMax] =
      resolveFloatSoftRange(property, meta, hardMin, hardMax);
  softMin_ = resolvedSoftMin;
  softMax_ = resolvedSoftMax;
  if (softMax_ <= softMin_) {
    softMin_ = hardMin;
    softMax_ = hardMax;
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setRange(softMin_, softMax_);
    propertyKnob->setValue(property.getValue().toDouble());
    propertyKnob->setPreviewHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(nextValue);
      previewValue(spinBox_->value());
    });
    propertyKnob->setCommitHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(nextValue);
      commitValue(spinBox_->value());
    });
  }

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
  spinBox_->setValue(property.getValue().toDouble());
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toDouble());
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
    slider_->setTracking(true); // ドラッグ中の追従を有効化
    slider_->setValue(floatToSliderPosition(property.getValue().toDouble(),
                                            softMin_, softMax_));
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(property.getValue().toDouble(), meta.unit, 3));
    }
  }

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, sliderUnit](const double nextValue) {
                     if (slider_) {
                       const QSignalBlocker blocker(slider_);
                       slider_->setValue(
                           floatToSliderPosition(nextValue, softMin_, softMax_));
                       if (auto *propertySlider =
                               static_cast<PropertySliderWidget *>(slider_)) {
                         propertySlider->setDisplayText(
                             formatNumericSliderText(nextValue, sliderUnit, 3));
                       }
                     }
                     if (auto *propertyKnob =
                             static_cast<PropertyNumericKnobWidget *>(knob_)) {
                       propertyKnob->setValue(nextValue);
                     }
                     if (spinBox_->hasFocus() && !sliderInteracting_) {
                       previewValue(nextValue);
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this, [this]() {
      sliderInteracting_ = true;
      previewCurrentValue();
    });

    QObject::connect(
        slider_, &QSlider::valueChanged, this, [this, sliderUnit](const int sliderValue) {
          const double nextValue =
              this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
          const QSignalBlocker blocker(spinBox_);
          spinBox_->setValue(nextValue);
          if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
            propertyKnob->setValue(nextValue);
          }
          if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
            propertySlider->setDisplayText(
                formatNumericSliderText(nextValue, sliderUnit, 3));
          }
          if (sliderInteracting_) {
            previewValue(nextValue);
          }
        });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      commitCurrentValue();
    });

    Artifact::installSliderJumpBehavior(this);
  }
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
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactFloatPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const double nextValue = value.toDouble();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (slider_) {
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(this->floatToSliderPosition(nextValue, softMin_, softMax_));
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(nextValue, spinBox_->suffix().trimmed(), 3));
    }
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setValue(nextValue);
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

  double sensitivity = range / 500.0;
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
ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(

ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyIntEditor"));
  spinBox_ = new ArtifactRelativeSpinBox(this);
  if (showSlider) {
    slider_ = new PropertySliderWidget(this);
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
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setRange(static_cast<double>(softMin_),
                           static_cast<double>(softMax_));
    propertyKnob->setValue(static_cast<double>(property.getValue().toInt()));
    propertyKnob->setPreviewHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(static_cast<int>(std::llround(nextValue)));
      previewValue(spinBox_->value());
    });
    propertyKnob->setCommitHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(static_cast<int>(std::llround(nextValue)));
      commitValue(spinBox_->value());
    });
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
          if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(property.getValue().toInt(), meta.unit, 0));
    }
  }

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this, sliderUnit](const int nextValue) {
        if (slider_) {
          const QSignalBlocker blocker(slider_);
          slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
          if (auto *propertySlider =
                  static_cast<PropertySliderWidget *>(slider_)) {
            propertySlider->setDisplayText(
                formatNumericSliderText(nextValue, sliderUnit, 0));
          }
        }
        if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
          propertyKnob->setValue(static_cast<double>(nextValue));
        }
        if (spinBox_->hasFocus() && !sliderInteracting_) {
          previewValue(nextValue);
        }
      });
  QObject::connect(spinBox_, &QSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
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
                       if (auto *propertyKnob =
                               static_cast<PropertyNumericKnobWidget *>(knob_)) {
                         propertyKnob->setValue(static_cast<double>(nextValue));
                       }
                       if (auto *propertySlider =
                               static_cast<PropertySliderWidget *>(slider_)) {
                         propertySlider->setDisplayText(
                             formatNumericSliderText(nextValue, sliderUnit, 0));
                       }
                       if (sliderInteracting_) {
                         previewValue(nextValue);
                       }
                     });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      commitCurrentValue();
    });
    Artifact::installSliderJumpBehavior(this);
  }
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
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(nextValue, spinBox_->suffix().trimmed(), 0));
    }
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setValue(static_cast<double>(nextValue));
  }
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

QWidget *ArtifactIntPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }


ArtifactPathPropertyEditor::ArtifactPathPropertyEditor(
} // namespace Artifact
