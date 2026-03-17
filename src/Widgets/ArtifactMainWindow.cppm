module;
#include <wobjectimpl.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>
#include <FloatingDockContainer.h>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QWidget>
#include <QColor>
#include <QHeaderView>
#include <QStatusBar>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QCloseEvent>
#include <QShowEvent>
#include <QList>
#include <QTimer>
#include <QTreeView>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef MessageBox
#endif

module Artifact.MainWindow;

import Artifact.MainWindow;
import Artifact.Widgets.ProjectManagerWidget;
import Menu.MenuBar;
import Widgets.ToolBar;
import Widgets.Dock.StyleManager;
import Artifact.Widgets.AppDialogs;

namespace Artifact {

using namespace ads;

namespace
{
#if defined(_WIN32)
using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

void applyDarkNativeTitleBar(QWidget* widget)
{
 if (!widget) {
  return;
 }

 const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
 if (!hwnd) {
  return;
 }

 static HMODULE dwmModule = ::LoadLibraryW(L"dwmapi.dll");
 if (!dwmModule) {
  return;
 }

 static const auto setWindowAttribute =
  reinterpret_cast<DwmSetWindowAttributeFn>(::GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
 if (!setWindowAttribute) {
  return;
 }

 const BOOL darkModeEnabled = TRUE;
 const DWORD darkModeAttributes[] = {20u, 19u};
 for (const DWORD attribute : darkModeAttributes) {
  setWindowAttribute(hwnd, attribute, &darkModeEnabled, sizeof(darkModeEnabled));
 }

 const COLORREF captionColor = RGB(36, 43, 52);
 const COLORREF textColor = RGB(236, 242, 248);
 const COLORREF borderColor = RGB(74, 96, 122);
 setWindowAttribute(hwnd, 35u, &captionColor, sizeof(captionColor));
 setWindowAttribute(hwnd, 36u, &textColor, sizeof(textColor));
 setWindowAttribute(hwnd, 34u, &borderColor, sizeof(borderColor));
}
#else
void applyDarkNativeTitleBar(QWidget*) {}
#endif

void refreshFloatingWidgetTree(QWidget* widget)
{
 if (!widget) {
  return;
 }

 // With WA_OpaquePaintEvent removed from the project panel hierarchy,
 // Qt's backing store now properly clears newly exposed areas during
 // resize.  We only need to ensure QTreeView's internal item layout
 // is up-to-date and schedule a normal (deferred) repaint.
 //
 // Avoid layout->activate(), forced repaint(), or updateGeometry()
 // on children — these fight against Qt's own layout propagation
 // and can produce stale-geometry artifacts during live resize.

 for (auto* projectView : widget->findChildren<Artifact::ArtifactProjectView*>()) {
  if (projectView) {
   projectView->refreshVisibleContent();
  }
 }

 widget->update();
}

ads::CFloatingDockContainer* findFloatingDockContainer(QWidget* widget)
{
 QWidget* cursor = widget;
 while (cursor) {
  if (auto* floatingWidget = qobject_cast<ads::CFloatingDockContainer*>(cursor)) {
   return floatingWidget;
  }
  cursor = cursor->parentWidget();
 }
 return nullptr;
}

void refreshDockWidgetSurface(ads::CDockWidget* dock)
{
 if (!dock) {
  return;
 }

 dock->update();

 if (auto* tab = dock->tabWidget()) {
  tab->updateStyle();
  tab->update();
 }

 if (auto* content = dock->widget()) {
  refreshFloatingWidgetTree(content);
 }
}

void scheduleFloatingRefresh(ads::CFloatingDockContainer* floatingWidget)
{
 if (!floatingWidget) {
  return;
 }

 if (floatingWidget->property("artifactFloatingRefreshScheduled").toBool()) {
  return;
 }

 floatingWidget->setProperty("artifactFloatingRefreshScheduled", true);
 QTimer::singleShot(0, floatingWidget, [floatingWidget]() {
  floatingWidget->setProperty("artifactFloatingRefreshScheduled", false);
  refreshFloatingWidgetTree(floatingWidget);
  QTimer::singleShot(16, floatingWidget, [floatingWidget]() {
   refreshFloatingWidgetTree(floatingWidget);
  });
 });
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer* floatingWidget, QObject* eventFilterOwner);

void wireDockWidgetSignals(ads::CDockWidget* dock, QObject* owner)
{
 if (!dock || !owner || dock->property("artifactFloatingHooksInstalled").toBool()) {
  return;
 }

 dock->setProperty("artifactFloatingHooksInstalled", true);

 QObject::connect(dock, &ads::CDockWidget::topLevelChanged, owner, [dock, owner](bool) {
  refreshDockWidgetSurface(dock);
  if (auto* floatingWidget = findFloatingDockContainer(dock)) {
   prepareFloatingDockContainer(floatingWidget, owner);
  }
 });

 QObject::connect(dock, &ads::CDockWidget::visibilityChanged, owner, [dock](bool) {
  refreshDockWidgetSurface(dock);
  if (auto* floatingWidget = findFloatingDockContainer(dock)) {
   scheduleFloatingRefresh(floatingWidget);
  }
 });
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer* floatingWidget, QObject* eventFilterOwner)
{
 if (!floatingWidget) {
  return;
 }

 if (eventFilterOwner) {
  floatingWidget->removeEventFilter(eventFilterOwner);
  floatingWidget->installEventFilter(eventFilterOwner);
 }

 QTimer::singleShot(0, floatingWidget, [floatingWidget]() {
  applyDarkNativeTitleBar(floatingWidget);
  floatingWidget->setAttribute(Qt::WA_StyledBackground, true);
  scheduleFloatingRefresh(floatingWidget);
 });
}
}

W_OBJECT_IMPL(ArtifactMainWindow)

class ArtifactMainWindow::Impl {
public:
 CDockManager* dockManager = nullptr;
 DockStyleManager* dockStyleManager = nullptr;
 QWidget* centralWidgetHost = nullptr;
 CDockWidget* primaryCenterDock = nullptr;
 bool primaryCenterDockAssigned = false;
 QList<CDockWidget*> dockWidgets;
 bool menuBarInitialized = false;
};

ArtifactMainWindow::ArtifactMainWindow(QWidget* parent)
 : QMainWindow(parent), impl_(new Impl())
{
 CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
 //CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);
 CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);

 QTimer::singleShot(0, this, [this]() {
  if (!impl_ || impl_->menuBarInitialized) return;
  auto* menuBar = new ArtifactMenuBar(this, this);
  setMenuBar(menuBar);
  impl_->menuBarInitialized = true;
 });

 auto* toolBar = new ArtifactToolBar(this);
 addToolBar(toolBar);

 impl_->dockManager = new CDockManager(this);
 impl_->dockStyleManager = new DockStyleManager(impl_->dockManager, this);
 if (qApp) {
  qApp->installEventFilter(this);
 }
 QObject::connect(impl_->dockManager, &CDockManager::floatingWidgetCreated, this, [this](ads::CFloatingDockContainer* floatingWidget) {
  prepareFloatingDockContainer(floatingWidget, this);
 });
 impl_->dockStyleManager->setGlowEnabled(true);
 impl_->dockStyleManager->setGlowColor(QColor(232, 120, 41));
 impl_->dockStyleManager->setGlowWidth(2);
 impl_->dockStyleManager->setGlowIntensity(0.72f);
impl_->dockManager->setStyleSheet(R"(
ads--CDockAreaWidget {
 background: #252525;
}
ads--CDockAreaTitleBar {
 background: #2e2e2e;
 border-bottom: 1px solid #1a1a1a;
}
ads--CDockAreaTitleBar QLabel {
 color: #c8c8c8;
 padding-left: 4px;
 font-weight: 600;
}
ads--CDockAreaTitleBar QToolButton {
 background: transparent;
 border: 1px solid transparent;
 border-radius: 4px;
 color: #b0b0b0;
 min-width: 18px;
 min-height: 18px;
 padding: 1px;
}
ads--CDockAreaTitleBar QToolButton:hover {
 background: #484848;
 border-color: #606060;
 color: #e8e8e8;
}
ads--CDockAreaTitleBar QToolButton:pressed {
 background: #3a3a3a;
 border-color: #787878;
}
ads--CDockAreaTitleBar QToolButton#tabsMenuButton,
ads--CDockAreaTitleBar QToolButton#undockButton,
ads--CDockAreaTitleBar QToolButton#closeButton {
 min-width: 20px;
 min-height: 20px;
 max-width: 20px;
 max-height: 20px;
 border-radius: 5px;
 margin-left: 3px;
 padding: 0px;
}
ads--CDockAreaTitleBar QToolButton#tabsMenuButton:hover,
ads--CDockAreaTitleBar QToolButton#undockButton:hover {
 background: #525252;
 border-color: #707070;
 color: #ffffff;
}
ads--CDockAreaTitleBar QToolButton#closeButton:hover {
 background: #7a3030;
 border-color: #c05050;
 color: #ffcccc;
}
ads--CDockAreaTitleBar QToolButton#tabsMenuButton:pressed,
ads--CDockAreaTitleBar QToolButton#undockButton:pressed,
ads--CDockAreaTitleBar QToolButton#closeButton:pressed {
 background: #404040;
 border-color: #888888;
}
ads--CFloatingDockContainer {
 background: #222222;
 border: 1px solid #4a4a4a;
}
ads--CFloatingDockContainer ads--CDockAreaWidget {
 background: #222222;
}
ads--CFloatingDockContainer ads--CDockAreaTitleBar {
 background: #363636;
 border-bottom: 1px solid #1a1a1a;
}
ads--CFloatingDockContainer ads--CDockAreaTitleBar QLabel {
 color: #d4d4d4;
}
ads--CDockWidgetTab {
 background: #2a2a2a;
 color: #989898;
 border: 1px solid #393939;
 border-bottom: none;
 border-top-left-radius: 4px;
 border-top-right-radius: 4px;
 padding: 5px 10px 5px 12px;
}
ads--CDockWidgetTab:hover {
 background: #353535;
 color: #d0d0d0;
 border-color: #525252;
}
ads--CDockWidgetTab[activeTab="true"] {
 background: #2a2a2a;
 color: #989898;
 border-color: #393939;
}
ads--CDockWidgetTab[artifactActiveTab="true"] {
 background: #383838;
 color: #f0f0f0;
 border-color: #e87829;
 border-top: 2px solid #e87829;
 font-weight: 600;
}
ads--CDockWidgetTab QLabel,
ads--CDockWidgetTab ads--CElidingLabel {
 background: transparent;
 color: #989898;
 padding-left: 0px;
 padding-right: 0px;
}
ads--CDockWidgetTab[activeTab="true"] QLabel,
ads--CDockWidgetTab[activeTab="true"] ads--CElidingLabel {
 color: #989898;
}
ads--CDockWidgetTab[artifactActiveTab="true"] QLabel,
ads--CDockWidgetTab[artifactActiveTab="true"] ads--CElidingLabel {
 color: #f0f0f0;
 font-weight: 600;
}
ads--CDockWidgetTab[artifactFloatingTab="true"] {
 background: #2e2e2e;
 color: #a0a0a0;
 border-color: #484848;
}
ads--CDockWidgetTab[artifactFloatingTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"] ads--CElidingLabel {
 color: #a0a0a0;
}
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover {
 background: #3c3c3c;
 color: #e0e0e0;
 border-color: #686868;
}
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover ads--CElidingLabel {
 color: #e0e0e0;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] {
 background: #2e2e2e;
 color: #a0a0a0;
 border-color: #484848;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] ads--CElidingLabel {
 color: #a0a0a0;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] {
 background: #404040;
 color: #ffffff;
 border-color: #e87829;
 border-top: 2px solid #e87829;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] ads--CElidingLabel {
 color: #ffffff;
}
ads--CDockWidget[artifactActiveDock="true"] {
 background: #262626;
 border: 2px solid #e87829;
}
ads--CDockWidget[artifactActiveDock="true"] ads--CDockAreaTitleBar {
 background: #333333;
 border-bottom: 1px solid #e87829;
}
ads--CFloatingDockContainer ads--CDockWidget[artifactActiveDock="true"] {
 border: 1px solid #e87829;
 background: #222222;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton,
ads--CDockWidgetTab QPushButton#tabCloseButton {
 background: transparent;
 border: 1px solid transparent;
 border-radius: 5px;
 color: transparent;
 min-width: 0px;
 min-height: 13px;
 max-width: 0px;
 max-height: 13px;
 padding: 0px;
 margin-left: 0px;
 margin-right: 0px;
}
ads--CDockWidgetTab:hover QAbstractButton#tabCloseButton,
ads--CDockWidgetTab:hover QPushButton#tabCloseButton,
ads--CDockWidgetTab[artifactActiveTab="true"] QAbstractButton#tabCloseButton,
ads--CDockWidgetTab[artifactActiveTab="true"] QPushButton#tabCloseButton {
 color: #c0c0c0;
 min-width: 14px;
 max-width: 14px;
 margin-left: 4px;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton:hover,
ads--CDockWidgetTab QPushButton#tabCloseButton:hover {
 background: #5a3030;
 border-color: #a05050;
 color: #ffeeee;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton:pressed,
ads--CDockWidgetTab QPushButton#tabCloseButton:pressed {
 background: #482828;
 border-color: #c06060;
}
)");
 impl_->centralWidgetHost = new QWidget(this);
 auto* centralDock = new CDockWidget(QStringLiteral("Workspace"), this);
 centralDock->setObjectName(QStringLiteral("ArtifactCentralDock"));
 centralDock->setWidget(impl_->centralWidgetHost);
 impl_->dockManager->setCentralWidget(centralDock);
 impl_->primaryCenterDock = centralDock;
 impl_->dockStyleManager->applyStyle();

 statusBar();
 resize(1280, 900);
}

ArtifactMainWindow::~ArtifactMainWindow()
{
 if (qApp) {
  qApp->removeEventFilter(this);
 }
 delete impl_;
}

void ArtifactMainWindow::addWidget()
{
}

void ArtifactMainWindow::addDockedWidget(const QString& title, ads::DockWidgetArea area, QWidget* widget)
{
 if (!impl_ || !impl_->dockManager || !widget) return;
 if (area == ads::CenterDockWidgetArea && impl_->primaryCenterDock && !impl_->primaryCenterDockAssigned) {
  impl_->primaryCenterDock->setWindowTitle(title);
  impl_->primaryCenterDock->setObjectName(title);
  impl_->primaryCenterDock->setWidget(widget);
  impl_->primaryCenterDockAssigned = true;
  if (!impl_->dockWidgets.contains(impl_->primaryCenterDock)) {
   impl_->dockWidgets.push_back(impl_->primaryCenterDock);
  }
  wireDockWidgetSignals(impl_->primaryCenterDock, this);
  impl_->dockStyleManager->applyStyle();
  return;
 }
 auto* dock = new CDockWidget(title, this);
 dock->setObjectName(title);
 dock->setWidget(widget);
 impl_->dockManager->addDockWidget(area, dock);
 impl_->dockWidgets.push_back(dock);
 wireDockWidgetSignals(dock, this);
 impl_->dockStyleManager->applyStyle();
}

void ArtifactMainWindow::addDockedWidgetTabbed(const QString& title, ads::DockWidgetArea area, QWidget* widget, const QString& tabGroupPrefix)
{
 addDockedWidgetTabbedWithId(title, title, area, widget, tabGroupPrefix);
}

void ArtifactMainWindow::addDockedWidgetTabbedWithId(const QString& title, const QString& dockId, ads::DockWidgetArea area, QWidget* widget, const QString& tabGroupPrefix)
{
 if (!impl_ || !impl_->dockManager || !widget) return;

 auto* dock = new CDockWidget(title, this);
 dock->setObjectName(dockId.isEmpty() ? title : dockId);
 dock->setWidget(widget);

 ads::CDockAreaWidget* targetArea = nullptr;
 if (!tabGroupPrefix.isEmpty()) {
  for (auto it = impl_->dockWidgets.crbegin(); it != impl_->dockWidgets.crend(); ++it) {
   auto* existingDock = *it;
   if (!existingDock) continue;
   const QString objectName = existingDock->objectName();
   const QString windowTitle = existingDock->windowTitle();
   if ((objectName == tabGroupPrefix || windowTitle == tabGroupPrefix) &&
       existingDock->dockAreaWidget()) {
    targetArea = existingDock->dockAreaWidget();
    break;
   }
  }
 }

 if (!targetArea && !tabGroupPrefix.isEmpty()) {
  for (auto it = impl_->dockWidgets.crbegin(); it != impl_->dockWidgets.crend(); ++it) {
   auto* existingDock = *it;
   if (!existingDock) continue;
   const QString objectName = existingDock->objectName();
   const QString windowTitle = existingDock->windowTitle();
   if ((objectName.startsWith(tabGroupPrefix) || windowTitle.startsWith(tabGroupPrefix)) &&
       existingDock->dockAreaWidget()) {
    targetArea = existingDock->dockAreaWidget();
    break;
   }
  }
 }

 if (targetArea) {
  impl_->dockManager->addDockWidget(ads::CenterDockWidgetArea, dock, targetArea);
 } else {
  impl_->dockManager->addDockWidget(area, dock);
 }

 impl_->dockWidgets.push_back(dock);
 dock->toggleView(true);
 dock->setAsCurrentTab();
 dock->raise();
 wireDockWidgetSignals(dock, this);
 impl_->dockStyleManager->applyStyle();
}

void ArtifactMainWindow::moveDockToTabGroup(const QString& title, const QString& tabGroupPrefix)
{
 if (!impl_ || !impl_->dockManager || title.isEmpty() || tabGroupPrefix.isEmpty()) return;

 CDockWidget* dockToMove = nullptr;
 ads::CDockAreaWidget* targetArea = nullptr;

 for (auto it = impl_->dockWidgets.crbegin(); it != impl_->dockWidgets.crend(); ++it) {
  auto* existingDock = *it;
  if (!existingDock) continue;

  const QString objectName = existingDock->objectName();
  const QString windowTitle = existingDock->windowTitle();
  if (!dockToMove && (objectName == title || windowTitle == title)) {
   dockToMove = existingDock;
  }
  if (!targetArea &&
      (objectName == tabGroupPrefix || windowTitle == tabGroupPrefix) &&
      existingDock->dockAreaWidget()) {
   targetArea = existingDock->dockAreaWidget();
  }
 }

 if (!targetArea) {
  for (auto it = impl_->dockWidgets.crbegin(); it != impl_->dockWidgets.crend(); ++it) {
   auto* existingDock = *it;
   if (!existingDock) continue;

   const QString objectName = existingDock->objectName();
   const QString windowTitle = existingDock->windowTitle();
   if ((objectName.startsWith(tabGroupPrefix) || windowTitle.startsWith(tabGroupPrefix)) &&
       existingDock->dockAreaWidget()) {
    targetArea = existingDock->dockAreaWidget();
    break;
   }
  }
 }

 if (!dockToMove || !targetArea || dockToMove->dockAreaWidget() == targetArea) {
  return;
 }

 impl_->dockManager->addDockWidgetTabToArea(dockToMove, targetArea);
 dockToMove->toggleView(true);
 impl_->dockStyleManager->applyStyle();
}

void ArtifactMainWindow::setDockVisible(const QString& title, const bool visible)
{
 if (!impl_) return;
 for (auto* dock : impl_->dockWidgets) {
  if (!dock) continue;
  if (dock->objectName() == title || dock->windowTitle() == title) {
   dock->toggleView(visible);
  }
 }
}

void ArtifactMainWindow::activateDock(const QString& title)
{
 if (!impl_) return;
 for (auto* dock : impl_->dockWidgets) {
  if (!dock) continue;
  if (dock->objectName() == title || dock->windowTitle() == title) {
   dock->toggleView(true);
   dock->setAsCurrentTab();
   dock->raise();
   impl_->dockStyleManager->applyStyle();
   return;
  }
 }
}

bool ArtifactMainWindow::closeDock(const QString& title)
{
 if (!impl_ || title.isEmpty()) return false;
 for (auto* dock : impl_->dockWidgets) {
  if (!dock) continue;
  if (dock->objectName() == title || dock->windowTitle() == title) {
   dock->closeDockWidget();
   impl_->dockStyleManager->applyStyle();
   return true;
  }
 }
 return false;
}

void ArtifactMainWindow::closeAllDocks()
{
 if (!impl_) return;
 for (auto* dock : impl_->dockWidgets) {
  if (dock) dock->closeDockWidget();
 }
}

void ArtifactMainWindow::showStatusMessage(const QString& message, int timeoutMs)
{
 statusBar()->showMessage(message, timeoutMs);
}

void ArtifactMainWindow::togglePanelsVisible(bool visible)
{
 if (!impl_) return;
 for (auto* dock : impl_->dockWidgets) {
  if (dock) dock->setVisible(visible);
 }
}

void ArtifactMainWindow::setStatusZoomLevel(float zoomPercent)
{
 statusBar()->showMessage(QStringLiteral("Zoom: %1%").arg(static_cast<int>(zoomPercent)), 1000);
}

void ArtifactMainWindow::setStatusCoordinates(int x, int y)
{
 statusBar()->showMessage(QStringLiteral("X: %1 Y: %2").arg(x).arg(y), 1000);
}

void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB)
{
 statusBar()->showMessage(QStringLiteral("Memory: %1 MB").arg(memoryMB), 1000);
}

void ArtifactMainWindow::setStatusFPS(double fps)
{
 statusBar()->showMessage(QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', 1)), 1000);
}

void ArtifactMainWindow::setStatusReady()
{
 statusBar()->showMessage(QStringLiteral("Ready"), 1500);
}

void ArtifactMainWindow::keyPressEvent(QKeyEvent* event)
{
 QMainWindow::keyPressEvent(event);
}

void ArtifactMainWindow::keyReleaseEvent(QKeyEvent* event)
{
 QMainWindow::keyReleaseEvent(event);
}

void ArtifactMainWindow::closeEvent(QCloseEvent* event)
{
 if (ArtifactMessageBox::confirmAction(this, QStringLiteral("終了"), QStringLiteral("Artifact を終了しますか？"))) {
  event->accept();
 } else {
  event->ignore();
 }
}

void ArtifactMainWindow::showEvent(QShowEvent* event)
{
 QMainWindow::showEvent(event);
 QTimer::singleShot(0, this, [this]() {
  if (!impl_ || !impl_->dockManager) {
   return;
  }
  applyDarkNativeTitleBar(this);
  refreshFloatingWidgetTree(this);
  for (auto* dock : impl_->dockWidgets) {
   wireDockWidgetSignals(dock, this);
   refreshDockWidgetSurface(dock);
  }
  const auto floatingWidgets = impl_->dockManager->floatingWidgets();
  for (auto* floatingWidget : floatingWidgets) {
   prepareFloatingDockContainer(floatingWidget, this);
  }
 });
}

bool ArtifactMainWindow::eventFilter(QObject* watched, QEvent* event)
{
 // 高速パス: フローティングドックに無関係なイベント型は
 // qobject_cast / findFloatingDockContainer の高コスト処理を
 // スキップして即座に返す。
 // ChildAdded / ChildRemoved / LayoutRequest / Polish / PolishRequest は
 // レイアウト処理中に大量発生するためトリガーから除外する。
 if (event) {
  switch (event->type()) {
  case QEvent::Resize:
  case QEvent::Show:
  case QEvent::Hide:
  case QEvent::ActivationChange:
  case QEvent::WindowActivate:
  case QEvent::WindowDeactivate:
  case QEvent::WindowStateChange:
  case QEvent::ZOrderChange:
   break;
  default:
   return QMainWindow::eventFilter(watched, event);
  }
 }

 ads::CFloatingDockContainer* floatingWidget = qobject_cast<ads::CFloatingDockContainer*>(watched);
 if (!floatingWidget) {
  if (auto* watchedWidget = qobject_cast<QWidget*>(watched)) {
   floatingWidget = findFloatingDockContainer(watchedWidget);
  }
 }

 if (floatingWidget) {
  const bool isRootFloatingWidget = (watched == floatingWidget);
  switch (event ? event->type() : QEvent::None) {
  case QEvent::Resize:
   // ルートコンテナのリサイズのみ再描画をスケジュールする。
   // 子ウィジェットのリサイズにも反応すると refreshFloatingWidgetTree 内の
   // layout->invalidate()/activate() が子リサイズを発火させ、イベントフィルタに
   // 戻り、再び refresh をスケジュールする無限カスケードが発生していた。
   if (isRootFloatingWidget) {
    scheduleFloatingRefresh(floatingWidget);
   }
   break;
  case QEvent::Show:
  case QEvent::Hide:
   scheduleFloatingRefresh(floatingWidget);
   break;
  case QEvent::ActivationChange:
  case QEvent::WindowActivate:
  case QEvent::WindowDeactivate:
  case QEvent::WindowStateChange:
  case QEvent::ZOrderChange:
   if (isRootFloatingWidget) {
    scheduleFloatingRefresh(floatingWidget);
   }
   break;
  default:
   break;
  }
 }

 return QMainWindow::eventFilter(watched, event);
}

}
