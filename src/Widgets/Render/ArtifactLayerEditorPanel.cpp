module;
#include <QWidget>
#include <QBoxLayout>


module Artifact.Widgets.LayerEditorPanel;

import std;
import Artifact.Widgets.CompositionFooter;
import Artifact.Widgets.Render.Layer;

namespace Artifact {

 class ArtifactLayerEditorPanel::Impl {
 private:

 public:
  ArtifactLayerEditor2DWidget* editor_ = nullptr;
  ArtifactCompositionViewerFooter* footer_ = nullptr;

  Impl();
 };

 ArtifactLayerEditorPanel::Impl::Impl()
 {
  editor_ = new ArtifactLayerEditor2DWidget();
  
  footer_ = new ArtifactCompositionViewerFooter();
 }

 ArtifactLayerEditorPanel::ArtifactLayerEditorPanel(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  auto vBoxLayout = new QVBoxLayout(this);
  vBoxLayout->setSpacing(0);
  vBoxLayout->addWidget(impl_->editor_);
  vBoxLayout->addWidget(impl_->footer_);

  setLayout(vBoxLayout);
 }

 ArtifactLayerEditorPanel::~ArtifactLayerEditorPanel()
 {
  delete impl_;
 }

};