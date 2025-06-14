module;
#include <wobjectimpl.h>

#include <QWidget>
module ArtifactInspectorWidget;
//#include "../../include/Widgets/ArtifactInspectorWidget.hpp"





namespace Artifact {

 //using namespace ArtifactWidgets;

 W_OBJECT_IMPL(ArtifactInspectorWidget)




 void ArtifactInspectorWidget::update()
 {

 }

 ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget* parent /*= nullptr*/) :QScrollArea(parent)
 {
  //auto p=new VolumeSlider();


 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {

 }

 void ArtifactInspectorWidget::triggerUpdate()
 {
  update();
 }

}