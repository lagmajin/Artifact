module;
#include <QFont>
#include <QString>

module Artifact.Layer.Text;
import Font.FreeFont;
import Text.Style;

namespace Artifact {
QFont makeTextFont(const ArtifactCore::TextStyle& style,
                   const QString& sampleText) {
  QFont font = ArtifactCore::FontManager::makeFont(style, sampleText);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikethrough);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
  return font;
}
} // namespace Artifact
