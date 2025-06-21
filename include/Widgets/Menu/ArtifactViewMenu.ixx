module;
#include <QMenu>

export module Menu:View;
//#pragma once

//#include <QtWidgets/QMenu>




export namespace Artifact {

 class ArtifactViewMenuPrivate;

 class ArtifactViewMenu :public QMenu{
  //Q_OBJECT
 private:

 public:
  explicit ArtifactViewMenu(QWidget*parent=nullptr);
  ~ArtifactViewMenu();
 signals:
  
 private slots:

 public slots:
  void registerView(const QString& name, QWidget* view);
 };







};