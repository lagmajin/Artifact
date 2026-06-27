module;
#include <QApplication>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QRadialGradient>
#include <QString>
#include <QStringList>
#include <algorithm>

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

QColor mixColor(const QColor& a, const QColor& b, const qreal t)
{
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * clamped,
                          a.greenF() + (b.greenF() - a.greenF()) * clamped,
                          a.blueF() + (b.blueF() - a.blueF()) * clamped,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * clamped);
}
} // namespace

QStringList artifactPreferredDisplayFontFamilies()
{
  return {
      QStringLiteral("Avenir Next"),
      QStringLiteral("Montserrat"),
      QStringLiteral("Poppins"),
      QStringLiteral("Nunito Sans"),
      QStringLiteral("Bahnschrift"),
      QStringLiteral("Segoe UI"),
  };
}

QString artifactPreferredDisplayFontFamily()
{
  const QStringList installedFamilies = QFontDatabase::families();
  for (const QString& candidate : artifactPreferredDisplayFontFamilies()) {
    if (installedFamilies.contains(candidate, Qt::CaseInsensitive)) {
      return candidate;
    }
  }
  return QApplication::font().family();
}

QStringList artifactPreferredJapaneseFontFamilies()
{
  return {
      QStringLiteral("M PLUS 1p"),
      QStringLiteral("M PLUS 1"),
      QStringLiteral("M PLUS Rounded 1c"),
      QStringLiteral("Noto Sans JP"),
      QStringLiteral("Yu Gothic UI"),
      QStringLiteral("Meiryo"),
      QStringLiteral("Segoe UI"),
  };
}

QString artifactPreferredJapaneseFontFamily()
{
  const QStringList installedFamilies = QFontDatabase::families();
  for (const QString& candidate : artifactPreferredJapaneseFontFamilies()) {
    if (installedFamilies.contains(candidate, Qt::CaseInsensitive)) {
      return candidate;
    }
  }
  return QApplication::font().family();
}

QFont artifactWideDisplayFont(qreal pointSize, int weight, qreal letterSpacing)
{
  QFont displayFont(artifactPreferredDisplayFontFamily());
  displayFont.setPointSizeF(pointSize);
  displayFont.setWeight(static_cast<QFont::Weight>(weight));
  displayFont.setCapitalization(QFont::AllUppercase);
  displayFont.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
  displayFont.setHintingPreference(QFont::PreferFullHinting);
  return displayFont;
}

QFont artifactJapaneseUIFont(qreal pointSize, int weight, qreal letterSpacing)
{
  QFont uiFont(artifactPreferredJapaneseFontFamily());
  uiFont.setPointSizeF(pointSize);
  uiFont.setWeight(static_cast<QFont::Weight>(weight));
  uiFont.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
  uiFont.setHintingPreference(QFont::PreferFullHinting);
  return uiFont;
}

QFont artifactJapaneseSectionFont(qreal pointSize, int weight, qreal letterSpacing)
{
  QFont sectionFont = artifactJapaneseUIFont(pointSize, weight, letterSpacing);
  sectionFont.setCapitalization(QFont::MixedCase);
  return sectionFont;
}

ArtifactSoftGradientBackground artifactSpitfireDiscoverBackground()
{
  ArtifactSoftGradientBackground background;
  background.topLeft = QColor(QStringLiteral("#F5F4F1"));
  background.center = QColor(QStringLiteral("#E2E0DB"));
  background.bottomRight = QColor(QStringLiteral("#C9C6C1"));
  background.highlight = QColor(255, 255, 255, 84);
  background.lowerShade = QColor(20, 22, 22, 30);
  background.sideShade = QColor(92, 87, 80, 22);
  background.highlightX = 0.14;
  background.highlightY = 0.10;
  background.highlightRadius = 0.78;
  return background;
}

void drawArtifactSoftGradientBackground(QPainter& painter, const QRectF& rect,
                                        const ArtifactSoftGradientBackground& background)
{
  if (rect.isEmpty()) {
    return;
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);

  QLinearGradient base(rect.topLeft(), rect.bottomRight());
  base.setInterpolationMode(QGradient::ComponentInterpolation);
  base.setColorAt(0.0, background.topLeft);
  base.setColorAt(0.28, mixColor(background.topLeft, background.center, 0.58));
  base.setColorAt(0.52, background.center);
  base.setColorAt(0.78, mixColor(background.center, background.bottomRight, 0.60));
  base.setColorAt(1.0, background.bottomRight);
  painter.fillRect(rect, base);

  const qreal longestEdge = std::max(rect.width(), rect.height());
  QRadialGradient highlight(
      QPointF(rect.left() + rect.width() * background.highlightX,
              rect.top() + rect.height() * background.highlightY),
      longestEdge * background.highlightRadius);
  highlight.setInterpolationMode(QGradient::ComponentInterpolation);
  highlight.setColorAt(0.0, background.highlight);
  highlight.setColorAt(0.42, QColor(background.highlight.red(), background.highlight.green(),
                                    background.highlight.blue(),
                                    static_cast<int>(background.highlight.alpha() * 0.58)));
  highlight.setColorAt(0.72, QColor(background.highlight.red(), background.highlight.green(),
                                    background.highlight.blue(),
                                    static_cast<int>(background.highlight.alpha() * 0.30)));
  highlight.setColorAt(1.0, QColor(background.highlight.red(), background.highlight.green(),
                                   background.highlight.blue(), 0));
  painter.fillRect(rect, highlight);

  QLinearGradient lowerShade(rect.left(), rect.top() + rect.height() * 0.45,
                             rect.left(), rect.bottom());
  lowerShade.setInterpolationMode(QGradient::ComponentInterpolation);
  lowerShade.setColorAt(0.0, QColor(background.lowerShade.red(), background.lowerShade.green(),
                                    background.lowerShade.blue(), 0));
  lowerShade.setColorAt(0.48, QColor(background.lowerShade.red(), background.lowerShade.green(),
                                     background.lowerShade.blue(),
                                     static_cast<int>(background.lowerShade.alpha() * 0.55)));
  lowerShade.setColorAt(1.0, background.lowerShade);
  painter.fillRect(rect, lowerShade);

  QLinearGradient sideShade(rect.left(), rect.top(), rect.right(), rect.top());
  sideShade.setInterpolationMode(QGradient::ComponentInterpolation);
  sideShade.setColorAt(0.0, QColor(background.sideShade.red(), background.sideShade.green(),
                                   background.sideShade.blue(), 0));
  sideShade.setColorAt(0.68, QColor(background.sideShade.red(), background.sideShade.green(),
                                    background.sideShade.blue(),
                                    static_cast<int>(background.sideShade.alpha() * 0.52)));
  sideShade.setColorAt(1.0, background.sideShade);
  painter.fillRect(rect, sideShade);

  painter.restore();
}

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
