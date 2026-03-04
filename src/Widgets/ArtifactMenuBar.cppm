module;
#include <QString>
#include <QMenuBar>
module Menu.MenuBar;

import Menu;

import Artifact.MainWindow;



namespace Artifact {

 class ArtifactMenuBar::Impl {
 private:

 public:
  ArtifactMainWindow* mainWindow_ = nullptr;
  ArtifactMenuBar* menuBar = nullptr;
  QMenu* fileMenu = nullptr;
  ArtifactEditMenu* editMenu = nullptr;

  //QMenu* compositionMenu=nullptr;
  QMenu* layerMenu = nullptr;
  QMenu* viewMenu = nullptr;
  ArtifactCompositionMenu* compositionMenu_ = nullptr;
  ArtifactLayerMenu* layerMenu_ = nullptr;
  ArtifactEffectMenu* effectMenu = nullptr;
  ArtifactAnimationMenu* animationMenu = nullptr;
  //ArtifactHelpMenu* helpMenu = nullptr;
  Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu);
  ~Impl();
  //void setUpUI(QMenu* menu);
 };

 ArtifactMenuBar::Impl::Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu) :mainWindow_(mainWindow)
 {
  fileMenu = new ArtifactFileMenu(menu);

  editMenu = new ArtifactEditMenu(menu);
  compositionMenu_ = new ArtifactCompositionMenu(mainWindow, menu);
  animationMenu = new ArtifactAnimationMenu(menu);
  effectMenu = new ArtifactEffectMenu(menu);

  layerMenu_ = new ArtifactLayerMenu(menu);

  menu->addMenu(fileMenu);
  menu->addMenu(editMenu);
  menu->addMenu(compositionMenu_);
  menu->addMenu(layerMenu_);
  menu->addMenu(effectMenu);
  menu->addMenu(animationMenu);

  QFont font("Segoe UI", 10);
  font.setWeight(QFont::DemiBold);  // or QFont::Bold
  
  menu->setFont(font);
 }

 ArtifactMenuBar::Impl::~Impl()
 {

 }


 ArtifactMenuBar::ArtifactMenuBar(ArtifactMainWindow* mainWindow, QWidget* parent/*=nullptr*/) :QMenuBar(parent), impl_(new Impl(mainWindow, this))
 {

  impl_->editMenu = new ArtifactEditMenu(this);



  impl_->viewMenu = new ArtifactViewMenu(this);
  //impl_->helpMenu = new ArtifactHelpMenu();



  //addMenu(impl_->compositionMenu);
  //addMenu(impl_->layerMenu);
  addMenu(impl_->viewMenu);
  //addMenu(impl_->helpMenu);

  QString styleSheet = R"(
        QMenuBar {
            background-color: #1E1E1E;
            color: #CCCCCC;
            border-bottom: 1px solid #111111;
            padding: 2px 0px;
        }

        QMenuBar::item {
            spacing: 5px;
            padding: 6px 12px;
            background-color: transparent;
            color: #CCCCCC;
            border-radius: 4px;
            font-size: 13px;
        }

        QMenuBar::item:selected {
            background-color: #333333;
            color: #FFFFFF;
        }

        QMenuBar::item:pressed {
            background-color: #094771;
            color: #FFFFFF;
        }

        QMenu {
            background-color: #2D2D30;
            color: #CCCCCC;
            border: 1px solid #1A1A1A;
            padding: 4px 0px;
            /* In Qt, drop-shadow can be natively enabled on menus via window flags, 
               but setting a slightly darker border simulates it nicely. */
        }

        QMenu::item {
            padding: 6px 30px 6px 24px; /* top, right, bottom, left */
            color: #CCCCCC;
            font-size: 13px;
            background-color: transparent;
        }

        QMenu::item:selected {
            background-color: #094771; /* Deep blue highlight */
            color: #FFFFFF;
        }
        
        QMenu::item:disabled {
            color: #666666;
            background-color: transparent;
        }

        QMenu::separator {
            height: 1px;
            background-color: #3E3E42;
            margin: 4px 10px;
        }
        
        QMenu::indicator {
            width: 13px;
            height: 13px;
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