module;
#include <wobjectimpl.h>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>
module Widgets.Inspector;

import Widgets.Utils.CSS;

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
 };

 ArtifactInspectorWidget::Impl::Impl()
 {

 }

 ArtifactInspectorWidget::Impl::~Impl()
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

 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {
  delete impl_;
 }

 void ArtifactInspectorWidget::triggerUpdate()
 {
  update();
 }

}