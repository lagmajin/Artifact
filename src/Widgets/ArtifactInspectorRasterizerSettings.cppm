module;

#include <QAbstractButton>
#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPointF>
#include <QPushButton>
#include <QRadioButton>
#include <QRectF>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

export module Artifact.Widgets.InspectorRasterizerSettings;

import Artifact.Widgets.InspectorStyle;
import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

namespace detail {

enum class RasterizerInitialSettingsMode {
  KeepDefaults,
  FitToSource,
  AskWhenAdding,
};

constexpr auto kRasterizerInitialSettingsKey =
    "Effects/RasterizerInitialSettingsMode";

RasterizerInitialSettingsMode rasterizerInitialSettingsModeFromValue(
    const QString &value) {
  if (value == QStringLiteral("fit")) {
    return RasterizerInitialSettingsMode::FitToSource;
  }
  if (value == QStringLiteral("defaults")) {
    return RasterizerInitialSettingsMode::KeepDefaults;
  }
  return RasterizerInitialSettingsMode::AskWhenAdding;
}

QString rasterizerInitialSettingsModeValue(
    const RasterizerInitialSettingsMode mode) {
  switch (mode) {
  case RasterizerInitialSettingsMode::KeepDefaults:
    return QStringLiteral("defaults");
  case RasterizerInitialSettingsMode::FitToSource:
    return QStringLiteral("fit");
  case RasterizerInitialSettingsMode::AskWhenAdding:
    return QStringLiteral("ask");
  }
  return QStringLiteral("ask");
}

RasterizerInitialSettingsMode rasterizerInitialSettingsModeFromSettings() {
  QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
  return rasterizerInitialSettingsModeFromValue(
      settings.value(QString::fromLatin1(kRasterizerInitialSettingsKey),
                     QStringLiteral("ask"))
          .toString());
}

class EffectSetupDescription final : public QLabel {
 public:
  using QLabel::QLabel;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.setFont(font());
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter |
                                 Qt::TextWordWrap,
                     text());
  }
};

void paintEffectSetupChoice(QAbstractButton* button, QPainter& painter,
                            const bool radio) {
  if (!button) return;
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  const QPalette pal = button->palette();
  const QColor accent = pal.color(QPalette::Highlight);
  const QRectF indicatorRect(3.5, button->height() / 2.0 - 6.0, 12.0, 12.0);
  painter.setPen(button->hasFocus() ? accent : pal.color(QPalette::Mid));
  painter.setBrush(button->isChecked() ? accent : pal.color(QPalette::Base));
  if (radio) {
    painter.drawEllipse(indicatorRect);
    if (button->isChecked()) {
      painter.setBrush(pal.color(QPalette::HighlightedText));
      painter.setPen(Qt::NoPen);
      painter.drawEllipse(indicatorRect.adjusted(4, 4, -4, -4));
    }
  } else {
    painter.drawRoundedRect(indicatorRect, 2.0, 2.0);
    if (button->isChecked()) {
      painter.setPen(pal.color(QPalette::HighlightedText));
      painter.drawLine(QPointF(6.5, button->height() / 2.0),
                       QPointF(9.5, button->height() / 2.0 + 3.0));
      painter.drawLine(QPointF(9.5, button->height() / 2.0 + 3.0),
                       QPointF(14.0, button->height() / 2.0 - 3.0));
    }
  }
  painter.setPen(button->isEnabled()
                     ? pal.color(QPalette::WindowText)
                     : pal.color(QPalette::Disabled, QPalette::WindowText));
  painter.drawText(button->rect().adjusted(23, 0, -4, 0),
                   Qt::AlignLeft | Qt::AlignVCenter, button->text());
}

class EffectSetupRadioButton final : public QRadioButton {
 public:
  using QRadioButton::QRadioButton;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    paintEffectSetupChoice(this, painter, true);
  }
};

class EffectSetupCheckBox final : public QCheckBox {
 public:
  using QCheckBox::QCheckBox;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    paintEffectSetupChoice(this, painter, false);
  }
};

class RasterizerInitialSettingsActionButton final : public QPushButton {
public:
  RasterizerInitialSettingsActionButton(const QString &text, QDialog *dialog,
                                        const int dialogResult)
      : QPushButton(text, dialog), dialog_(dialog), dialogResult_(dialogResult) {
    setAttribute(Qt::WA_Hover, true);
    setMinimumHeight(30);
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    const bool primary = dialogResult_ == QDialog::Accepted;
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor base = primary
        ? blendColor(pal.color(QPalette::Button), accent, 0.48)
        : pal.color(QPalette::Button);
    painter.setPen(primary ? accent : pal.color(QPalette::Mid));
    painter.setBrush(isDown() ? blendColor(base, accent, 0.30)
                              : underMouse() ? blendColor(base, accent, 0.14)
                                             : base);
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    painter.setPen(isEnabled() ? pal.color(QPalette::ButtonText)
                               : pal.color(QPalette::Disabled,
                                           QPalette::ButtonText));
    painter.drawText(rect(), Qt::AlignCenter, text());
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    const bool activate = event && event->button() == Qt::LeftButton &&
                          rect().contains(event->position().toPoint());
    QPushButton::mouseReleaseEvent(event);
    if (activate && dialog_) {
      dialog_->done(dialogResult_);
    }
  }

  void keyReleaseEvent(QKeyEvent* event) override {
    const bool activate = event && isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter);
    QPushButton::keyReleaseEvent(event);
    if (activate && dialog_) {
      dialog_->done(dialogResult_);
    }
  }

private:
  QDialog *dialog_ = nullptr;
  int dialogResult_ = QDialog::Rejected;
};

class RasterizerInitialSettingsDialog final : public QDialog {
public:
  RasterizerInitialSettingsDialog(const QSize &sourceSize, QWidget *parent)
      : QDialog(parent) {
    setWindowTitle(QStringLiteral("Rasterizer Effect Initial Settings"));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    auto *description = new EffectSetupDescription(
        QStringLiteral("Choose how to initialize pixel-based settings for this effect. "
                       "Auto Fit uses the source size (%1 x %2) only once; it will not "
                       "change the effect after it is added.")
            .arg(sourceSize.width())
            .arg(sourceSize.height()),
        this);
    description->setWordWrap(true);
    applyInspectorLabelPalette(description, false);
    layout->addWidget(description);

    defaultsButton_ = new EffectSetupRadioButton(
        QStringLiteral("Keep effect defaults"), this);
    fitButton_ = new EffectSetupRadioButton(
        QStringLiteral("Auto Fit to source size"), this);
    defaultsButton_->setMinimumHeight(28);
    fitButton_->setMinimumHeight(28);
    defaultsButton_->setChecked(true);
    layout->addWidget(defaultsButton_);
    layout->addWidget(fitButton_);

    rememberChoice_ = new EffectSetupCheckBox(
        QStringLiteral("Use this choice automatically for future rasterizer effects"),
        this);
    rememberChoice_->setMinimumHeight(28);
    layout->addWidget(rememberChoice_);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancelButton = new RasterizerInitialSettingsActionButton(
        QStringLiteral("Cancel"), this, QDialog::Rejected);
    auto *addButton = new RasterizerInitialSettingsActionButton(
        QStringLiteral("Add Effect"), this, QDialog::Accepted);
    applyInspectorButton(cancelButton, false);
    applyInspectorButton(addButton, true);
    buttons->addWidget(cancelButton);
    buttons->addWidget(addButton);
    layout->addLayout(buttons);
  }

  RasterizerInitialSettingsMode selectedMode() const {
    return fitButton_ && fitButton_->isChecked()
               ? RasterizerInitialSettingsMode::FitToSource
               : RasterizerInitialSettingsMode::KeepDefaults;
  }

  bool rememberChoice() const {
    return rememberChoice_ && rememberChoice_->isChecked();
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event && (event->key() == Qt::Key_Return ||
                  event->key() == Qt::Key_Enter)) {
      done(QDialog::Accepted);
      return;
    }
    QDialog::keyPressEvent(event);
  }

private:
  QRadioButton *defaultsButton_ = nullptr;
  QRadioButton *fitButton_ = nullptr;
  QCheckBox *rememberChoice_ = nullptr;
};

bool isSourceScaledRasterizerProperty(const QString &name) {
  const QString key = name.toLower();
  return key.contains(QStringLiteral("radius")) ||
         key.contains(QStringLiteral("blur")) ||
         key.contains(QStringLiteral("distance")) ||
         key.contains(QStringLiteral("offset")) ||
         key.contains(QStringLiteral("shift")) ||
         key.contains(QStringLiteral("width")) ||
         key.contains(QStringLiteral("height")) ||
         key.contains(QStringLiteral("size")) ||
         key.contains(QStringLiteral("thickness"));
}

int applyRasterizerSourceFit(ArtifactAbstractEffect *effect,
                             const QSize &sourceSize) {
  if (!effect || sourceSize.width() <= 0 || sourceSize.height() <= 0) {
    return 0;
  }
  const double scale = std::clamp(
      std::sqrt((static_cast<double>(sourceSize.width()) * sourceSize.height()) /
                (1920.0 * 1080.0)),
      0.25, 4.0);
  int adjustedCount = 0;
  for (const auto &property : effect->getProperties()) {
    const QString name = property.getName();
    const QVariant value = property.getValue();
    if (!isSourceScaledRasterizerProperty(name) ||
        (property.getType() != PropertyType::Float &&
         property.getType() != PropertyType::Integer) ||
        !value.isValid()) {
      continue;
    }
    const double scaled = std::clamp(
        value.toDouble() * scale, property.getMinValue().isValid()
                                      ? property.getMinValue().toDouble()
                                      : -std::numeric_limits<double>::max(),
        property.getMaxValue().isValid()
            ? property.getMaxValue().toDouble()
            : std::numeric_limits<double>::max());
    effect->setPropertyValue(UniString::fromQString(name),
                             property.getType() == PropertyType::Integer
                                 ? QVariant(qRound(scaled))
                                 : QVariant(scaled));
    ++adjustedCount;
  }
  return adjustedCount;
}

} // namespace detail

} // namespace Artifact
