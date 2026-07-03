module;
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QFrame>
#include <QSignalBlocker>
#include <QEvent>
#include <QWidget>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Font.FreeFont;
import Artifact.Layer.Text;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Widgets.FontPicker;

namespace Artifact::detail {
void applyPropertyFieldPalette(QWidget *widget, bool elevated = false);
void applyPropertyButtonPalette(QAbstractButton *button, bool accent = false);
void applyThemeTextPalette(QWidget *widget, int shade = 100);
} // namespace Artifact::detail

namespace Artifact {
using namespace detail;

ArtifactStringPropertyEditor::ArtifactStringPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyStringEditor"));
  lineEdit_ = new QLineEdit(property.getValue().toString(), this);
  lineEdit_->setMinimumHeight(26);
  lineEdit_->setFrame(false);
  applyPropertyFieldPalette(lineEdit_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(lineEdit_);

  QObject::connect(lineEdit_, &QLineEdit::editingFinished, this,
                   [this]() { commitValue(lineEdit_->text()); });
}

QVariant ArtifactStringPropertyEditor::value() const {
  return lineEdit_ ? QVariant(lineEdit_->text()) : QVariant();
}

void ArtifactStringPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!lineEdit_) {
    return;
  }
  const QSignalBlocker blocker(lineEdit_);
  lineEdit_->setText(value.toString());
}


ArtifactMultilineStringPropertyEditor::ArtifactMultilineStringPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyMultilineStringEditor"));
  textEdit_ = new QTextEdit(this);
  textEdit_->setAcceptRichText(false);
  textEdit_->setMinimumHeight(72);
  textEdit_->setTabChangesFocus(true);
  textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
  textEdit_->setFrameStyle(QFrame::NoFrame);
  applyPropertyFieldPalette(textEdit_, true);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(textEdit_);

  setValueFromVariant(property.getValue());

  QObject::connect(textEdit_, &QTextEdit::textChanged, this,
                   [this]() { previewValue(textEdit_->toPlainText()); });
  textEdit_->installEventFilter(this);
}

QVariant ArtifactMultilineStringPropertyEditor::value() const {
  return textEdit_ ? QVariant(textEdit_->toPlainText()) : QVariant();
}

void ArtifactMultilineStringPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (!textEdit_) {
    return;
  }
  const QSignalBlocker blocker(textEdit_);
  textEdit_->setPlainText(value.toString());
}

bool ArtifactMultilineStringPropertyEditor::eventFilter(QObject *watched,
                                                        QEvent *event) {
  if (watched == textEdit_ && event->type() == QEvent::FocusOut) {
    commitCurrentValue();
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}


ArtifactFontFamilyPropertyEditor::ArtifactFontFamilyPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFontEditor"));
  fontPicker_ = new FontPickerWidget(this);
  applyPropertyFieldPalette(fontPicker_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(fontPicker_);

  setValueFromVariant(property.getValue());
  if (false) {
    QObject::connect(fontPicker_, &FontPickerWidget::fontChanged, this,
                     [this](const QString &family) {
                       commitValue(
                           ArtifactCore::FontManager::resolvedFamily(family));
                     });
  }

  fontChangeSubscription_ =
      ArtifactCore::globalEventBus().subscribe<FontChangedEvent>(
          [this](const FontChangedEvent &ev) {
            commitValue(ArtifactCore::FontManager::resolvedFamily(ev.fontName));
          });
}

QVariant ArtifactFontFamilyPropertyEditor::value() const {
  return fontPicker_ ? QVariant(fontPicker_->currentFont()) : QVariant();
}

void ArtifactFontFamilyPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (!fontPicker_) {
    return;
  }
  const QString family =
      ArtifactCore::FontManager::resolvedFamily(value.toString());
  fontPicker_->setCurrentFont(family);
}


} // namespace Artifact
