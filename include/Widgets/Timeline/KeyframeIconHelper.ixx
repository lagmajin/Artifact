module;
#include <QColor>
#include <QIcon>
#include <QSize>
#include <QString>
export module Artifact.Widgets.Timeline.KeyframeIconHelper;

export namespace Artifact {

enum class KeyframeIconState {
  Normal,
  Selected,
  Locked,
  Disabled
};

enum class KeyframeIconMeaning {
  Normal,
  Hold,
  Ease,
  Auto,
  Manual,
  Dummy
};

struct KeyframeIconStyle {
  QSize size = QSize(14, 14);
  QColor fillColor = QColor(QStringLiteral("#FFD84D"));
  QColor outlineColor = QColor(QStringLiteral("#FFF1A8"));
  KeyframeIconState state = KeyframeIconState::Normal;
  KeyframeIconMeaning meaning = KeyframeIconMeaning::Normal;
  bool currentFrame = false;
};

QIcon makeKeyframeIcon(const KeyframeIconStyle& style);
QIcon cachedKeyframeIcon(const KeyframeIconStyle& style);

} // namespace Artifact
