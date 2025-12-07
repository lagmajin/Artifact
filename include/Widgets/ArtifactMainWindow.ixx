module ;

#include <ads_globals.h>
#include <wobjectcpp.h>

#include <QtWidgets/QMainWindow>


export module Artifact.ArtifactMainWindow;

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
 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public /*slots*/:
  void addWidget();
  void addDockedWidget(const QString& title, ads::DockWidgetArea area,QWidget* widget);

  void closeAllDocks();
  void showStatusMessage(const QString& message, int timeoutMs = 2000);
  void togglePanelsVisible(bool visible);
 };







};