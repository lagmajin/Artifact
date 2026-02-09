module;
#include <QGraphicsScene>


module Artifact.TimelineScene;


namespace ArtifactCore {

 void ArtifactTimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
 {
  //painter->fillRect(rect, Qt::darkGray);
 }

 ArtifactTimelineScene::ArtifactTimelineScene(QWidget* parent/*=nullptr*/) :QGraphicsScene(nullptr)
 {
  // シーンサイズは親のリサイズイベントで動的に更新される
  setSceneRect(0, 0, 10000, 600);
 }


};