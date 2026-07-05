module;
#include <QColor>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>
#include <QTextEdit>
#include <QWidget>
#include <memory>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.RelativeSpinBox;
import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Utils.Path;

namespace Artifact {
using namespace detail;

ArtifactTextAnimatorColorEditor::ArtifactTextAnimatorColorEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyTextAnimatorColorEditor"));

  textEdit_ = new QTextEdit(this);
  textEdit_->setAcceptRichText(false);
  textEdit_->setMinimumHeight(72);
  textEdit_->setTabChangesFocus(true);
  textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
  textEdit_->setFrameStyle(QFrame::NoFrame);
  applyPropertyFieldPalette(textEdit_, true);

  colorButton_ = new QPushButton(QStringLiteral(" "), this);
  colorButton_->setObjectName(QStringLiteral("propertyColorSwatchButton"));
  colorButton_->setFixedSize(36, 24);
  colorButton_->setToolTip(QStringLiteral("Apply color to selected text range"));
  colorButton_->hide();
  applyPropertyButtonPalette(colorButton_, true);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(textEdit_);
  layout->addWidget(colorButton_, 0, Qt::AlignTop);

  setValueFromVariant(property.getValue());

  QObject::connect(textEdit_, &QTextEdit::textChanged, this,
                   [this]() { previewValue(textEdit_->toPlainText()); });
  QObject::connect(textEdit_, &QTextEdit::selectionChanged, this,
                   &ArtifactTextAnimatorColorEditor::onSelectionChanged);
  QObject::connect(colorButton_, &QPushButton::clicked, this,
                   &ArtifactTextAnimatorColorEditor::onColorPicked);
  textEdit_->installEventFilter(this);
}

QVariant ArtifactTextAnimatorColorEditor::value() const {
  return textEdit_ ? QVariant(textEdit_->toPlainText()) : QVariant();
}

void ArtifactTextAnimatorColorEditor::setValueFromVariant(
    const QVariant &value) {
  if (!textEdit_) return;
  const QSignalBlocker blocker(textEdit_);
  textEdit_->setPlainText(value.toString());
}

bool ArtifactTextAnimatorColorEditor::eventFilter(QObject *watched,
                                                    QEvent *event) {
  if (watched == textEdit_ && event->type() == QEvent::FocusOut) {
    commitCurrentValue();
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}

void ArtifactTextAnimatorColorEditor::onSelectionChanged() {
  if (!textEdit_) return;
  const QTextCursor cursor = textEdit_->textCursor();
  const bool hasSelection = cursor.hasSelection();
  colorButton_->setVisible(hasSelection);
}

void ArtifactTextAnimatorColorEditor::onColorPicked() {
  if (!textEdit_ || !layer_) return;

  const QTextCursor cursor = textEdit_->textCursor();
  if (!cursor.hasSelection()) return;

  const int selStart = cursor.selectionStart();
  const int selEnd = cursor.selectionEnd();
  if (selEnd <= selStart) return;

  ArtifactWidgets::FloatColorPicker picker(colorButton_);
  picker.setWindowTitle(QStringLiteral("Select Text Range Color"));
  picker.setInitialColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
  if (picker.exec() != QDialog::Accepted) return;

  const ArtifactCore::FloatColor picked = picker.getColor();
  const QColor qColor =
      QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
  if (!qColor.isValid()) return;

  const int animIdx = layer_->applyColorToSelectorRange(
      selStart, selEnd,
      ArtifactCore::FloatRGBA(picked.r(), picked.g(), picked.b(), picked.a()));
  if (animIdx >= 0) {
    colorButton_->hide();
    Q_EMIT colorApplied(selStart, selEnd, qColor);
  }
}

ArtifactAbstractPropertyEditor *
createPropertyEditorWidget(const ArtifactCore::AbstractProperty &property,
                           QWidget *parent) {
  if (detail::isMultilineTextProperty(property)) {
    return new ArtifactTextAnimatorColorEditor(property, parent);
  }
  if (detail::isFontFamilyProperty(property)) {
    return new ArtifactFontFamilyPropertyEditor(property, parent);
  }
  if (detail::isPathProperty(property)) {
    return new ArtifactPathPropertyEditor(property, parent);
  }
  if (const auto enumOptions = detail::enumOptionsForProperty(property)) {
    return new ArtifactEnumPropertyEditor(property, *enumOptions, parent);
  }
  if (property.getType() == ArtifactCore::PropertyType::Float &&
      property.getName() == QStringLiteral("transform.rotation")) {
    return new ArtifactRotationPropertyEditor(property, parent);
  }
  if (property.getType() == ArtifactCore::PropertyType::Integer &&
      property.getName() == QStringLiteral("text.animatorCount")) {
    return new ArtifactAnimatorCountPropertyEditor(property, parent);
  }
  if (property.getName() == QStringLiteral("shape.dashPattern")) {
    return new ArtifactDashPatternPropertyEditor(property, parent);
  }

  switch (property.getType()) {
  case ArtifactCore::PropertyType::Float:
    return new ArtifactFloatPropertyEditor(
        property, parent, detail::shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Integer:
    return new ArtifactIntPropertyEditor(
        property, parent, detail::shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Boolean:
    return new ArtifactBoolPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::Color:
    return new ArtifactColorPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::String:
    return new ArtifactStringPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::ObjectReference:
    return new ArtifactObjectReferencePropertyEditor(property, parent);
  default:
    return nullptr;
  }
}

} // namespace Artifact
