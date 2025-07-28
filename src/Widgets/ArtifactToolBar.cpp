module;
#include <QAction>
#include <QToolBar>
#include <wobjectimpl.h>
module Widgets.ToolBar;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactToolBar)

 class ArtifactToolBar::Impl{
 private:

 public:
  Impl(QToolBar* toolbar);
  ~Impl();
  //QAction* homeAction_ = nullptr;
	//QAction*
 };

 ArtifactToolBar::Impl::Impl(QToolBar* toolbar)
 {
  auto homeAction_ = new QAction();

  toolbar->addAction(homeAction_);

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






};


