module;
#include <QAction>
#include <QToolBar>
#include <wobjectimpl.h>
module Widgets.ToolBar;
import Utils;
namespace Artifact
{
 W_OBJECT_IMPL(ArtifactToolBar)

 class ArtifactToolBar::Impl{
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
 	
 };

 ArtifactToolBar::Impl::Impl(QToolBar* toolbar)
 {
  homeAction_ = new QAction();
  homeAction_->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/home.png"));
  handTool_ = new QAction();
 	

  toolbar->addAction(homeAction_);
  toolbar->addAction(handTool_);
 }

 ArtifactToolBar::Impl::~Impl()
 {

 }

 ArtifactToolBar::ArtifactToolBar(QWidget* parent /*=nullptr*/):QToolBar(parent),impl_(new Impl(this))
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


