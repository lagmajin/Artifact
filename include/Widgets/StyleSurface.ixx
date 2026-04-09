module;
#include <QLabel>
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
