module;
#include <QGraphicsScene>

module Artifact.TimelineScene;

namespace ArtifactCore {

void ArtifactTimelineScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    // painter->fillRect(rect, Qt::darkGray);
}

ArtifactTimelineScene::ArtifactTimelineScene(QWidget* parent/*=nullptr*/) : QGraphicsScene(nullptr)
{
    Q_UNUSED(parent);
    // Scene rect is updated later from parent resize handling.
    setSceneRect(0, 0, 10000, 600);
}

};
