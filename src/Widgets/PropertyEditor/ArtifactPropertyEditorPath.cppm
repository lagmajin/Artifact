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
import Utils.Path;
import Artifact.Service.Project;

namespace Artifact {
using namespace detail;

ArtifactPathPropertyEditor::ArtifactPathPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyPathEditor"));
  lineEdit_ = new QLineEdit(property.getValue().toString(), this);
  browseButton_ = new QPushButton(QStringLiteral("..."), this);
  lineEdit_->setMinimumHeight(26);
  lineEdit_->setFrame(false);
  browseButton_->setObjectName(QStringLiteral("propertyPathBrowseButton"));
  browseButton_->setFixedSize(28, 26);
  applyPropertyFieldPalette(lineEdit_);
  applyPropertyButtonPalette(browseButton_);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(lineEdit_, 1);
  layout->addWidget(browseButton_, 0);

  const QString propertyName = property.getName();
  QObject::connect(lineEdit_, &QLineEdit::editingFinished, this,
                   [this]() { commitValue(lineEdit_->text()); });
  QObject::connect(browseButton_, &QPushButton::clicked, this,
                   [this, propertyName]() {
                     const QString initialPath = lineEdit_->text().trimmed();
                     const QString selectedPath = QFileDialog::getOpenFileName(
                         this, QStringLiteral("Select Source"), initialPath,
                         fileDialogFilterForProperty(propertyName));
                     if (selectedPath.isEmpty()) {
                       return;
                     }
                     lineEdit_->setText(selectedPath);
                     commitValue(selectedPath);
                   });
}

QVariant ArtifactPathPropertyEditor::value() const {
  return lineEdit_ ? QVariant(lineEdit_->text()) : QVariant();
}

void ArtifactPathPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!lineEdit_) {
    return;
  }
  const QSignalBlocker blocker(lineEdit_);
  lineEdit_->setText(value.toString());
}

} // namespace Artifact
