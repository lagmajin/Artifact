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
import Artifact.Widgets.ObjectPicker;
import Artifact.Service.Project;

namespace Artifact {
using namespace detail;

ArtifactObjectReferencePropertyEditor::ArtifactObjectReferencePropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  Q_UNUSED(property);

  referenceWidget_ = new QWidget(this);
  auto *layout = new QHBoxLayout(referenceWidget_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto *caption = new QLabel(QStringLiteral("Object Reference"), referenceWidget_);
  valueLabel_ = new QLabel(referenceWidget_);
  pickButton_ = new PropertyCallbackButton(QStringLiteral("Pick"), referenceWidget_);
  clearButton_ = new PropertyCallbackButton(QStringLiteral("Clear"), referenceWidget_);

  layout->addWidget(caption, 0);
  layout->addWidget(valueLabel_, 1);
  layout->addWidget(pickButton_, 0);
  layout->addWidget(clearButton_, 0);

  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(referenceWidget_);

  static_cast<PropertyCallbackButton *>(pickButton_)->setCallback(
      [this]() { onReferencePicked(); });
  static_cast<PropertyCallbackButton *>(clearButton_)->setCallback(
      [this]() { onReferenceChanged(-1); });

  clearButton_->setEnabled(false);
  updateReferenceDisplay();
}

QVariant ArtifactObjectReferencePropertyEditor::value() const {
  return QVariant::fromValue<qint64>(currentId_);
}

void ArtifactObjectReferencePropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (value.canConvert<qint64>()) {
    currentId_ = value.toLongLong();
  } else {
    currentId_ = -1;
  }
  updateReferenceDisplay();
}

void ArtifactObjectReferencePropertyEditor::onReferencePicked() {
  ArtifactObjectPickerDialog dialog(this);
  dialog.setCurrentSelectionId(currentId_ < 0
                                   ? ArtifactCore::Id::Nil()
                                   : ArtifactCore::Id(QString::number(currentId_)));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString selectedIdText = dialog.selectedId().toString();
  bool ok = false;
  const qint64 selectedId = selectedIdText.toLongLong(&ok);
  if (!ok) {
    onReferenceChanged(static_cast<qint64>(qHash(selectedIdText)));
    return;
  }

  onReferenceChanged(selectedId);
}

void ArtifactObjectReferencePropertyEditor::onReferenceChanged(qint64 newId) {
  currentId_ = newId;
  updateReferenceDisplay();
  commitValue(value());
}

void ArtifactObjectReferencePropertyEditor::updateReferenceDisplay() {
  if (!valueLabel_) {
    return;
  }

  if (currentId_ < 0) {
    valueLabel_->setText(QStringLiteral("None"));
    if (clearButton_) {
      clearButton_->setEnabled(false);
    }
    return;
  }

  QString displayText = QString::number(currentId_);
  auto *service = ArtifactProjectService::instance();
  if (service) {
    auto composition = service->currentComposition().lock();
    if (composition) {
      const auto layer = composition->layerById(
          ArtifactCore::LayerID(ArtifactCore::Id(QString::number(currentId_))));
      if (layer) {
        displayText = QStringLiteral("%1 (%2)")
                          .arg(layer->layerName(), QString::number(currentId_));
      }
    }
  }

  valueLabel_->setText(displayText);
  if (clearButton_) {
    clearButton_->setEnabled(true);
  }
}
} // namespace Artifact
