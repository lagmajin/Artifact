module;
#include <QFileSystemModel>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QToolButton>
#include <wobjectimpl.h>
#include <boost/asio/basic_signal_set.hpp>
module Widgets.AssetBrowser;
import Widgets.Utils.CSS;
namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactAssetBrowserToolBar)



  class ArtifactAssetBrowserToolBar::Impl
 {
 private:
 	
 public:
  Impl();
  ~Impl();
  QLineEdit* searchWidget = nullptr;
 };

 ArtifactAssetBrowserToolBar::Impl::Impl()
 {
  searchWidget = new QLineEdit();
 }

 ArtifactAssetBrowserToolBar::Impl::~Impl()
 {

 }

 ArtifactAssetBrowserToolBar::ArtifactAssetBrowserToolBar(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {

 	
  auto layout = new QHBoxLayout();
  layout->addWidget(impl_->searchWidget);
 	

  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 W_OBJECT_IMPL(ArtifactAssetBrowser)

  class ArtifactAssetBrowser::Impl
 {
 private:


 public:
  QTreeView* directoryView_ = nullptr;
  void handleDirectryChanged();
  void handleDoubleClicked();

 };

 ArtifactAssetBrowser::ArtifactAssetBrowser(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setWindowTitle("AssetBrowser");


  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);


  auto assetToolBar = new ArtifactAssetBrowserToolBar();


  auto vLayout = new QVBoxLayout();


  auto layout = new QHBoxLayout();

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto model = new QFileSystemModel(this);
  model->setRootPath(""); // 空にしておくと全体が見える
  model->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);

  directoryView->setModel(model);
  directoryView->setColumnHidden(1, true); // Size
  directoryView->setColumnHidden(2, true); // Type
  directoryView->setColumnHidden(3, true);
  directoryView->setHeaderHidden(true);

  QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  directoryView->setRootIndex(model->index(desktopPath));

  directoryView->setIndentation(15);          // 階層のインデント
  directoryView->setExpandsOnDoubleClick(true);
  directoryView->setAnimated(true);

  auto assetPathLabel = new QLabel("Assets > 2D Texture");
  assetPathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  assetPathLabel->setStyleSheet("font-weight: bold;");

  auto fileView = new QListWidget();
  fileView->setViewMode(QListView::IconMode);
  fileView->setIconSize(QSize(64, 64));
  fileView->setResizeMode(QListView::Adjust);
  fileView->setFlow(QListView::LeftToRight);
  //fileView->setModel(model);  // QFileSystemModel をセット
  fileView->setRootIndex(model->index(desktopPath));
  fileView->setTextElideMode(Qt::ElideRight);
  // デスクトップを表示


  auto filePathLabel = new QLabel();

  auto VBoxLayout = new  QVBoxLayout();
  VBoxLayout->addWidget(assetPathLabel);
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(filePathLabel);

  vLayout->addWidget(assetToolBar);
  layout->addWidget(directoryView);
  layout->addLayout(VBoxLayout);
  setLayout(layout);

 }

 ArtifactAssetBrowser::~ArtifactAssetBrowser()
 {
  delete impl_;
 }


};