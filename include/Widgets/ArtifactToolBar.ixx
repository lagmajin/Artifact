module;
#include <QToolBar>
#include <wobjectdefs.h>
export module Widgets.ToolBar;

export namespace Artifact {

 class ArtifactToolBar:public QToolBar{
  W_OBJECT(ArtifactToolBar)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactToolBar(QWidget*parent =nullptr);
  ~ArtifactToolBar();
  void setActionEnabledAnimated(QAction* action, bool enabled);
  void setCompactMode(bool enabled);      // アイコンだけ
  void setTextUnderIcon(bool enabled);
  void lockHeight(bool locked = true);
 public:

	void homeRequested() W_SIGNAL(homeRequested)
 };









};