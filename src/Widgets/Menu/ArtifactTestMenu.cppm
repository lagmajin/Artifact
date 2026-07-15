module;
#include <algorithm>
#include <utility>
#include <functional>
#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QWidget>
#include <QToolButton>
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
class WidgetInspectorHighlight final : public QWidget {
public:
 explicit WidgetInspectorHighlight(QWidget *parent = nullptr) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                 Qt::WindowTransparentForInput);
 }

protected:
 void paintEvent(QPaintEvent *) override {
  QPainter painter(this);
  painter.setPen(QPen(QColor(255, 72, 72), 3));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(2, 2, -2, -2));
 }
};

class WidgetInspectorHarness final : public QObject {
public:
 static WidgetInspectorHarness *instance() {
  static WidgetInspectorHarness *harness =
      new WidgetInspectorHarness(QApplication::instance());
  return harness;
 }

 void start() {
  if (active_) {
   return;
  }
  active_ = true;
  QApplication::instance()->installEventFilter(this);
  QApplication::setOverrideCursor(Qt::CrossCursor);
  qInfo() << "[WidgetInspector] armed; click a widget or press Escape";
 }

protected:
 bool eventFilter(QObject *, QEvent *event) override {
  if (!active_) {
   return false;
  }
  if (event->type() == QEvent::KeyPress) {
   auto *keyEvent = static_cast<QKeyEvent *>(event);
   if (keyEvent->key() == Qt::Key_Escape) {
    finish();
    qInfo() << "[WidgetInspector] cancelled";
    return true;
   }
  }
  if (event->type() != QEvent::MouseButtonPress) {
   return false;
  }

  auto *mouseEvent = static_cast<QMouseEvent *>(event);
  QWidget *target = QApplication::widgetAt(mouseEvent->globalPosition().toPoint());
  if (!target || target == highlight_.data()) {
   finish();
   return true;
  }

  const QString report = describe(target);
  showHighlight(target);
  qInfo().noquote() << "[WidgetInspector]\n" << report;
  finish(false);
  QMessageBox::information(nullptr, QStringLiteral("Widget Inspector"), report);
  if (highlight_) {
   highlight_->hide();
  }
  return true;
 }

private:
 explicit WidgetInspectorHarness(QObject *parent) : QObject(parent) {}

 QString describe(QWidget *widget) const {
  QStringList hierarchy;
  for (QWidget *current = widget; current; current = current->parentWidget()) {
   const QString name = current->objectName().isEmpty()
                            ? QStringLiteral("<unnamed>")
                            : current->objectName();
   hierarchy.push_back(QStringLiteral("%1 (%2)")
                           .arg(QString::fromLatin1(current->metaObject()->className()),
                                name));
  }
  QStringList controlDetails;
  if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
   controlDetails << QStringLiteral("Text: %1")
                         .arg(button->text().isEmpty()
                                  ? QStringLiteral("<empty>")
                                  : button->text());
   controlDetails << QStringLiteral("Checkable: %1  Checked: %2")
                         .arg(button->isCheckable())
                         .arg(button->isChecked());
  }
  if (auto *toolButton = qobject_cast<QToolButton *>(widget)) {
   QAction *defaultAction = toolButton->defaultAction();
   controlDetails << QStringLiteral("Default action: %1")
                         .arg(defaultAction
                                  ? (defaultAction->text().isEmpty()
                                         ? QStringLiteral("<unnamed action>")
                                         : defaultAction->text())
                                  : QStringLiteral("<none>"));
  }
  controlDetails << QStringLiteral("Tooltip: %1")
                        .arg(widget->toolTip().isEmpty()
                                 ? QStringLiteral("<empty>")
                                 : widget->toolTip());
  controlDetails << QStringLiteral("QObject parent: %1")
                        .arg(widget->parent()
                                 ? QString::fromLatin1(
                                       widget->parent()->metaObject()->className())
                                 : QStringLiteral("<none>"));
  controlDetails << QStringLiteral("Window flags: 0x%1")
                        .arg(static_cast<qulonglong>(widget->windowFlags()),
                             0, 16);

  return QStringLiteral(
             "Class: %1\nObject name: %2\nGeometry: %3, %4  %5 x %6\n"
             "Visible: %7\nEnabled: %8\nWindow: %9\n%10\n\nParent hierarchy:\n%11")
      .arg(QString::fromLatin1(widget->metaObject()->className()),
           widget->objectName().isEmpty() ? QStringLiteral("<unnamed>")
                                          : widget->objectName())
      .arg(widget->x())
      .arg(widget->y())
      .arg(widget->width())
      .arg(widget->height())
      .arg(widget->isVisible())
      .arg(widget->isEnabled())
      .arg(widget->window()->objectName().isEmpty()
               ? QString::fromLatin1(widget->window()->metaObject()->className())
               : widget->window()->objectName())
      .arg(controlDetails.join(QStringLiteral("\n")))
      .arg(hierarchy.join(QStringLiteral("\n  -> ")));
 }

 void showHighlight(QWidget *target) {
  if (!highlight_) {
   highlight_ = new WidgetInspectorHighlight();
  }
  const QRect globalRect(target->mapToGlobal(QPoint(0, 0)), target->size());
  highlight_->setGeometry(globalRect.adjusted(-3, -3, 3, 3));
  highlight_->show();
  highlight_->raise();
 }

 void finish(bool hideHighlight = true) {
  if (!active_) {
   return;
  }
  active_ = false;
  QApplication::instance()->removeEventFilter(this);
  QApplication::restoreOverrideCursor();
  if (hideHighlight && highlight_) {
   highlight_->hide();
  }
 }

 bool active_ = false;
 QPointer<WidgetInspectorHighlight> highlight_;
};

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

 auto *widgetInspector = addAction(QStringLiteral("Inspect Widget on Next Click"));
 widgetInspector->setObjectName(QStringLiteral("WidgetInspectorAction"));

 auto *lazyDockDiagnostics = addMenu(QStringLiteral("Lazy Dock Initialization"));
 lazyDockDiagnostics->setObjectName(QStringLiteral("LazyDockDiagnostics"));

}

void ArtifactWidgetTestMenu::showEvent(QShowEvent* event)
{
 QMenu::showEvent(event);
 auto *diagnostics = findChild<QMenu *>(QStringLiteral("LazyDockDiagnostics"),
                                       Qt::FindDirectChildrenOnly);
 if (!diagnostics) {
  return;
 }
 diagnostics->clear();
 QStringList rows;
 int warningCount = 0;
 for (auto *widget : QApplication::allWidgets()) {
  if (!widget || !widget->property("artifactLazyDock").toBool()) {
   continue;
  }
  const bool created = widget->property("artifactLazyWidgetCreated").toBool();
  const bool pending =
      widget->property("artifactLazyWidgetCreationPending").toBool();
  const bool startup =
      widget->property("artifactLazyWidgetStartupPending").toBool();
  const bool factory =
      widget->property("artifactLazyFactoryAvailable").toBool();
  const QString error =
      widget->property("artifactLazyWidgetLastError").toString();
  const bool warning = !error.isEmpty() || (pending && !widget->isVisible()) ||
                       (!created && !factory);
  warningCount += warning ? 1 : 0;
  rows.push_back(
      QStringLiteral("%1 %2 | created=%3 factory=%4 pending=%5 startup=%6 visible=%7%8")
          .arg(warning ? QStringLiteral("WARN") : QStringLiteral("OK"),
               widget->objectName())
          .arg(created)
          .arg(factory)
          .arg(pending)
          .arg(startup)
          .arg(widget->isVisible())
          .arg(error.isEmpty() ? QString()
                               : QStringLiteral(" | %1").arg(error)));
 }
 rows.sort(Qt::CaseInsensitive);
 auto *summary = diagnostics->addAction(
     QStringLiteral("%1 docks, %2 warnings").arg(rows.size()).arg(warningCount));
 summary->setEnabled(false);
 diagnostics->addSeparator();
 if (rows.isEmpty()) {
  auto *empty = diagnostics->addAction(QStringLiteral("No lazy docks registered"));
  empty->setEnabled(false);
 } else {
  for (const auto &row : rows) {
   auto *item = diagnostics->addAction(row);
   item->setEnabled(false);
  }
 }
 qInfo().noquote() << "[LazyDockHarness]" << rows.join(QLatin1Char('\n'));
}

void ArtifactWidgetTestMenu::mouseReleaseEvent(QMouseEvent* event)
{
 auto *action = activeAction();
 if (action && action->objectName() == QStringLiteral("WidgetInspectorAction")) {
  close();
  WidgetInspectorHarness::instance()->start();
  event->accept();
  return;
 }
 QMenu::mouseReleaseEvent(event);
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
