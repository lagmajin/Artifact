module ;
#include <QLabel>
#include <QWidget>
#include <QBoxLayout>
#include <QComboBox>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionFooter;



namespace Artifact {
	
 W_OBJECT_IMPL(ArtifactCompositionViewerFooter)
	
 class ArtifactCompositionViewerFooter::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
  QLabel* label = nullptr;
 	
 };

 ArtifactCompositionViewerFooter::Impl::Impl()
 {

 }

 ArtifactCompositionViewerFooter::Impl::~Impl()
 {

 }

 ArtifactCompositionViewerFooter::ArtifactCompositionViewerFooter(QWidget* parent /*= nullptr*/)
 {
  setMaximumHeight(20);
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(5, 0, 5, 0);
  layout->setSpacing(10);
  auto resLabel = new QLabel("Resolution:", this);
  auto font=resLabel->font();
  font.setPointSize(8);
  resLabel->setFont(font);
 	
 	layout->addWidget(resLabel);
  auto resCombo = new QComboBox(this);
  resCombo->addItems({ "1920x1080", "1280x720", "800x600" });
  
  
  layout->addWidget(resCombo);

  layout->addStretch();
  setLayout(layout);

 
 }

 ArtifactCompositionViewerFooter::~ArtifactCompositionViewerFooter()
 {

 }

}