module;
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>
#include <QWidget>
#include <memory>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.RelativeSpinBox;

namespace Artifact {
using namespace detail;

ArtifactRotationPropertyEditor::ArtifactRotationPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyRotationEditor"));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto *knob = new PropertyRotationKnobWidget(this);
  knob_ = knob;
  knob->setFixedSize(36, 36);
  applyPropertyFieldPalette(knob);

  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  const auto meta = property.metadata();
  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1000000.0,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1000000.0);
  spinBox_->setValue(property.getValue().toDouble());
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toDouble());
  } else {
    spinBox_->setSingleStep(1.0);
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  spinBox_->setMinimumHeight(22);
  spinBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }

  layout->addWidget(spinBox_, 0);
  layout->addWidget(knob_, 0, Qt::AlignHCenter);

  knob->setValue(spinBox_->value());
  knob->setPreviewHandler([this](const double value) {
    if (!spinBox_) {
      return;
    }
    const QSignalBlocker blocker(spinBox_);
    spinBox_->setValue(value);
    previewValue(value);
  });
  knob->setCommitHandler([this](const double value) {
    if (!spinBox_) {
      return;
    }
    const QSignalBlocker blocker(spinBox_);
    spinBox_->setValue(value);
    commitValue(value);
  });

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, knob](const double nextValue) {
                     const QSignalBlocker blocker(knob);
                     knob->setValue(nextValue);
                     if (spinBox_->hasFocus()) {
                       previewValue(nextValue);
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
}

QVariant ArtifactRotationPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactRotationPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const double nextValue = value.toDouble();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (auto *knob = static_cast<PropertyRotationKnobWidget *>(knob_)) {
    const QSignalBlocker knobBlocker(knob);
    knob->setValue(nextValue);
  }
}

bool ArtifactRotationPropertyEditor::supportsScrub() const { return true; }

void ArtifactRotationPropertyEditor::scrubByPixels(
    const int deltaPixels, const Qt::KeyboardModifiers modifiers) {
  if (!spinBox_) {
    return;
  }
  double sensitivity = 0.5;
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    sensitivity *= 0.2;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    sensitivity *= 4.0;
  }
  spinBox_->setValue(spinBox_->value() +
                     static_cast<double>(deltaPixels) * sensitivity);
}

QWidget *ArtifactRotationPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }
  auto *spinBox = static_cast<ArtifactRelativeDoubleSpinBox *>(spinBox_);
  if (auto *lineEdit = spinBox->scrubLineEdit()) {
    return lineEdit;
  }
  return ArtifactAbstractPropertyEditor::scrubTargetWidget();
}

ArtifactPropertyEditorRowWidget::~ArtifactPropertyEditorRowWidget() = default;
} // namespace Artifact
