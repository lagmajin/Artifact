module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QToolBar>
#include <QTreeView>

export module ArtifactProjectManagerWidget;

import std;


import Project;

export namespace Artifact {

 /*
class ArtifactToolBar :public QToolBar {
private:

public:

};
*/

 class ArtifactProjectView :public QTreeView {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectView(QWidget* parent = nullptr);
  ~ArtifactProjectView();
 };

 class ArtifactProjectManagerToolBox :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactProjectManagerToolBox(QWidget* widget);
  ~ArtifactProjectManagerToolBox();
 };

 class ArtifactProjectManagerWidget :public QWidget {
  W_OBJECT(ArtifactProjectManagerWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void dropEvent(QDropEvent* event);
  void dragEnterEvent(QDragEnterEvent* event);
  QSize sizeHint() const;

  void contextMenuEvent(QContextMenuEvent* event) override;

 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();
  void setFilter();



  void triggerUpdate();

 public:


 };







};