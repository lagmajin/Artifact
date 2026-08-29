module;

#include <QColor>
#include <QEvent>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWindow>
#include <QtGlobal>

export module Artifact.Widgets.Timeline.DiligentRenderWindow;

export namespace Artifact {

struct DiligentTimelineRectVisual {
  QRectF rect;
  QColor color;
};

struct DiligentTimelineLineVisual {
  QPointF from;
  QPointF to;
  QColor color;
  float thickness = 1.0f;
};

struct DiligentTimelineTriangleVisual {
  QPointF p0;
  QPointF p1;
  QPointF p2;
  QColor color;
};

struct DiligentTimelineVisualSnapshot {
  QColor background{38, 40, 46};
  QVector<DiligentTimelineRectVisual> rects;
  QVector<DiligentTimelineLineVisual> lines;
  QVector<DiligentTimelineTriangleVisual> triangles;
  quint64 generation = 0;
};

// Parallel, display-only GPU timeline surface. The existing QWidget/QPainter
// timeline remains the authoritative editing and fallback path.
class ArtifactDiligentTimelineRenderWindow final : public QWindow {
private:
  class Impl;
  Impl* impl_;

protected:
  bool event(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void exposeEvent(QExposeEvent* event) override;

public:
  explicit ArtifactDiligentTimelineRenderWindow(QWindow* parent = nullptr);
  ~ArtifactDiligentTimelineRenderWindow() override;
  void setSnapshot(const DiligentTimelineVisualSnapshot& snapshot);
  quint64 snapshotGeneration() const;
  bool initialize();
  bool isGpuReady() const;
  void requestRender();
};

} // namespace Artifact
