#include "../../include/Widgets/ArtifactInspectorWidget.hpp"





namespace Artifact {

 using namespace ArtifactWidgets;






 void ArtifactInspectorWidget::update()
 {

 }

 ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget* parent /*= nullptr*/) :QScrollArea(parent)
 {
  auto p=new VolumeSlider();


 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {

 }

 void ArtifactInspectorWidget::triggerUpdate()
 {
  update();
 }

}