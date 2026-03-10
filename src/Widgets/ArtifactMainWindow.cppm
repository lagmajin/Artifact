module;
#include <wobjectimpl.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <QWidget>
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

namespace Artifact {

using namespace ads;

W_OBJECT_IMPL(ArtifactMainWindow)

class ArtifactMainWindow::Impl {
public:
 CDockManager* dockManager = nullptr;
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
