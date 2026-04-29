module;
#include <utility>

#include <wobjectdefs.h>
#include <QString>
#include <QWidget>
#include <QVariant>
export module Widgets.ToolOptionsBar;


export namespace Artifact {

// ツール固有のオプションを表示するセカンダリツールバー
class ArtifactToolOptionsBar : public QWidget {
  W_OBJECT(ArtifactToolOptionsBar)

public:
  explicit ArtifactToolOptionsBar(QWidget *parent = nullptr);
  ~ArtifactToolOptionsBar();

  // ツール名に応じてオプションパネルを切替
  void setCurrentTool(const QString &toolName);
  void setTextOptions(const QString &fontFamily, int fontSize, bool bold,
                      bool italic, bool underline, int horizontalAlignment,
                      int verticalAlignment, int wrapMode, int layoutMode,
                      bool enabled = true);
  void clearTextOptions();
  void setShapeOptions(int shapeType, int width, int height, bool fillEnabled,
                       bool strokeEnabled, int strokeWidth, int cornerRadius,
                       int starPoints, int starInnerRadiusPercent,
                       int polygonSides, bool enabled = true);
  void clearShapeOptions();

signals:
  void optionChanged(const QString &toolName, const QString &optionName,
                     const QVariant &value) W_SIGNAL(optionChanged, toolName, optionName, value);

private:
  class Impl;
  Impl *impl_;
};

} // namespace Artifact
