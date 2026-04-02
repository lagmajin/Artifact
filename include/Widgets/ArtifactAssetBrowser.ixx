module;
#include <wobjectdefs.h>
#include <QWidget>
#include <QTreeView>
#include <QStringList>
#include <QSize>
#include <QFrame>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPoint>

export module Widgets.AssetBrowser;

export namespace Artifact {

class SearchBarWidget : public QWidget
{
  W_OBJECT(SearchBarWidget)
public:
  SearchBarWidget();
  ~SearchBarWidget();
};

class AssetFolderTreeView : public QTreeView
{
public:
  AssetFolderTreeView();
  ~AssetFolderTreeView();
};

class ArtifactAssetBrowserToolBar : public QWidget
{
  W_OBJECT(ArtifactAssetBrowserToolBar)
private:
  class Impl;
  Impl* impl_;
public:
  explicit ArtifactAssetBrowserToolBar(QWidget* parent = nullptr);
  ~ArtifactAssetBrowserToolBar();
};

class ArtifactBreadcrumbWidget : public QFrame
{
  W_OBJECT(ArtifactBreadcrumbWidget)
private:
  class Impl;
  Impl* impl_;
public:
  explicit ArtifactBreadcrumbWidget(QWidget* parent = nullptr);
  ~ArtifactBreadcrumbWidget();

  void setPath(const QString& path);
  void setRootPath(const QString& rootPath);

signals:
  void pathClicked(const QString& path) W_SIGNAL(pathClicked, path);
};

class ArtifactAssetBrowser : public QWidget
{
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

  QSize sizeHint() const override;

  void setSearchFilter(const QString& filter);
  void setFileTypeFilter(const QString& type);
  void setStatusFilter(const QString& status);
  void navigateToFolder(const QString& folderPath);
  void selectAssetPaths(const QStringList& filePaths);

signals:
  void folderChanged(const QString& folderPath) W_SIGNAL(folderChanged, folderPath);
  void selectionChanged(const QStringList& selectedFiles) W_SIGNAL(selectionChanged, selectedFiles);
  void itemDoubleClicked(const QString& itemPath) W_SIGNAL(itemDoubleClicked, itemPath);
  void filesDropped(const QStringList& filePaths) W_SIGNAL(filesDropped, filePaths);

protected:
  void showContextMenu(const QPoint& pos);
  void updateFileInfo(const QString& filePath);
};

} // namespace Artifact
