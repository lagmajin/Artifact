
module;
#include <QMenu>
#include <QWidget>
#include <QAction>
module Menu.Test;

import Artifact.Widgets.SoftwareRenderTest;






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
