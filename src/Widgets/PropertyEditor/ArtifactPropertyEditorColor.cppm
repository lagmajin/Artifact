module;
#include <QColor>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPalette>
#include <QVariant>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import FloatColorPickerDialog;
import Utils.Path;

namespace Artifact {
using namespace detail;

ArtifactColorPropertyEditor::ArtifactColorPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyColorEditor"));
  button_ = new QPushButton(QStringLiteral(" "), this);
  valueLabel_ = new QLabel(this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(button_, 0);
  layout->addWidget(valueLabel_, 1);

  button_->setObjectName(QStringLiteral("propertyColorSwatchButton"));
  valueLabel_->setObjectName(QStringLiteral("propertyColorValueLabel"));
  button_->setFixedSize(36, 24);
  applyPropertyButtonPalette(button_, true);
  applyPropertyLabelPalette(valueLabel_);
  currentColor_ = propertyColor(property);
  applyColor(currentColor_);
  QObject::connect(button_, &QPushButton::clicked, this, [this]() {
    ArtifactWidgets::FloatColorPicker picker(button_);
    picker.setWindowTitle(QStringLiteral("Select Color"));
    picker.setInitialColor(ArtifactCore::FloatColor(
        currentColor_.redF(), currentColor_.greenF(), currentColor_.blueF(),
        currentColor_.alphaF()));
    if (picker.exec() != QDialog::Accepted) {
      return;
    }
    const ArtifactCore::FloatColor picked = picker.getColor();
    const QColor nextColor =
        QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
    if (!nextColor.isValid()) {
      return;
    }
    applyColor(nextColor);
    commitValue(nextColor);
  });
}

QVariant ArtifactColorPropertyEditor::value() const {
  return QVariant(currentColor_);
}

void ArtifactColorPropertyEditor::setValueFromVariant(const QVariant &value) {
  QColor nextColor;
  if (value.canConvert<QColor>()) {
    nextColor = value.value<QColor>();
  } else {
    const QString text = value.toString().trimmed();
    if (!text.isEmpty()) {
      nextColor = QColor(text);
    }
  }
  if (nextColor.isValid()) {
    applyColor(nextColor);
  }
}

void ArtifactColorPropertyEditor::applyColor(const QColor &color) {
  currentColor_ = color;
  if (button_) {
    QPalette pal = button_->palette();
    pal.setColor(QPalette::Button, color);
    pal.setColor(QPalette::ButtonText,
                 QColor::fromRgbF(color.redF() > 0.5 ? 0.08 : 0.94,
                                  color.greenF() > 0.5 ? 0.08 : 0.94,
                                  color.blueF() > 0.5 ? 0.08 : 0.94));
    button_->setAutoFillBackground(true);
    button_->setPalette(pal);
  }
  if (valueLabel_) {
    valueLabel_->setText(color.name(QColor::HexArgb).toUpper());
  }
}

} // namespace Artifact
