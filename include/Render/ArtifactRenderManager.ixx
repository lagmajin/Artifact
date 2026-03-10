module;
#include <QString>
#include <QSize>

export module Artifact.Render.Manager;

export namespace Artifact {

 struct DummyRenderRequest {
  QString compositionId;
  QString compositionName;
  QSize frameSize = QSize(1920, 1080);
  QString outputDirectory;
 };

 struct DummyRenderResult {
  bool success = false;
  QString outputPath;
  QString message;
 };

 class ArtifactRenderManager {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactRenderManager();
  ~ArtifactRenderManager();

  static ArtifactRenderManager& instance();

  void enqueue(const DummyRenderRequest& request);
  int queueSize() const;
  void clearQueue();

  DummyRenderResult renderDummyImage(const DummyRenderRequest& request);
 };
};
