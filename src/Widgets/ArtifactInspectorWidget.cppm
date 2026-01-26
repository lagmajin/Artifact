module;
#include <wobjectimpl.h>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
module Widgets.Inspector;
import std;
import Widgets.Utils.CSS;

import Artifact.Service.Project;

namespace Artifact {

 using namespace ArtifactCore;

 //using namespace ArtifactWidgets;

 W_OBJECT_IMPL(ArtifactInspectorWidget)

  class ArtifactInspectorWidget::Impl {
  private:

  public:
   Impl();
   ~Impl();
   QWidget* containerWidget = nullptr;
   QLabel* layerNameLabel = nullptr;
   QMenu* inspectorMenu_ = nullptr;
   void rebuildMenu();
   void defaultHandleKeyPressEvent(QKeyEvent* event);
   void defaultHandleMousePressEvent(QMouseEvent* event);
   
   void showContextMenu();
 	
   void handleProjectClosed();
 };

 ArtifactInspectorWidget::Impl::Impl()
 {

 }

 ArtifactInspectorWidget::Impl::~Impl()
 {

 }

 void ArtifactInspectorWidget::Impl::rebuildMenu()
 {

 }

 void ArtifactInspectorWidget::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
 }

 void ArtifactInspectorWidget::Impl::handleProjectClosed()
 {

 }

 void ArtifactInspectorWidget::update()
 {

 }

 ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget* parent /*= nullptr*/) :QScrollArea(parent),impl_(new Impl())
 {
  
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);


  auto layout = new QVBoxLayout();
  impl_->containerWidget = new QWidget();

  auto layerNameLabel = new QLabel("Layer:");
  layout->addWidget(layerNameLabel);

  // レイアウトの上詰めと余白調整
  layout->setAlignment(Qt::AlignTop);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  impl_->containerWidget->setLayout(layout);

  setWidget(impl_->containerWidget);
  setWidgetResizable(true);
 	
  auto projectService=ArtifactProjectService::instance();

 //connect(projectService,)	
 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {
  delete impl_;
 }

 void ArtifactInspectorWidget::triggerUpdate()
 {
  update();
 }

 void ArtifactInspectorWidget::contextMenuEvent(QContextMenuEvent*)
 {



 }

}