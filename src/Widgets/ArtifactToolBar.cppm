module;
#include <QAction>
#include <QToolBar>
#include <wobjectimpl.h>
module Widgets.ToolBar;
import Utils;


import Artifact.Tool.Manager;
import Artifact.Service.Application;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactToolBar)

  class ArtifactToolBar::Impl {
  private:

  public:
   Impl(QToolBar* toolbar);
   ~Impl();
   QAction* homeAction_ = nullptr;
   QAction* selectTool_ = nullptr;
   QAction* maskTool_ = nullptr;
   QAction* handTool_ = nullptr;
   QAction* shapeTool_ = nullptr;
   QAction* papetTool_ = nullptr;
   void handleHomeSelected();
   void handleMaskToolSelected();
 	
 };

 ArtifactToolBar::Impl::Impl(QToolBar* toolbar)
 {
  homeAction_ = new QAction();
  homeAction_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/home.png"));
  handTool_ = new QAction();

  maskTool_ = new QAction();
  maskTool_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/pen.png"));

  shapeTool_ = new QAction();
  shapeTool_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/path.png"));

  toolbar->addAction(homeAction_);
  toolbar->addAction(handTool_);
  toolbar->addAction(maskTool_);
  toolbar->addAction(shapeTool_);

  QObject::connect(homeAction_, &QAction::triggered, [=]() {
   handleHomeSelected();
   });
  QObject::connect(handTool_, &QAction::triggered, [=]() {
   handleMaskToolSelected();
   });
  toolbar->setStyleSheet(R"(
QToolButton {
    background: transparent;
    border: none;
    padding: 4px;
}
QToolButton:hover {
    background: rgba(255,255,255,30);
}
QToolButton:pressed {
    background: rgba(255,255,255,60);
}
QToolButton:checked {
    background: rgba(255,255,255,50);
}
)");
 }

 ArtifactToolBar::Impl::~Impl()
 {

 }

 void ArtifactToolBar::Impl::handleHomeSelected()
 {

 }

 void ArtifactToolBar::Impl::handleMaskToolSelected()
 {

 }

 ArtifactToolBar::ArtifactToolBar(QWidget* parent /*=nullptr*/) :QToolBar(parent), impl_(new Impl(this))
 {
  setIconSize(QSize(32, 32));
 }
 ArtifactToolBar::~ArtifactToolBar()
 {
  delete impl_;
 }

 void ArtifactToolBar::setCompactMode(bool enabled)
 {

 }

 void ArtifactToolBar::setTextUnderIcon(bool enabled)
 {

 }




};


