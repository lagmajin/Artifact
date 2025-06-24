module ;

#include <ads_globals.h>
#include <wobjectcpp.h>

#include <QtWidgets/QMainWindow>


export module ArtifactMainWindow;

export namespace Artifact {
 
 struct ArtifactMainWindowPrivate;

 class ArtifactMainWindow :public QMainWindow {
  W_OBJECT(ArtifactMainWindow)
 private:

 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public slots:
  void addWidget();
  void addDockedWidget(QWidget* widget, const QString& title, ads::DockWidgetArea area);
 signals:
 };







};