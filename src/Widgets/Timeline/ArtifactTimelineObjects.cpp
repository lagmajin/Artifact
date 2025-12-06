module;
#include <QColor>
#include <QGraphicsItem>
module Artifact.Timeline.Objects;


namespace Artifact
{

 ResizeHandle::ResizeHandle(Side s, QGraphicsItem* parent) : QGraphicsObject(parent), side(s)
 {
  //setRect(0, 0, 6, parent->boundingRect().height());
  //setBrush(Qt::gray);
  //setCursor(s == Left ? Qt::SizeHorCursor : Qt::SizeHorCursor);
  setFlag(ItemIsMovable);
  setFlag(ItemSendsGeometryChanges);
 }







 ClipItem::ClipItem(double start, double duration, double height) : QGraphicsObject()
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

 QRectF ClipItem::boundingRect() const
 {
  throw std::logic_error("The method or operation is not implemented.");
 }

 void ClipItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget /*= nullptr*/)
 {
  throw std::logic_error("The method or operation is not implemented.");
 }

 QVariant ClipItem::itemChange(GraphicsItemChange change, const QVariant& value)
 {
  throw std::logic_error("The method or operation is not implemented.");
 }

};