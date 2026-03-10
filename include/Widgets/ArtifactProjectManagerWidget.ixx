module;
#include <QList>
#include <wobjectdefs.h>
#include <QWidget>
#include <QToolBar>
#include <QTreeView>
#include <QFileInfo>
#include <QModelIndex>
#include <QStringList>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Widgets.ProjectManagerWidget;





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

class HoverThumbnailPopupWidget:public QWidget {
private:
 class Impl;
 Impl* impl_;
public:
 explicit HoverThumbnailPopupWidget(QWidget* parent = nullptr);
 ~HoverThumbnailPopupWidget();
 void setThumbnail(const QPixmap& pixmap);
 void setLabels(const QStringList& labels);
 void setLabel(int idx, const QString& text);
 void showAt(const QPoint& globalPos);
};

 class ArtifactProjectView :public QTreeView {
  W_OBJECT(ArtifactProjectView)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void dropEvent(QDropEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
 public:
  explicit ArtifactProjectView(QWidget* parent = nullptr);
  ~ArtifactProjectView();
  void handleItemDoubleClicked(const QModelIndex& index);


  QSize sizeHint() const override;

 public /*signals*/:
  void itemSelected(const QModelIndex& index) W_SIGNAL(itemSelected, index);
 };

 class ArtifactProjectManagerToolBox :public QWidget
 {
  W_OBJECT(ArtifactProjectManagerToolBox)
 private:
  class Impl;
  Impl* impl_;
 protected:

  void resizeEvent(QResizeEvent* event) override;

 public:
  explicit ArtifactProjectManagerToolBox(QWidget* widget = nullptr);
  ~ArtifactProjectManagerToolBox();

 public /*signals*/:
  void newCompositionRequested() W_SIGNAL(newCompositionRequested);
  void newFolderRequested() W_SIGNAL(newFolderRequested);
  void deleteRequested() W_SIGNAL(deleteRequested);
  void generateProxyRequested() W_SIGNAL(generateProxyRequested);
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
  void setThumbnailEnabled(bool b = true);
 public /*signals*/:
  void onFileDropped(const QStringList& list) W_SIGNAL(onFileDropped, list);
   
 public/*Slots*/:
  void updateRequested();
  W_SLOT(updateRequested);
 };







};
