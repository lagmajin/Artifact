module;

#include <QString>
#include <QMenuBar>
module Menu.MenuBar;

import Menu;
import Menu.Help;

import Artifact.MainWindow;

namespace Artifact {

 class ArtifactMenuBar::Impl {
 public:
  ArtifactMainWindow* mainWindow_ = nullptr;
  QMenu* fileMenu = nullptr;
  ArtifactEditMenu* editMenu = nullptr;
  ArtifactCompositionMenu* compositionMenu_ = nullptr;
  ArtifactLayerMenu* layerMenu_ = nullptr;
  ArtifactEffectMenu* effectMenu = nullptr;
  ArtifactAnimationMenu* animationMenu = nullptr;
  ArtifactViewMenu* viewMenu = nullptr;
  ArtifactHelpMenu* helpMenu = nullptr;

  Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu);
  ~Impl() = default;
 };

 ArtifactMenuBar::Impl::Impl(ArtifactMainWindow* mainWindow, ArtifactMenuBar* menu) :mainWindow_(mainWindow)
 {
  fileMenu = new ArtifactFileMenu(menu);
  editMenu = new ArtifactEditMenu(menu);
  compositionMenu_ = new ArtifactCompositionMenu(mainWindow, menu);
  animationMenu = new ArtifactAnimationMenu(menu);
  effectMenu = new ArtifactEffectMenu(menu);
  layerMenu_ = new ArtifactLayerMenu(menu);
  viewMenu = new ArtifactViewMenu(menu);
  helpMenu = new ArtifactHelpMenu(menu);

  // ˆê”Ô‰E‚ÉHelp‚ª—ˆ‚é‚æ‚¤‚É‡˜‚ð®—
  menu->addMenu(fileMenu);
  menu->addMenu(editMenu);
  menu->addMenu(compositionMenu_);
  menu->addMenu(layerMenu_);
  menu->addMenu(effectMenu);
  menu->addMenu(animationMenu);
  menu->addMenu(viewMenu);
  menu->addMenu(helpMenu);

  QFont font("Segoe UI", 10);
  font.setWeight(QFont::DemiBold);
  menu->setFont(font);
 }

 ArtifactMenuBar::ArtifactMenuBar(ArtifactMainWindow* mainWindow, QWidget* parent/*=nullptr*/) 
  : QMenuBar(parent), impl_(new Impl(mainWindow, this))
 {
  QString styleSheet = R"(
        QMenuBar {
            background-color: #1E1E1E;
            color: #E0E0E0;
            border-bottom: 1px solid #111111;
            padding: 2px 0px;
        }

        QMenuBar::item {
            spacing: 5px;
            padding: 6px 12px;
            background-color: transparent;
            color: #E0E0E0;
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
            background-color: #252526;
            color: #E0E0E0;
            border: 1px solid #454545;
            padding: 4px 0px;
        }

        QMenu::item {
            padding: 6px 40px 6px 28px;
            color: #E0E0E0;
            font-size: 13px;
            background-color: transparent;
        }

        QMenu::item:selected {
            background-color: #007ACC;
            color: #FFFFFF;
        }

        QMenu::item:disabled {
            color: #666666;
        }

        QMenu::separator {
            height: 1px;
            background-color: #454545;
            margin: 4px 10px;
        }

        QMenu::shortcut {
            color: #888888;
            padding-right: 10px;
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

}