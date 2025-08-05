module;
#include <QString>
module Menu.MenuBar;

import Menu;

import ArtifactMainWindow;



namespace Artifact {

 class ArtifactMenuBar::Impl {
 private:

 public:
  ArtifactMainWindow* mainWindow_ = nullptr;
  ArtifactMenuBar* menuBar = nullptr;
  QMenu* fileMenu=nullptr;
  ArtifactEditMenu* editMenu = nullptr;
  QMenu* compositionMenu=nullptr;
  QMenu* layerMenu = nullptr;
  QMenu* viewMenu = nullptr;
    
  ArtifactAnimationMenu* animationMenu = nullptr;
  //ArtifactHelpMenu* helpMenu = nullptr;
  Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu);
  ~Impl();
  //void setUpUI(QMenu* menu);
 };

 ArtifactMenuBar::Impl::Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu):mainWindow_(mainWindow)
 {
  fileMenu = new ArtifactFileMenu(menu);
  
  editMenu = new ArtifactEditMenu(menu);
  animationMenu = new ArtifactAnimationMenu(menu);

  menu->addMenu(fileMenu);
  menu->addMenu(editMenu);
  menu->addMenu(animationMenu);
 }

 ArtifactMenuBar::Impl::~Impl()
 {

 }


 ArtifactMenuBar::ArtifactMenuBar(ArtifactMainWindow* mainWindow, QWidget* parent/*=nullptr*/):QMenuBar(parent),impl_(new Impl(mainWindow,this))
 {
  
  impl_->editMenu = new ArtifactEditMenu(this);

  impl_->compositionMenu = new ArtifactCompositionMenu(mainWindow,this);
  impl_->layerMenu = new ArtifactLayerMenu(this);
  impl_->viewMenu = new ArtifactViewMenu(this);
  //impl_->helpMenu = new ArtifactHelpMenu();


  
  addMenu(impl_->compositionMenu);
  addMenu(impl_->layerMenu);
  addMenu(impl_->viewMenu);
  //addMenu(impl_->helpMenu);

  QString styleSheet = R"(
        QMenuBar {
            background-color: #333333; /* ダークな背景色 */
            color: #FFFFFF; /* 明るいテキスト色 */
            border: 1px solid #555555; /* わずかなボーダー */
        }

        QMenuBar::item {
            spacing: 3px; /* メニューアイテム間のスペース */
            padding: 5px 10px; /* パディング */
            background-color: transparent; /* デフォルトで透明な背景 */
            color: #FFFFFF; /* 明るいテキスト色 */
        }

        QMenuBar::item:selected {
            background-color: #555555; /* ホバー時の背景色 */
        }

        QMenuBar::item:pressed {
            background-color: #222222; /* クリック時の背景色 */
        }

        QMenu {
            background-color: #444444; /* ドロップダウンメニューの背景色 */
            color: #FFFFFF; /* ドロップダウンメニューのテキスト色 */
            border: 1px solid #666666; /* ドロップダウンメニューのボーダー */
        }

        QMenu::item {
            padding: 5px 20px 5px 20px; /* ドロップダウンメニューアイテムのパディング */
            color: #FFFFFF; /* ドロップダウンメニューアイテムのテキスト色 */
        }

        QMenu::item:selected {
            background-color: #666666; /* ドロップダウンメニューアイテムのホバー時の背景色 */
        }

        QMenu::separator {
            height: 1px;
            background-color: #666666; /* セパレータの色 */
            margin-left: 10px;
            margin-right: 10px;
        }
    )";

  setStyleSheet(styleSheet);
 }

 ArtifactMenuBar::~ArtifactMenuBar()
 {
  delete impl_;
 }

 void ArtifactMenuBar::setMainWindow(ArtifactMainWindow* window)
 {

 }


};