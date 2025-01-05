#pragma once

#include <memory>

#include <QtCore/QObject>



namespace Artifact {

 class ArtifactProjectPrivate;

 class ArtifactProject :public QObject {
  Q_OBJECT
 private:
  std::unique_ptr<ArtifactProjectPrivate> pImpl_;

 public:
  ArtifactProject();
  ~ArtifactProject();
  QString projectName() const;
  void setProjectName(const QString&name);
 signals:
  void updated();
  void projectNameChanged(const QString& name);

 public slots:
 };


}