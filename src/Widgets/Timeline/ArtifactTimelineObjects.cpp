module;
#include <QColor>
#include <QGraphicsItem>
module Artifact.Timeline.Objects;


namespace Artifact
{

 ResizeHandle::ResizeHandle(Side s, QGraphicsItem* parent) : QGraphicsRectItem(parent), side(s)
 {
  setRect(0, 0, 6, parent->boundingRect().height());
  //setBrush(Qt::gray);
  //setCursor(s == Left ? Qt::SizeHorCursor : Qt::SizeHorCursor);
  setFlag(ItemIsMovable);
  setFlag(ItemSendsGeometryChanges);
 }

 QVariant ResizeHandle::itemChange(GraphicsItemChange change, const QVariant& value)
 {
  if (change == ItemPositionChange) {
   // 親のサイズ変更ロジックへ通知
  }
  return QGraphicsRectItem::itemChange(change, value);
 }






 ClipItem::ClipItem(double start, double duration, double height) : QGraphicsRectItem(start, 0, duration, height)
 {
  //setBrush(QColor(70, 120, 180));
  setFlags(
   QGraphicsItem::ItemIsMovable |
   QGraphicsItem::ItemIsSelectable |
   QGraphicsItem::ItemSendsGeometryChanges
  );
 }

 ClipItem::~ClipItem()
 {

 }

};