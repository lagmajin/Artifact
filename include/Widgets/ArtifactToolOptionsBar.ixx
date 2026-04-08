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

signals:
  void optionChanged(const QString &toolName, const QString &optionName,
                     const QVariant &value) W_SIGNAL(optionChanged, toolName, optionName, value);

private:
  class Impl;
  Impl *impl_;
};

} // namespace Artifact
