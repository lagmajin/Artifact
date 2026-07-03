module;
#include <QHBoxLayout>
#include <QAbstractButton>
#include <QSignalBlocker>
#include <QVariant>

module Artifact.Widgets.PropertyEditor;

namespace Artifact {

ArtifactBoolPropertyEditor::ArtifactBoolPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyBoolEditor"));
  toggleSwitch_ = new ArtifactToggleSwitch(this);
  applyPropertyFieldPalette(toggleSwitch_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(toggleSwitch_, 0, Qt::AlignLeft | Qt::AlignVCenter);
  layout->addStretch();

  toggleSwitch_->setChecked(property.getValue().toBool());
  QObject::connect(toggleSwitch_, &QAbstractButton::toggled, this,
                   [this](const bool checked) { commitValue(checked); });
}

QVariant ArtifactBoolPropertyEditor::value() const {
  return toggleSwitch_ ? QVariant(toggleSwitch_->isChecked()) : QVariant();
}

void ArtifactBoolPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!toggleSwitch_) {
    return;
  }
  const QSignalBlocker blocker(toggleSwitch_);
  toggleSwitch_->setChecked(value.toBool());
}

} // namespace Artifact
