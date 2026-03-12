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
#include <QStatusBar>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QCloseEvent>
#include <QShowEvent>
#include <QList>
#include <QTimer>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

module Artifact.MainWindow;

import Artifact.MainWindow;
import Menu.MenuBar;
import Widgets.ToolBar;
import Widgets.Dock.StyleManager;

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

 if (auto* layout = widget->layout()) {
  layout->invalidate();
  layout->activate();
 }

 widget->updateGeometry();
 widget->update();

 for (auto* scrollArea : widget->findChildren<QAbstractScrollArea*>()) {
  if (!scrollArea) {
   continue;
  }
  scrollArea->updateGeometry();
  if (scrollArea->viewport()) {
   scrollArea->viewport()->update();
  }
  scrollArea->update();
 }

 for (auto* child : widget->findChildren<QWidget*>()) {
  if (!child || child->isWindow()) {
   continue;
  }
  child->updateGeometry();
  child->update();
 }
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

 dock->updateGeometry();
 dock->update();

 if (auto* tab = dock->tabWidget()) {
  tab->updateStyle();
  tab->updateGeometry();
  tab->update();
  for (auto* child : tab->findChildren<QWidget*>()) {
   if (!child) {
    continue;
   }
   child->updateGeometry();
   child->update();
  }
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
 //CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
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
 impl_->dockStyleManager->setGlowColor(QColor(86, 156, 214));
 impl_->dockStyleManager->setGlowWidth(3);
 impl_->dockStyleManager->setGlowIntensity(0.82f);
impl_->dockManager->setStyleSheet(R"(
ads--CDockAreaWidget {
 background: #1f1f1f;
}
ads--CDockAreaTitleBar {
 background: #232323;
 border-bottom: 1px solid #2d2d2d;
}
ads--CDockAreaTitleBar QLabel {
 color: #cfd9e6;
 padding-left: 4px;
 font-weight: 600;
}
ads--CDockAreaTitleBar QToolButton {
 background: transparent;
 border: 1px solid transparent;
 border-radius: 5px;
 color: #b9c3cf;
 min-width: 18px;
 min-height: 18px;
 padding: 1px;
}
ads--CDockAreaTitleBar QToolButton:hover {
 background: #324055;
 border-color: #4c6686;
 color: #f4f8ff;
}
ads--CDockAreaTitleBar QToolButton:pressed {
 background: #273244;
 border-color: #6d8fb8;
}
ads--CFloatingDockContainer {
 background: #171c24;
 border: 1px solid #41546d;
}
ads--CFloatingDockContainer ads--CDockAreaWidget {
 background: #171c24;
}
ads--CFloatingDockContainer ads--CDockAreaTitleBar {
 background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                             stop:0 #31445b,
                             stop:1 #253344);
 border-bottom: 1px solid #6280a4;
}
ads--CFloatingDockContainer ads--CDockAreaTitleBar QLabel {
 color: #eef5ff;
}
ads--CDockWidgetTab {
 background: #262b31;
 color: #b3bcc7;
 border: 1px solid #3d4652;
 border-bottom: none;
 border-top-left-radius: 4px;
 border-top-right-radius: 4px;
 padding: 5px 10px 5px 12px;
}
ads--CDockWidgetTab:hover {
 background: #313943;
 color: #edf2f7;
 border-color: #58677a;
}
ads--CDockWidgetTab[activeTab="true"] {
 background: #3a444f;
 color: #f4f7fb;
 border-color: #73849a;
}
ads--CDockWidgetTab[artifactActiveTab="true"] {
 background: #4f6478;
 color: #ffffff;
 border-color: #9cb2c7;
 border-top: 2px solid #d1dee9;
 font-weight: 600;
}
ads--CDockWidgetTab QLabel,
ads--CDockWidgetTab ads--CElidingLabel {
 background: transparent;
 color: #b3bcc7;
 padding-left: 0px;
 padding-right: 0px;
}
ads--CDockWidgetTab[activeTab="true"] QLabel,
ads--CDockWidgetTab[activeTab="true"] ads--CElidingLabel {
 color: #f4f7fb;
}
ads--CDockWidgetTab[artifactActiveTab="true"] QLabel,
ads--CDockWidgetTab[artifactActiveTab="true"] ads--CElidingLabel {
 color: #ffffff;
 font-weight: 600;
}
ads--CDockWidgetTab[artifactFloatingTab="true"] {
 background: #232a32;
 color: #b4c0cc;
 border-color: #4a5765;
}
ads--CDockWidgetTab[artifactFloatingTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"] ads--CElidingLabel {
 color: #b4c0cc;
}
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover {
 background: #303b47;
 color: #f3f7fb;
 border-color: #66798e;
}
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"]:hover ads--CElidingLabel {
 color: #f3f7fb;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] {
 background: #435364;
 color: #f7fbff;
 border-color: #8ea4bc;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"][activeTab="true"] ads--CElidingLabel {
 color: #f7fbff;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] {
 background: #5a7086;
 color: #ffffff;
 border-color: #c0d0de;
}
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] QLabel,
ads--CDockWidgetTab[artifactFloatingTab="true"][artifactActiveTab="true"] ads--CElidingLabel {
 color: #ffffff;
}
ads--CDockWidget[artifactActiveDock="true"] {
 background: #1c2430;
 border: 2px solid #569cd6;
}
ads--CDockWidget[artifactActiveDock="true"] ads--CDockAreaTitleBar {
 background: #253246;
 border-bottom: 1px solid #569cd6;
}
ads--CFloatingDockContainer ads--CDockWidget[artifactActiveDock="true"] {
 border: 1px solid #78aee0;
 background: #1a2431;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton,
ads--CDockWidgetTab QPushButton#tabCloseButton {
 background: transparent;
 border: 1px solid transparent;
 border-radius: 6px;
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
ads--CDockWidgetTab[activeTab="true"] QAbstractButton#tabCloseButton,
ads--CDockWidgetTab[activeTab="true"] QPushButton#tabCloseButton,
ads--CDockWidgetTab[artifactActiveTab="true"] QAbstractButton#tabCloseButton,
ads--CDockWidgetTab[artifactActiveTab="true"] QPushButton#tabCloseButton {
 color: #a8a8a8;
 min-width: 13px;
 max-width: 13px;
 margin-left: 3px;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton:hover,
ads--CDockWidgetTab QPushButton#tabCloseButton:hover {
 background: #3a3a3a;
 border-color: #565656;
 color: #f2f2f2;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton:pressed,
ads--CDockWidgetTab QPushButton#tabCloseButton:pressed {
 background: #4a4a4a;
 border-color: #6a6a6a;
}
)");
 impl_->centralWidgetHost = new QWidget(this);
 auto* centralDock = new CDockWidget(QStringLiteral("Workspace"), this);
 centralDock->setObjectName(QStringLiteral("ArtifactCentralDock"));
 centralDock->setWidget(impl_->centralWidgetHost);
 impl_->dockManager->setCentralWidget(centralDock);
 impl_->primaryCenterDock = centralDock;
 impl_->dockStyleManager->applyStyle();

 statusBar()->showMessage(QStringLiteral("Ready"), 2000);
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
 if (!impl_ || !impl_->dockManager || !widget) return;

 auto* dock = new CDockWidget(title, this);
 dock->setObjectName(title);
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
   dock->setVisible(visible);
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
 const auto ret = QMessageBox::question(
  this,
  QStringLiteral("Confirm"),
  QStringLiteral("Close Artifact?"),
  QMessageBox::Yes | QMessageBox::No);
 if (ret == QMessageBox::Yes) {
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
  case QEvent::Show:
  case QEvent::Hide:
   scheduleFloatingRefresh(floatingWidget);
   break;
  case QEvent::ActivationChange:
  case QEvent::ChildAdded:
  case QEvent::ChildRemoved:
  case QEvent::LayoutRequest:
  case QEvent::Polish:
  case QEvent::PolishRequest:
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
