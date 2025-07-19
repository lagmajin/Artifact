module;
#include <QString>
module Menu.MenuBar;

import Menu;





namespace Artifact {

 class ArtifactMenuBar::Impl {
 private:

 public:
  QMenuBar* menuBar = nullptr;
  QMenu* fileMenu=nullptr;
  QMenu* compositionMenu=nullptr;
  QMenu* layerMenu = nullptr;
  QMenu* viewMenu = nullptr;
  Impl(QMenuBar*menu);
  ~Impl();
  //void setUpUI(QMenu* menu);
 };

 ArtifactMenuBar::Impl::Impl(QMenuBar* menu)
 {

 }

 ArtifactMenuBar::Impl::~Impl()
 {

 }

 ArtifactMenuBar::ArtifactMenuBar(QWidget* parent) :QMenuBar(parent),impl(new Impl(this))
 {
  impl->fileMenu = new ArtifactFileMenu(this);


  impl->compositionMenu = new ArtifactCompositionMenu(this);
  impl->layerMenu = new ArtifactLayerMenu(this);
  impl->viewMenu = new ArtifactViewMenu(this);
  


  addMenu(impl->fileMenu);
  addMenu(impl->compositionMenu);
  addMenu(impl->layerMenu);
  addMenu(impl->viewMenu);

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
  delete impl;
 }


};