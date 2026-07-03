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

    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyDashPatternEditor"));

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  presetCombo_ = new PropertyComboBox(this);
  presetCombo_->addItem(QStringLiteral("Solid"), QString());
  presetCombo_->addItem(QStringLiteral("Dotted"), QStringLiteral("2,4"));
  presetCombo_->addItem(QStringLiteral("Dashed"), QStringLiteral("6,3"));
  presetCombo_->addItem(QStringLiteral("Dash-Dot"), QStringLiteral("6,3,2,3"));
  presetCombo_->addItem(QStringLiteral("Dash-Dot-Dot"), QStringLiteral("6,3,2,3,2,3"));
  presetCombo_->addItem(QStringLiteral("Custom"), QString());
  presetCombo_->setMinimumHeight(22);
  applyPropertyFieldPalette(presetCombo_, true);

  customEdit_ = new QLineEdit(this);
  customEdit_->setPlaceholderText(QStringLiteral("e.g. 4,2"));
  customEdit_->setMinimumHeight(22);
  customEdit_->setEnabled(false);
  applyPropertyFieldPalette(customEdit_, true);

  layout->addWidget(presetCombo_, 1);
  layout->addWidget(customEdit_, 1);

  QObject::connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
    const QString pattern = presetCombo_->itemData(idx).toString();
    customEdit_->setEnabled(pattern.isEmpty() && idx == presetCombo_->count() - 1);
    if (!pattern.isEmpty()) {
      customEdit_->setText(pattern);
      commitValue(pattern);
    }
  });

  QObject::connect(customEdit_, &QLineEdit::editingFinished, this, [this]() {
    commitValue(customEdit_->text());
  });

  setValueFromVariant(property.getValue());
}

QVariant ArtifactDashPatternPropertyEditor::value() const {
  if (!customEdit_) return {};
  return customEdit_->text();
}

void ArtifactDashPatternPropertyEditor::setValueFromVariant(const QVariant &value) {
  const QString pattern = value.toString();
  customEdit_->setText(pattern);
  for (int i = 0; i < presetCombo_->count(); ++i) {
    if (presetCombo_->itemData(i).toString() == pattern) {
      presetCombo_->setCurrentIndex(i);
      customEdit_->setEnabled(false);
      return;
    }
  }
  if (!pattern.isEmpty()) {
    presetCombo_->setCurrentIndex(presetCombo_->count() - 1);
    customEdit_->setEnabled(true);
  } else {
    presetCombo_->setCurrentIndex(0);
    customEdit_->setEnabled(false);
  }
}

void ArtifactDashPatternPropertyEditor::applyPreset(const QString& pattern) {
  customEdit_->setText(pattern);
  commitValue(pattern);
}

ArtifactRotationPropertyEditor::ArtifactRotationPropertyEditor(
} // namespace Artifact
