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

ArtifactEnumPropertyEditor::ArtifactEnumPropertyEditor(
    const ArtifactCore::AbstractProperty &property, OptionList options,
    QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent), options_(std::move(options)) {
  setObjectName(QStringLiteral("propertyEnumEditor"));
  comboBox_ = new PropertyComboBox(this);
  comboBox_->setMinimumHeight(26);
  comboBox_->setFrame(false);
  applyPropertyFieldPalette(comboBox_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(comboBox_);

  for (const auto &[value, label] : options_) {
    comboBox_->addItem(label, value);
  }
  setValueFromVariant(property.getValue());

  QObject::connect(comboBox_, &QComboBox::currentIndexChanged, this,
                   [this](int index) {
                     if (index < 0) {
                       return;
                     }
                     commitValue(comboBox_->currentData());
                   });
}

QVariant ArtifactEnumPropertyEditor::value() const {
  return comboBox_ ? comboBox_->currentData() : QVariant();
}

void ArtifactEnumPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!comboBox_) {
    return;
  }
  const int desired = value.toInt();
  for (int i = 0; i < comboBox_->count(); ++i) {
    if (comboBox_->itemData(i).toInt() == desired) {
      const QSignalBlocker blocker(comboBox_);
      comboBox_->setCurrentIndex(i);
      return;
    }
  }
}

} // namespace Artifact
