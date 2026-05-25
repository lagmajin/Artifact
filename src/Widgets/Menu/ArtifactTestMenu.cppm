module;
#include <utility>
#include <functional>
#include <QAction>
#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QWidget>
#include <wobjectimpl.h>

module Menu.Test;

import Artifact.Widgets.SoftwareRenderTest;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.LayerCompositeTest;
import Artifact.Widgets.TimelineLayerTest;
import Artifact.Widgets.PlaybackControlTestWidget;
import Artifact.Widgets.Test.ScrollPoC;
import Menu.Test2;
import Artifact.Service.Project;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Utils.Path;

namespace Artifact {

namespace {
template <typename WidgetT>
void openTestWidget(int width, int height)
{
 auto* widget = new WidgetT();
 widget->setAttribute(Qt::WA_DeleteOnClose, true);
 widget->resize(width, height);
 widget->show();
 widget->raise();
 widget->activateWindow();
}

QAction* addOpenWidgetAction(QMenu* menu, const QString& text, const QString& iconPath, const std::function<void()>& openFn)
{
 auto* action = new QAction(text, menu);
 if (!iconPath.isEmpty()) {
  action->setIcon(QIcon(resolveIconPath(iconPath)));
 }
 menu->addAction(action);
 QObject::connect(action, &QAction::triggered, menu, [openFn]() { openFn(); });
 return action;
}
}

ArtifactRenderTestMenu::ArtifactRenderTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Render Test");
 setIcon(QIcon(resolveIconPath("Studio/software_render.svg")));

 addOpenWidgetAction(this, "Software 3D Render Test...", "Studio/software_render.svg", []() {
  openTestWidget<ArtifactSoftwareRenderTestWidget>(960, 600);
 });
 addOpenWidgetAction(this, "Software Composition Test...", "Studio/software_composition.svg", []() {
  openTestWidget<ArtifactSoftwareCompositionTestWidget>(1100, 760);
 });
 addOpenWidgetAction(this, "Software Layer Test...", "Studio/software_layer.svg", []() {
  openTestWidget<ArtifactSoftwareLayerTestWidget>(1100, 820);
 });
 addOpenWidgetAction(this, "Layer Composite Test...", "Studio/layer_composite.svg", []() {
  openTestWidget<ArtifactLayerCompositeTestWidget>(900, 700);
 });
 addOpenWidgetAction(this, "Timeline Layer Test...", "Studio/timeline_layer.svg", []() {
  openTestWidget<ArtifactTimelineLayerTestWidget>(1600, 1000);
 });
}

ArtifactRenderTestMenu::~ArtifactRenderTestMenu()
{
}

ArtifactWidgetTestMenu::ArtifactWidgetTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Widget Test");
 setIcon(QIcon(resolveIconPath("Studio/test.svg")));

 addOpenWidgetAction(this, "Scroll PoC...", "Studio/scroll.svg", []() {
  openTestWidget<ArtifactScrollPoCWidget>(600, 500);
 });
}

ArtifactWidgetTestMenu::~ArtifactWidgetTestMenu()
{
}

ArtifactMediaTestMenu::ArtifactMediaTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Media Test");
 setIcon(QIcon(resolveIconPath("Studio/play_arrow.svg")));

 addOpenWidgetAction(this, "Playback Control Test...", "Studio/replay.svg", []() {
  openTestWidget<ArtifactPlaybackControlTestWidget>(1100, 760);
 });
}

ArtifactMediaTestMenu::~ArtifactMediaTestMenu()
{
}

ArtifactTestMenu::ArtifactTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Test");
 setIcon(QIcon(resolveIconPath("Studio/test.svg")));

 auto* renderMenu = new ArtifactRenderTestMenu(this);
 auto* widgetMenu = new ArtifactWidgetTestMenu(this);
 auto* mediaMenu = new ArtifactMediaTestMenu(this);
 auto* imageProcessingMenu = new ArtifactImageProcessingTestMenu(this);

 addMenu(renderMenu);
 addMenu(widgetMenu);
 addMenu(mediaMenu);
 addMenu(imageProcessingMenu);

 addSeparator();

 auto* startSoftwareTestPipelineAction = new QAction("Software Test Pipeline を開始", this);
 startSoftwareTestPipelineAction->setIcon(QIcon(resolveIconPath("Studio/pipeline.svg")));
 addAction(startSoftwareTestPipelineAction);
 QObject::connect(startSoftwareTestPipelineAction, &QAction::triggered, this, []() {
  auto* projectService = ArtifactProjectService::instance();
  if (!projectService) {
   QMessageBox::warning(nullptr, "Software Test", "ProjectService が利用できません。");
   return;
  }

  ArtifactCompositionInitParams params = ArtifactCompositionInitParams::hdPreset();
  params.setCompositionName(UniString(QStringLiteral("SoftwareTest")));
  projectService->createComposition(params);
  auto currentComp = projectService->currentComposition().lock();
  if (!currentComp) {
   QMessageBox::warning(nullptr, "Software Test", "コンポジション作成に失敗しました。");
   return;
  }
  const int beforeLayerCount = currentComp->allLayer().size();

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

  openTestWidget<ArtifactSoftwareCompositionTestWidget>(1100, 760);

  QMessageBox::information(
      nullptr,
      "Software Test",
      QStringLiteral(
          "Software Test Pipeline を初期化しました。\n\n"
          "1) コンポジション作成\n"
          "2) 平面レイヤー追加\n"
          "3) Software Composition Test 起動\n\n"
          "このメニューからいつでも再起動できます。"));
 });
}

ArtifactTestMenu::~ArtifactTestMenu()
{
}

};
