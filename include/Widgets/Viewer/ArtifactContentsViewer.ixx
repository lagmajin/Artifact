module;
#include <wobjectdefs.h>
#include <QFile>
#include <QWidget>
#include <QWheelEvent>
#include <QMouseEvent>

export module Artifact.Contents.Viewer;

export namespace Artifact
{
 class ArtifactContentsViewer :public QWidget
 {
 	W_OBJECT(ArtifactContentsViewer)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactContentsViewer(QWidget* parent = nullptr);
  ~ArtifactContentsViewer();
  void setFilePath(const QString& filepath);
 public/*slots*/:
  void play(); W_SLOT(play);
  void pause(); W_SLOT(pause);
  void stop(); W_SLOT(stop);
  void playRange(int64_t start, int64_t end); W_SLOT(playRange);
  void rotateLeft(); W_SLOT(rotateLeft);
  void rotateRight(); W_SLOT(rotateRight);
  void resetView(); W_SLOT(resetView);

 protected:
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
 };

};
