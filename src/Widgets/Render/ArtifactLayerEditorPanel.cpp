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
  ~Impl();
 };

 ArtifactLayerEditorPanel::Impl::Impl()
 {
  editor_ = new ArtifactLayerEditor2DWidget();
  
  footer_ = new ArtifactCompositionViewerFooter();
 }

 ArtifactLayerEditorPanel::Impl::~Impl()
 {

 }

 ArtifactLayerEditorPanel::ArtifactLayerEditorPanel(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  

  auto vBoxLayout = new QVBoxLayout(this);
  vBoxLayout->setContentsMargins(0, 0, 0, 0);
  vBoxLayout->setSpacing(1);
  vBoxLayout->addWidget(impl_->editor_);
  vBoxLayout->addWidget(impl_->footer_);

  setLayout(vBoxLayout);
 }

 ArtifactLayerEditorPanel::~ArtifactLayerEditorPanel()
 {
  delete impl_;
 }

 void ArtifactLayerEditorPanel::closeEvent(QCloseEvent* event)
 {
  this->deleteLater();
 }

 QSize ArtifactLayerEditorPanel::sizeHint() const
 {
  return QSize(400, 600); // •200pxA‚‚³100px‚ª–]‚Ü‚µ‚¢
 }

};