#pragma once

#include <memory>


#include <QtCore/QObject>



namespace Artifact {

 class ArtifactProjectManagerPrivate;

 class ArtifactProjectManager :public QObject {
 private:

 public:
  ArtifactProjectManager();
  ~ArtifactProjectManager();
 signals:
  void projectSettingChanged();
 public slots:

 };




};