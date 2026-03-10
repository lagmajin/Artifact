module;
#include <wobjectimpl.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <QWidget>
#include <QColor>
#include <QStatusBar>
#include <QMessageBox>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QList>
#include <QTimer>

module Artifact.MainWindow;

import Artifact.MainWindow;
import Menu.MenuBar;
import Widgets.ToolBar;
import Widgets.Dock.StyleManager;

namespace Artifact {

using namespace ads;

W_OBJECT_IMPL(ArtifactMainWindow)

class ArtifactMainWindow::Impl {
public:
 CDockManager* dockManager = nullptr;
 DockStyleManager* dockStyleManager = nullptr;
 QWidget* centralWidgetHost = nullptr;
 QList<CDockWidget*> dockWidgets;
 bool menuBarInitialized = false;
};

ArtifactMainWindow::ArtifactMainWindow(QWidget* parent)
 : QMainWindow(parent), impl_(new Impl())
{
 //CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
 //CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden, true);

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
ads--CDockWidgetTab {
 background: #2b2b2b;
 color: #a9a9a9;
 border: 1px solid #333333;
 border-bottom: none;
 border-top-left-radius: 3px;
 border-top-right-radius: 3px;
 padding: 4px 10px;
}
ads--CDockWidgetTab:hover {
 background: #343434;
 color: #d0d0d0;
}
ads--CDockWidgetTab[activeTab="true"] {
 background: #1e1e1e;
 color: #f0f0f0;
 border-color: #4c4c4c;
}
ads--CDockWidgetTab QAbstractButton#tabCloseButton,
ads--CDockWidgetTab QPushButton#tabCloseButton {
 background: transparent;
 border: 1px solid transparent;
 border-radius: 6px;
 color: #a8a8a8;
 min-width: 14px;
 min-height: 14px;
 max-width: 14px;
 max-height: 14px;
 padding: 0px;
 margin-left: 6px;
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

 statusBar()->showMessage(QStringLiteral("Ready"), 2000);
 resize(1280, 900);
}

ArtifactMainWindow::~ArtifactMainWindow()
{
 delete impl_;
}

void ArtifactMainWindow::addWidget()
{
}

void ArtifactMainWindow::addDockedWidget(const QString& title, ads::DockWidgetArea area, QWidget* widget)
{
 if (!impl_ || !impl_->dockManager || !widget) return;
 auto* dock = new CDockWidget(title, this);
 dock->setObjectName(title);
 dock->setWidget(widget);
 impl_->dockManager->addDockWidget(area, dock);
 impl_->dockWidgets.push_back(dock);
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
}

}
