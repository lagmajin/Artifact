module;
#include <QObject>
#include <QString>
export module Artifact.PreviewController;

import std;
import Frame.Position;

export namespace Artifact
{

 class ArtifactPreviewController :public QObject
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit ArtifactPreviewController();
  ~ArtifactPreviewController();

  void play();
  void stop();
 	
 };






};