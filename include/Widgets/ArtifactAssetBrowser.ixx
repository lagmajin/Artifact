module;

#include <wobjectdefs.h>
#include <QWidget>
#include <QTreeView>
#include <QStringList>
#include <QListView>

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
export module Widgets.AssetBrowser;



//#pragma once

//#include <wobjectcpp.h>
//#include <QtWidgets/QtWidgets>



export namespace Artifact {

 class SearchBarWidget :public QWidget
 {
  W_OBJECT(SearchBarWidget)
 private:

 public:
  SearchBarWidget();
  ~SearchBarWidget();
 };


 class AssetFolderTreeView :public QTreeView
 {
 private:

 public:

 };

 class ArtifactAssetBrowserToolBar :public QWidget
 {
  W_OBJECT(ArtifactAssetBrowserToolBar)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactAssetBrowserToolBar(QWidget* parent = nullptr);
  ~ArtifactAssetBrowserToolBar();
  public/**/:
 	
 };


 class ArtifactAssetBrowser :public QWidget {
  W_OBJECT(ArtifactAssetBrowser)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
 public:
  explicit ArtifactAssetBrowser(QWidget* parent = nullptr);
   ~ArtifactAssetBrowser();

   // Public methods for filtering
   void setSearchFilter(const QString& filter);
   void setFileTypeFilter(const QString& type); // "all", "images", "videos", "audio"
   void setStatusFilter(const QString& status); // "all", "imported", "missing", "unused"
   void navigateToFolder(const QString& folderPath);
  
  public:
   void folderChanged(const QString& folderPath) W_SIGNAL(folderChanged, folderPath)
   void selectionChanged(const QStringList& selectedFiles) W_SIGNAL(selectionChanged, selectedFiles)
   void itemDoubleClicked(const QString& itemPath) W_SIGNAL(itemDoubleClicked, itemPath)
   void filesDropped(const QStringList& filePaths) W_SIGNAL(filesDropped, filePaths)
 
  protected:
   void showContextMenu(const QPoint& pos);
   void updateFileInfo(const QString& filePath);


 };






};