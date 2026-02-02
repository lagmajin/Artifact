module;
#include <QColor>
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QVariant>
#include <QPointF>
#include <QRectF>
#include <algorithm>

module Artifact.Timeline.Objects;

namespace Artifact
{

// Pimpl for ClipItem
class ClipItem::Impl {
public:
    double start = 0.0;
    double duration = 100.0;
    double height = 20.0;
    double minDuration = 10.0;
    ResizeHandle* leftHandle = nullptr;
    ResizeHandle* rightHandle = nullptr;
    Impl() {}
    ~Impl() {}
};

 ResizeHandle::ResizeHandle(Side s, QGraphicsItem* parent) : QGraphicsObject(parent), side(s)
 {
  //setRect(0, 0, 6, parent->boundingRect().height());
  //setBrush(Qt::gray);
  //setCursor(s == Left ? Qt::SizeHorCursor : Qt::SizeHorCursor);
  setFlag(ItemIsMovable);
  setFlag(ItemSendsGeometryChanges);
  setCursor(Qt::SizeHorCursor);
 }

QRectF ResizeHandle::boundingRect() const {
    return QRectF(0, 0, 6, parentItem() ? parentItem()->boundingRect().height() : 20);
}

void ResizeHandle::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(200, 200, 200));
    painter->drawRect(boundingRect());
    painter->restore();
}

QVariant ResizeHandle::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemPositionChange && parentItem()) {
        // Notify parent clip about handle move
        auto clip = dynamic_cast<ClipItem*>(parentItem());
        if (clip) {
            QPointF newPos = value.toPointF();
            // newPos is in parent's coordinates; convert to scene X
            qreal sceneX = parentItem()->mapToScene(newPos).x();
            clip->handleMoved(side, sceneX);
        }
    }
    return QGraphicsObject::itemChange(change, value);
}







 ClipItem::ClipItem(double start, double duration, double height) : QGraphicsObject()
 {
  //setBrush(QColor(70, 120, 180));
  setFlags(
   QGraphicsItem::ItemIsMovable |
   QGraphicsItem::ItemIsSelectable |
   QGraphicsItem::ItemSendsGeometryChanges
  );

    // create impl and resize handles
    impl_ = new Impl();
    impl_->start = start;
    impl_->duration = duration;
    impl_->height = height;
    impl_->minDuration = 10.0;
    impl_->leftHandle = new ResizeHandle(ResizeHandle::Left, this);
    impl_->rightHandle = new ResizeHandle(ResizeHandle::Right, this);
    impl_->leftHandle->setParentItem(this);
    impl_->rightHandle->setParentItem(this);
    // Position this clip
    setStartDuration(start, duration);
 }

 ClipItem::~ClipItem()
 {

 }

 QRectF ClipItem::boundingRect() const
 {
 // Use stored geometry if available; fallback to reasonable default
 if (impl_) return QRectF(0, 0, impl_->duration, impl_->height);
 return QRectF(0, 0, 100, 20);
 }

 void ClipItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget /*= nullptr*/)
 {
 painter->save();
 painter->setPen(Qt::NoPen);
 painter->setBrush(QColor(70, 120, 180));
 painter->drawRect(boundingRect());
 painter->restore();
 }

void ClipItem::handleMoved(ResizeHandle::Side side, qreal sceneX)
{
    // Simple handling: adjust clip width or start based on left/right handle
    // For now just log/placeholder
    Q_UNUSED(side);
    Q_UNUSED(sceneX);
}

void ClipItem::setStartDuration(double start, double duration)
{
    setPos(start, 0);
    // We would set a width internally and update boundingRect; for now rely on transform
    Q_UNUSED(duration);
}

QVariant ClipItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
  // Provide a minimal implementation for itemChange used by the timeline.
  // Return base class behavior where appropriate.
  switch (change) {
  case QGraphicsItem::ItemPositionChange:
  case QGraphicsItem::ItemPositionHasChanged:
  case QGraphicsItem::ItemSelectedChange:
  case QGraphicsItem::ItemSelectedHasChanged:
    return QGraphicsObject::itemChange(change, value);
  default:
    return QGraphicsObject::itemChange(change, value);
  }
}

};