module;
#include <QPainter>
#include <QPalette>
#include <QString>

module Widgets.StyleSurface;

import Widgets.Utils.CSS;

namespace Artifact {

namespace {
QColor resolveToneColor(ArtifactTextTone tone)
{
  const auto& theme = ArtifactCore::currentDCCTheme();
  switch (tone) {
  case ArtifactTextTone::Gold:
    return QColor(QStringLiteral("#D4AF37"));
  case ArtifactTextTone::MutedGold:
    return QColor(QStringLiteral("#B8942D"));
  case ArtifactTextTone::Accent:
    return QColor(theme.accentColor);
  case ArtifactTextTone::Success:
    return QColor(QStringLiteral("#55D68A"));
  case ArtifactTextTone::Warning:
    return QColor(QStringLiteral("#F0B44C"));
  case ArtifactTextTone::Danger:
    return QColor(QStringLiteral("#F35B5B"));
  case ArtifactTextTone::Muted:
    return QColor(theme.textColor).darker(140);
  case ArtifactTextTone::Default:
  default:
    return QColor(theme.textColor);
  }
}
} // namespace

ArtifactFramedToolButton::ArtifactFramedToolButton(QWidget* parent)
    : QToolButton(parent)
{
  setAutoRaise(false);
  setProperty("artifactFramedToolButton", true);
}

ArtifactToneLabel::ArtifactToneLabel(QWidget* parent)
    : QLabel(parent)
{
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAutoFillBackground(false);
}

void ArtifactToneLabel::setTone(ArtifactTextTone tone)
{
  if (tone_ == tone) {
    return;
  }
  tone_ = tone;
  update();
}

ArtifactTextTone ArtifactToneLabel::tone() const
{
  return tone_;
}

void ArtifactToneLabel::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  const QWidget* bgSource = parentWidget() ? parentWidget() : this;
  painter.fillRect(rect(), bgSource->palette().window().color());
  painter.setPen(resolveToneColor(tone_));
  painter.setFont(font());
  painter.drawText(rect(), alignment() | Qt::TextWordWrap, text());
}

}
