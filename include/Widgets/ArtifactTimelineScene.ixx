module;
#include <QGraphicsScene>
export module Artifact.TimelineScene;

export namespace ArtifactCore {

 class ArtifactTimelineScene :public QGraphicsScene
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineScene(QWidget* parent = nullptr);
  void drawBackground(QPainter* painter, const QRectF& rect) override;


 };








};