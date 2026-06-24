module;
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QWidget>
#include <QPaintEvent>

export module Widgets.StyleSurface;

export namespace Artifact {

enum class ArtifactTextTone
{
  Default,
  Gold,
  MutedGold,
  Accent,
  Success,
  Warning,
  Danger,
  Muted,
};

struct ArtifactSoftGradientBackground
{
  QColor topLeft = QColor(QStringLiteral("#F4F3F0"));
  QColor center = QColor(QStringLiteral("#E4E2DE"));
  QColor bottomRight = QColor(QStringLiteral("#CFCBC7"));
  QColor highlight = QColor(255, 255, 255, 76);
  QColor lowerShade = QColor(0, 0, 0, 26);
  QColor sideShade = QColor(96, 92, 86, 24);
  qreal highlightX = 0.16;
  qreal highlightY = 0.12;
  qreal highlightRadius = 0.72;
};

QStringList artifactPreferredDisplayFontFamilies();
QString artifactPreferredDisplayFontFamily();
QFont artifactWideDisplayFont(qreal pointSize, int weight = QFont::DemiBold,
                              qreal letterSpacing = 2.4);
QStringList artifactPreferredJapaneseFontFamilies();
QString artifactPreferredJapaneseFontFamily();
QFont artifactJapaneseUIFont(qreal pointSize, int weight = QFont::Normal,
                             qreal letterSpacing = 0.0);
QFont artifactJapaneseSectionFont(qreal pointSize, int weight = QFont::DemiBold,
                                  qreal letterSpacing = 0.6);

ArtifactSoftGradientBackground artifactSpitfireDiscoverBackground();
void drawArtifactSoftGradientBackground(QPainter& painter, const QRectF& rect,
                                        const ArtifactSoftGradientBackground& background =
                                            ArtifactSoftGradientBackground{});

class ArtifactFramedToolButton final : public QToolButton
{
public:
  explicit ArtifactFramedToolButton(QWidget* parent = nullptr);
};

class ArtifactToneLabel final : public QLabel
{
public:
  explicit ArtifactToneLabel(QWidget* parent = nullptr);

  void setTone(ArtifactTextTone tone);
  ArtifactTextTone tone() const;

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  ArtifactTextTone tone_ = ArtifactTextTone::Default;
};

}
