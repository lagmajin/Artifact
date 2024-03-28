#pragma once

#include <QtWidgets/QtWidgets>


namespace Artifact {

 struct ArtifactAssetBrowserPrivate;

 class ArtifactAssetBrowser :public QWidget{
  Q_OBJECT
 private:

 public:
  explicit ArtifactAssetBrowser(QWidget* parent = nullptr);
  ~ArtifactAssetBrowser();
 signals:

 public slots:
 };






};