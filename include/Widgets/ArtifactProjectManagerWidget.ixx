module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QToolBar>
#include <QTreeView>
#include <QFileInfo>
#include <QStringList>

export module Artifact.Widgets.ProjectManagerWidget;

import std;

import Artifact.Project;

W_REGISTER_ARGTYPE(QStringList)
W_REGISTER_ARGTYPE(QFileInfo)

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
 protected:
  void mouseMoveEvent(QMouseEvent* event) override;
   void mousePressEvent(QMouseEvent* event) override;
 public:
  explicit ArtifactProjectView(QWidget* parent = nullptr);
  ~ArtifactProjectView();
 };

 class ArtifactProjectManagerToolBox :public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 protected:

  void resizeEvent(QResizeEvent* event) override;

 public:
  explicit ArtifactProjectManagerToolBox(QWidget* widget=nullptr);
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
  public /*signals*/:
   void onFileDropped(const QStringList& list) W_SIGNAL(onFileDropped, list)
 	
 public:
  void updateRequested();
  W_SLOT(updateRequested);
 };







};