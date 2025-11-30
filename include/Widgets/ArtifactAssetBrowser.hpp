module;

#include <wobjectdefs.h>
#include <QWidget>
#include <QTreeView>
#include <QStringList>

export module Widgets.AssetBrowser;

//#pragma once

//#include <wobjectcpp.h>
//#include <QtWidgets/QtWidgets>

import std;

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
 public:
  explicit ArtifactAssetBrowser(QWidget* parent = nullptr);
  ~ArtifactAssetBrowser();


  void folderChanged(const QString& folderPath) W_SIGNAL(folderChanged, folderPath)
   void selectionChanged(const QStringList& selectedFiles) W_SIGNAL(selectionChanged, selectedFiles)
   void itemDoubleClicked(const QString& itemPath) W_SIGNAL(itemDoubleClicked, itemPath)


 };






};