module;
#include <utility>

#include <wobjectdefs.h>
#include <QFile>
#include <QWidget>
#include <QEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFocusEvent>
export module Artifact.Contents.Viewer;


export namespace Artifact
{
 enum class ContentsViewerMode {
  Source,
  Final,
  Compare
 };

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
  void setViewerMode(ContentsViewerMode mode);
  void setViewerAssignment(int viewerIndex);
  int viewerAssignment() const;
  void assignCompareSourceA();
  void assignCompareSourceB();
 public/*slots*/:
  void play(); W_SLOT(play);
  void pause(); W_SLOT(pause);
  void stop(); W_SLOT(stop);
  void playRange(int64_t start, int64_t end); W_SLOT(playRange);
  void rotateLeft(); W_SLOT(rotateLeft);
  void rotateRight(); W_SLOT(rotateRight);
  void resetView(); W_SLOT(resetView);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
 };

};
