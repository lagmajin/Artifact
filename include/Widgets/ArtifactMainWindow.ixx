module ;

#include <ads_globals.h>
#include <wobjectcpp.h>

#include <QtWidgets/QMainWindow>


export module ArtifactMainWindow;

export namespace Artifact {
 
 //struct ArtifactMainWindowPrivate;

 class ArtifactMainWindow :public QMainWindow {
  W_OBJECT(ArtifactMainWindow)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void keyPressEvent(QKeyEvent* event) override;

  void keyReleaseEvent(QKeyEvent* event) override;

 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public slots:
  void addWidget();
  void addDockedWidget(const QString& title, ads::DockWidgetArea area,QWidget* widget);

  void closeAllDocks();
  void showStatusMessage(const QString& message, int timeoutMs = 2000);
  void togglePanelsVisible(bool visible);
 };







};