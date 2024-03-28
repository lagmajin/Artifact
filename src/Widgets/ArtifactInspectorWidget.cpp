#include "../../include/Widgets/ArtifactInspectorWidget.hpp"





namespace Artifact {

 using namespace ArtifactWidgets;






 ArtifactInspectorWidget::ArtifactInspectorWidget(QWidget* parent /*= nullptr*/):QScrollArea(parent)
 {
  auto p=new VolumeSlider();


 }

 ArtifactInspectorWidget::~ArtifactInspectorWidget()
 {

 }

}