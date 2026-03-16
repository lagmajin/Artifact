
module;
#include <QMenu>
#include <QWidget>
#include <QAction>
module Menu.Test;

import Artifact.Widgets.SoftwareRenderTest;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.LayerCompositeTest;
import Artifact.Widgets.TimelineLayerTest;






namespace Artifact {

 ArtifactRenderTestMenu::ArtifactRenderTestMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {
  setTitle("RenderTest");
 }

 ArtifactRenderTestMenu::~ArtifactRenderTestMenu()
 {

 }

 ArtifactTestMenu::ArtifactTestMenu(QWidget* parent /*= nullptr*/) :QMenu(parent)
 {
  setTitle("Test");

  auto* softwareRenderTestAction = new QAction("Software 3D Render Test...", this);
  addAction(softwareRenderTestAction);
  QObject::connect(softwareRenderTestAction, &QAction::triggered, this, []() {
      auto* w = new ArtifactSoftwareRenderTestWidget();
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->resize(960, 600);
      w->show();
      w->raise();
      w->activateWindow();
  });

  auto* softwareCompositionTestAction = new QAction("Software Composition Test...", this);
  addAction(softwareCompositionTestAction);
  QObject::connect(softwareCompositionTestAction, &QAction::triggered, this, []() {
      auto* w = new ArtifactSoftwareCompositionTestWidget();
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->resize(1100, 760);
      w->show();
      w->raise();
      w->activateWindow();
  });

  auto* softwareLayerTestAction = new QAction("Software Layer Test...", this);
  addAction(softwareLayerTestAction);
  QObject::connect(softwareLayerTestAction, &QAction::triggered, this, []() {
      auto* w = new ArtifactSoftwareLayerTestWidget();
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->resize(1100, 820);
      w->show();
      w->raise();
      w->activateWindow();
  });

  auto* layerCompositeTestAction = new QAction("Layer Composite Test...", this);
  addAction(layerCompositeTestAction);
  QObject::connect(layerCompositeTestAction, &QAction::triggered, this, []() {
      auto* w = new ArtifactLayerCompositeTestWidget();
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->show();
      w->raise();
      w->activateWindow();
  });

  auto* timelineLayerTestAction = new QAction("Timeline Layer Test...", this);
  addAction(timelineLayerTestAction);
  QObject::connect(timelineLayerTestAction, &QAction::triggered, this, []() {
      auto* w = new ArtifactTimelineLayerTestWidget();
      w->setAttribute(Qt::WA_DeleteOnClose, true);
      w->resize(1600, 1000);
      w->show();
      w->raise();
      w->activateWindow();
  });

 }

 ArtifactTestMenu::~ArtifactTestMenu()
 {

 }

 class ArtifactMediaTestMenuPrivate {
 private:

 public:

 };




 ArtifactMediaTestMenu::ArtifactMediaTestMenu(QWidget* parent /*= nullptr*/) :QMenu(parent)
 {

 }

 ArtifactMediaTestMenu::~ArtifactMediaTestMenu()
 {

 }



};
