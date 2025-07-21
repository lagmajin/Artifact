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
 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public slots:
  void addWidget();
  void addDockedWidget(const QString& title, ads::DockWidgetArea area,QWidget* widget);
  //void addDockedWidget()
 signals:
 
 
 };







};