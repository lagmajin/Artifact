#pragma once

#include <QtCore/QObject>



namespace Artifact {

 class ArtifactCompositionManagerPrivate;

 class ArtifactCompositionManager:public QObject {
 private:

 public:
  ArtifactCompositionManager();
  ~ArtifactCompositionManager();

  void Search();

 //signals:

  //slots:
 public:
  void CreateNewComposition();
  void RemoveAllComposition();
 };







}