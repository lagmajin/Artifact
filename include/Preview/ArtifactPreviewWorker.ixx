module;
#include <wobjectdefs.h>
#include <QObject>
export module Artifact.Preview.Worker;

export namespace Artifact
{
 class ArtifactPreviewWorker :public QObject
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactPreviewWorker(QObject* parent = nullptr);
  ~ArtifactPreviewWorker();
 };


};