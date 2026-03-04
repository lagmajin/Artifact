module;
#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QKeySequence>

module Artifact.Menu.View;

import std;
import Artifact.Service.Project;

namespace Artifact {

  class ArtifactViewMenu::Impl {
  public:
   Impl(ArtifactViewMenu* menu);
   ~Impl();

   QAction* zoomInAction = nullptr;
   QAction* zoomOutAction = nullptr;
   QAction* defaultZoomAction = nullptr;
   QAction* fitToScreenAction = nullptr;
   
   QMenu* resolutionMenu = nullptr;
   QAction* resFullAction = nullptr;
   QAction* resHalfAction = nullptr;
   QAction* resThirdAction = nullptr;
   QAction* resQuarterAction = nullptr;

   QAction* showGridAction = nullptr;
   QAction* snapToGridAction = nullptr;
   QAction* showGuidesAction = nullptr;
   QAction* snapToGuidesAction = nullptr;
   QAction* showRulersAction = nullptr;
   QAction* useDisplayColorManagementAction = nullptr;

  };

  ArtifactViewMenu::Impl::Impl(ArtifactViewMenu* menu)
  {
   zoomInAction = new QAction("ズームイン(&I)");
   zoomInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
   
   zoomOutAction = new QAction("ズームアウト(&O)");
   zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));

   defaultZoomAction = new QAction("100% ズーム");
   defaultZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));

   fitToScreenAction = new QAction("画面に合わせる(&F)");
   fitToScreenAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Slash));

   resolutionMenu = new QMenu("解像度(&R)");
   resFullAction = resolutionMenu->addAction("フル画質");
   resHalfAction = resolutionMenu->addAction("1/2画質");
   resThirdAction = resolutionMenu->addAction("1/3画質");
   resQuarterAction = resolutionMenu->addAction("1/4画質");
   resFullAction->setCheckable(true);
   resFullAction->setChecked(true);
   resHalfAction->setCheckable(true);
   resThirdAction->setCheckable(true);
   resQuarterAction->setCheckable(true);

   showGridAction = new QAction("グリッドを表示(&G)");
   showGridAction->setShortcut(QKeySequence("Ctrl+'"));
   showGridAction->setCheckable(true);

   snapToGridAction = new QAction("グリッドにスナップ(&S)");
   snapToGridAction->setShortcut(QKeySequence("Ctrl+Shift+'"));
   snapToGridAction->setCheckable(true);

   showGuidesAction = new QAction("ガイドを表示");
   showGuidesAction->setShortcut(QKeySequence("Ctrl+;"));
   showGuidesAction->setCheckable(true);

   snapToGuidesAction = new QAction("ガイドにスナップ");
   snapToGuidesAction->setShortcut(QKeySequence("Ctrl+Shift+;"));
   snapToGuidesAction->setCheckable(true);

   showRulersAction = new QAction("定規を表示(&R)");
   showRulersAction->setShortcut(QKeySequence("Ctrl+R"));
   showRulersAction->setCheckable(true);

   useDisplayColorManagementAction = new QAction("ディスプレイのカラーマネジメントを使用");
   useDisplayColorManagementAction->setCheckable(true);

   menu->addAction(zoomInAction);
   menu->addAction(zoomOutAction);
   menu->addAction(defaultZoomAction);
   menu->addAction(fitToScreenAction);
   menu->addSeparator();
   menu->addMenu(resolutionMenu);
   menu->addSeparator();
   menu->addAction(useDisplayColorManagementAction);
   menu->addSeparator();
   menu->addAction(showGridAction);
   menu->addAction(snapToGridAction);
   menu->addAction(showGuidesAction);
   menu->addAction(snapToGuidesAction);
   menu->addAction(showRulersAction);
  }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle("表示(&V)");
  setTearOffEnabled(false);
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {
  delete impl_;
 }

 void ArtifactViewMenu::registerView(const QString& name, QWidget* view)
 {

 }

};