#pragma once

#include <QtCore/QObject>



namespace Artifact {

 class ArtifactProjectPrivate;

 class ArtifactProject :public QObject {
  Q_OBJECT
 private:

 public:
  QString projectName() const;
  void setProjectName(const QString&name);
 signals:
  void updated();
  void projectNameChanged(const QString& name);

 public slots:
 };

 void ArtifactProject::setProjectName(const QString& name)
 {

 }


}