module;
#include <QMenu>
#include <QWidget>
#include <QAction>
#include <QMessageBox>
module Menu.Test;

import Artifact.Widgets.SoftwareRenderTest;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.LayerCompositeTest;
import Artifact.Widgets.TimelineLayerTest;
import Artifact.Service.Project;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;



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

  addSeparator();

  auto* startSoftwareTestPipelineAction = new QAction("Software Test Pipeline を開始", this);
  addAction(startSoftwareTestPipelineAction);
  QObject::connect(startSoftwareTestPipelineAction, &QAction::triggered, this, []() {
      auto* projectService = ArtifactProjectService::instance();
      if (!projectService) {
          QMessageBox::warning(nullptr, "Software Test", "ProjectService が利用できません。");
          return;
      }

      // 1. コンポジション作成
      ArtifactCompositionInitParams params = ArtifactCompositionInitParams::hdPreset();
      params.setCompositionName(UniString(QStringLiteral("SoftwareTest")));
      projectService->createComposition(params);
      auto currentComp = projectService->currentComposition().lock();
      if (!currentComp) {
          QMessageBox::warning(nullptr, "Software Test", "コンポジション作成に失敗しました。");
          return;
      }
      const int beforeLayerCount = currentComp->allLayer().size();

      // 2. 平面レイヤー追加
      ArtifactSolidLayerInitParams solidParams(QStringLiteral("Solid 1"));
      solidParams.setWidth(params.width());
      solidParams.setHeight(params.height());
      solidParams.setColor(FloatColor(0.22f, 0.52f, 0.88f, 1.0f));
      projectService->addLayerToCurrentComposition(solidParams);
      currentComp = projectService->currentComposition().lock();
      if (!currentComp || currentComp->allLayer().size() <= beforeLayerCount) {
          QMessageBox::warning(nullptr, "Software Test", "平面レイヤー追加に失敗しました。");
          return;
      }

      // 3. Software Composition Test を起動
      auto* preview = new ArtifactSoftwareCompositionTestWidget();
      preview->setAttribute(Qt::WA_DeleteOnClose, true);
      preview->resize(1100, 760);
      preview->show();
      preview->raise();
      preview->activateWindow();

      QMessageBox::information(
          nullptr,
          "Software Test",
          QStringLiteral("Software Test Pipeline を初期化しました。\n\n"
              "1) コンポジション作成\n"
              "2) 平面レイヤー追加\n"
              "3) Software Composition Test 起動\n\n"
              "このウィンドウを閉じて、Test メニューからいつでも再起動できます。"));
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
