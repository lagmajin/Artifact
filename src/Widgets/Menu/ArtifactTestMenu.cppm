module;
#include <algorithm>
#include <utility>
#include <functional>
#include <QAction>
#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QSize>
#include <QWidget>
#include <wobjectimpl.h>

module Menu.Test;

import Artifact.Widgets.SoftwareRenderTest;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.LayerCompositeTest;
import Artifact.Widgets.TimelineLayerTest;
import Artifact.Widgets.Test.ScrollPoC;
import Menu.Test2;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Composition.InitParams;
import Artifact.Composition.Abstract;
import Color.Float;
import Utils.Id;
import Layer.Blend;
import Artifact.Layer.InitParams;
import Artifact.Layer.Abstract;
import Utils.Path;

namespace Artifact {
using namespace ArtifactCore;
using ArtifactCore::CompositionID;
using ArtifactCore::FloatColor;
using ArtifactCore::LAYER_BLEND_TYPE;

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

ArtifactAbstractLayerPtr addDebugSolidBlendLayer(
    const CompositionID &compositionId, const QString &name,
    const QSize &size, const FloatColor &color,
    const LAYER_BLEND_TYPE blendMode, const float opacity)
{
  auto &manager = ArtifactProjectManager::getInstance();
  ArtifactSolidLayerInitParams params(name);
  params.setWidth(std::max<int>(1, size.width()));
  params.setHeight(std::max<int>(1, size.height()));
  params.setColor(color);
  auto result = manager.addLayerToComposition(
      compositionId, static_cast<ArtifactLayerInitParams &>(params));
 if (!result.success || !result.layer) {
  return {};
 }
 result.layer->setBlendMode(blendMode);
 result.layer->setOpacity(std::clamp(opacity, 0.0f, 1.0f));
 return result.layer;
}
}

ArtifactRenderTestMenu::ArtifactRenderTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Render Test");
 setIcon(QIcon(resolveIconPath("Studio/testmenu_software_render.svg")));

 addOpenWidgetAction(this, "Software 3D Render Test...", "Studio/testmenu_software_render.svg", []() {
  openTestWidget<ArtifactSoftwareRenderTestWidget>(960, 600);
 });
 addOpenWidgetAction(this, "Software Composition Test...", "Studio/testmenu_software_composition.svg", []() {
  openTestWidget<ArtifactSoftwareCompositionTestWidget>(1100, 760);
 });
 addOpenWidgetAction(this, "Software Layer Test...", "Studio/testmenu_software_layer.svg", []() {
  openTestWidget<ArtifactSoftwareLayerTestWidget>(1100, 820);
 });
 addOpenWidgetAction(this, "Layer Composite Test...", "Studio/testmenu_layer_composite.svg", []() {
  openTestWidget<ArtifactLayerCompositeTestWidget>(900, 700);
 });
 addOpenWidgetAction(this, "Timeline Layer Test...", "Studio/testmenu_timeline_layer.svg", []() {
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
 setIcon(QIcon(resolveIconPath("Studio/menubar_test.svg")));

 addOpenWidgetAction(this, "Scroll PoC...", "Studio/testmenu_scroll.svg", []() {
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
 setIcon(QIcon(resolveIconPath("Studio/testmenu_play_arrow.svg")));
}

ArtifactMediaTestMenu::~ArtifactMediaTestMenu()
{
}

ArtifactTestMenu::ArtifactTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent)
{
 setTitle("Test");
 setIcon(QIcon(resolveIconPath("Studio/menubar_test.svg")));

 auto* renderMenu = new ArtifactRenderTestMenu(this);
 auto* widgetMenu = new ArtifactWidgetTestMenu(this);
 auto* mediaMenu = new ArtifactMediaTestMenu(this);
 auto* imageProcessingMenu = new ArtifactImageProcessingTestMenu(this);

 addMenu(renderMenu);
 addMenu(widgetMenu);
 addMenu(mediaMenu);
 addMenu(imageProcessingMenu);

 addSeparator();

 auto *addDebugBlendLayersAction =
     new QAction("Add Debug Blend Test Layers", this);
 addDebugBlendLayersAction->setIcon(
     QIcon(resolveIconPath("Studio/testmenu_layer_composite.svg")));
 addAction(addDebugBlendLayersAction);
 QObject::connect(addDebugBlendLayersAction, &QAction::triggered, this, []() {
  auto *projectService = ArtifactProjectService::instance();
  if (!projectService) {
   QMessageBox::warning(nullptr, "Debug Layers",
                        "ProjectService が利用できません。");
   return;
  }

  auto comp = projectService->currentComposition().lock();
  if (!comp) {
   QMessageBox::warning(nullptr, "Debug Layers",
                        "先にコンポジションを開いてください。");
   return;
  }

  const QSize compSize = comp->effectiveCompositionSize().isValid()
                             ? comp->effectiveCompositionSize()
                             : QSize(1920, 1080);
  const CompositionID compositionId = comp->id();

  ArtifactAbstractLayerPtr lastCreatedLayer;
  lastCreatedLayer = addDebugSolidBlendLayer(
      compositionId, QStringLiteral("Debug Base Plate"), compSize,
      FloatColor(0.32f, 0.32f, 0.36f, 1.0f), LAYER_BLEND_TYPE::BLEND_NORMAL,
      1.0f);
  if (!lastCreatedLayer) {
   QMessageBox::warning(nullptr, "Debug Layers",
                        "デバッグ用ベースレイヤーの追加に失敗しました。");
   return;
  }

  lastCreatedLayer = addDebugSolidBlendLayer(
      compositionId, QStringLiteral("Debug Multiply Plate"), compSize,
      FloatColor(0.78f, 0.42f, 0.18f, 1.0f), LAYER_BLEND_TYPE::BLEND_MULTIPLY,
      0.58f);
  if (!lastCreatedLayer) {
   QMessageBox::warning(nullptr, "Debug Layers",
                        "Multiply テストレイヤーの追加に失敗しました。");
   return;
  }

  lastCreatedLayer = addDebugSolidBlendLayer(
      compositionId, QStringLiteral("Debug Screen Plate"), compSize,
      FloatColor(0.18f, 0.72f, 0.98f, 1.0f), LAYER_BLEND_TYPE::BLEND_SCREEN,
      0.52f);
  if (!lastCreatedLayer) {
   QMessageBox::warning(nullptr, "Debug Layers",
                        "Screen テストレイヤーの追加に失敗しました。");
   return;
  }

  projectService->selectLayer(lastCreatedLayer->id());
  QMessageBox::information(
      nullptr, "Debug Layers",
      QStringLiteral("Debug blend test layers を追加しました。\n\n"
                     "- Debug Base Plate\n"
                     "- Debug Multiply Plate\n"
                     "- Debug Screen Plate\n\n"
                     "タイムライン上で並び替えたり、不透明度を変えて合成検証できます。"));
 });

 addSeparator();

 auto* startSoftwareTestPipelineAction = new QAction("Software Test Pipeline を開始", this);
 startSoftwareTestPipelineAction->setIcon(QIcon(resolveIconPath("Studio/testmenu_pipeline.svg")));
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
