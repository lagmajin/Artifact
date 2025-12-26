module;
#include <QLabel>
#include <QWidget>
#include <QBoxLayout>
#include <QComboBox>
#include <QToolButton>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionFooter;

import std;
import Utils.Path;
import Artifact.Service.Project;



namespace Artifact {

 W_OBJECT_IMPL(ArtifactCompositionViewerFooter)

  class ArtifactCompositionViewerFooter::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
  QLabel* label = nullptr;
  QToolButton* pSnapShotButton = nullptr;
  QToolButton* pShutterButton = nullptr;

 };

 ArtifactCompositionViewerFooter::Impl::Impl()
 {
  pSnapShotButton = new QToolButton();
  pSnapShotButton->setIcon(QIcon(ArtifactCore::getIconPath() + "/Png/photo_camera.png"));

 }

 ArtifactCompositionViewerFooter::Impl::~Impl()
 {

 }

 ArtifactCompositionViewerFooter::ArtifactCompositionViewerFooter(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setMaximumHeight(20);
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(5, 0, 5, 0);
  layout->setSpacing(10);
  auto resLabel = new QLabel("Resolution:", this);
  auto font = resLabel->font();
  font.setPointSize(8);
  resLabel->setFont(font);

  layout->addWidget(resLabel);

  auto resCombo = new QComboBox(this);
  resCombo->addItems({ "1920x1080", "1280x720", "800x600" });


  layout->addWidget(resCombo);
  layout->addWidget(impl_->pSnapShotButton);
  layout->addStretch();
  setLayout(layout);

  connect(impl_->pSnapShotButton, &QToolButton::clicked, this, &ArtifactCompositionViewerFooter::takeSnapShotRequested);
 }

 ArtifactCompositionViewerFooter::~ArtifactCompositionViewerFooter()
 {
  delete impl_;
 }

};