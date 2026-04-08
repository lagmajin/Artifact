module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <wobjectdefs.h>
#include <QList>
#include <QWidget>
#include <QScrollBar>
#include <QToolBar>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPaintEvent>
#include <QStringList>
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
 protected:
  void paintEvent(QPaintEvent* event) override;
 public:
  explicit HoverThumbnailPopupWidget(QWidget* parent = nullptr);
  ~HoverThumbnailPopupWidget();
  void setThumbnail(const QPixmap& pixmap);
  void setLabels(const QStringList& labels);
  void setLabel(int idx, const QString& text);
  void showAt(const QPoint& globalPos);
 };

 class ArtifactProjectView :public QWidget {
  W_OBJECT(ArtifactProjectView)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void dropEvent(QDropEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool event(QEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
 public:
  explicit ArtifactProjectView(QWidget* parent = nullptr);
  ~ArtifactProjectView();
  void setModel(QAbstractItemModel* model);
  QAbstractItemModel* model() const;
  QItemSelectionModel* selectionModel() const;
  QModelIndex currentIndex() const;
  void setCurrentIndex(const QModelIndex& index);
  void setSortingEnabled(bool enabled);
  void sortByColumn(int column, Qt::SortOrder order);
  void setColumnWidth(int column, int width);
  void expand(const QModelIndex& index);
  void collapse(const QModelIndex& index);
  void setExpanded(const QModelIndex& index, bool expanded);
  
  void handleItemDoubleClicked(const QModelIndex& index);
  void editIndex(const QModelIndex& index);
  void ensureIndexVisible(const QModelIndex& index);
  void refreshVisibleContent();

  void expandAll();
  void collapseAll();
  void expandToDepth(int depth);
  QModelIndex indexAt(const QPoint& pos) const;
  QRect visualRect(const QModelIndex& index) const;

  QSize sizeHint() const override;

 public /*signals*/:
  void itemSelected(const QModelIndex& index) W_SIGNAL(itemSelected, index);
  void itemDoubleClicked(const QModelIndex& index) W_SIGNAL(itemDoubleClicked, index);

 private:
  void updateScrollRange();
  QScrollBar* verticalScrollBar_;
  int scrollY_ = 0;
  
  struct RenderItem {
      QModelIndex index;
      int level = 0;
      bool isExpanded = false;
      QRect rect;
      QRect expandRect;
  };
  QList<RenderItem> visibleItems_;
 };

 class ArtifactProjectManagerToolBox :public QWidget
 {
  W_OBJECT(ArtifactProjectManagerToolBox)
 private:
  class Impl;
  Impl* impl_;
 protected:

  void paintEvent(QPaintEvent* event) override;
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
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  QSize sizeHint() const;
  void showEvent(QShowEvent* event) override;
  bool event(QEvent* event) override;

  void contextMenuEvent(QContextMenuEvent* event) override;

 public:
  explicit ArtifactProjectManagerWidget(QWidget* parent = nullptr);
  ~ArtifactProjectManagerWidget();
  ArtifactProjectView* projectView() const;
  bool selectItemsByFilePaths(const QStringList& filePaths);
  void setFilter();
  void triggerUpdate();
  void setThumbnailEnabled(bool b = true);
  public /*signals*/:
  void onFileDropped(const QStringList& list) W_SIGNAL(onFileDropped, list);
  void itemDoubleClicked(const QModelIndex& index) W_SIGNAL(itemDoubleClicked, index);
   
 public/*Slots*/:
  void updateRequested();
  W_SLOT(updateRequested);
 };

};
