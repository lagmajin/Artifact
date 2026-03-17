module ;


#include <wobjectcpp.h>

#include <QtWidgets/QMainWindow>
#include <memory> // Added for std::unique_ptr if used in Impl
#include <ads_globals.h>


export module Artifact.MainWindow;

import Artifact.Widgets.AudioMixer; // Added
import Audio.Mixer; // Added

import std;

export namespace Artifact {
 
 //struct ArtifactMainWindowPrivate;

 class ArtifactMainWindow :public QMainWindow {
  // ReSharper disable CppInspection
 	W_OBJECT(ArtifactMainWindow)
     // ReSharper restore CppInspection
 private:
  class Impl;
  Impl* impl_;
 protected:
 void keyPressEvent(QKeyEvent* event) override;
 void keyReleaseEvent(QKeyEvent* event) override;
 void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public /*slots*/:
  void addWidget();
  void addDockedWidget(const QString& title, ads::DockWidgetArea area,QWidget* widget);
  void addDockedWidgetTabbed(const QString& title, ads::DockWidgetArea area, QWidget* widget, const QString& tabGroupPrefix);
  void addDockedWidgetTabbedWithId(const QString& title, const QString& dockId, ads::DockWidgetArea area, QWidget* widget, const QString& tabGroupPrefix);
  void moveDockToTabGroup(const QString& title, const QString& tabGroupPrefix);
  void setDockVisible(const QString& title, bool visible);
  void activateDock(const QString& title);
  bool closeDock(const QString& title);

  void closeAllDocks();
  void showStatusMessage(const QString& message, int timeoutMs = 2000);
  void togglePanelsVisible(bool visible);
  
  // Status bar update methods
  void setStatusZoomLevel(float zoomPercent);
  void setStatusCoordinates(int x, int y);
  void setStatusMemoryUsage(uint64_t memoryMB);
  void setStatusFPS(double fps);
  void setStatusReady();
 };







};
