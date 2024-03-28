#pragma once

#include <QtWidgets/QMainWindow>


namespace Artifact {
 
 struct ArtifactMainWindowPrivate;

 class ArtifactMainWindow :public QMainWindow {
  Q_OBJECT
 private:

 public:
  explicit ArtifactMainWindow(QWidget* parent = nullptr);
  ~ArtifactMainWindow();
 public slots:

 signals:
 };







};