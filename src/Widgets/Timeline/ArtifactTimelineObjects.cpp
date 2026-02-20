module;
#include <QColor>
#include <wobjectimpl.h>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QVariant>
#include <QPointF>
#include <QRectF>
#include <QBrush>
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
    // Drag/ghost support
    bool isDragging = false;
    QGraphicsRectItem* ghostRect = nullptr;
    QPointF dragStartScenePos;
    QPointF dragStartItemPos;
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

W_OBJECT_IMPL(ClipItem)

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
 
 // Draw main clip rectangle
 QRectF rect = boundingRect();
 QColor fillColor = QColor(70, 120, 180);
 
 // Highlight selected clips
 if (isSelected()) {
  fillColor = QColor(100, 150, 220);
 }
 
 painter->setPen(Qt::NoPen);
 painter->setBrush(fillColor);
 painter->drawRect(rect);
 
 // Draw border
 QPen borderPen(isSelected() ? QColor(255, 200, 0) : QColor(50, 90, 150));
 borderPen.setWidth(isSelected() ? 2 : 1);
 painter->setPen(borderPen);
 painter->setBrush(Qt::NoBrush);
 painter->drawRect(rect);
 
 painter->restore();
 }

void ClipItem::handleMoved(ResizeHandle::Side side, qreal sceneX)
{
    if (!impl_) {
        return;
    }
    
    double newStart = impl_->start;
    double newDuration = impl_->duration;
    
    if (side == ResizeHandle::Left) {
        // Left handle: adjust start and duration
        double delta = sceneX - (impl_->start + mapToScene(pos()).x());
        newStart = std::max(0.0, impl_->start + delta);
        newDuration = impl_->duration - (newStart - impl_->start);
        
        // Maintain minimum duration
        if (newDuration < impl_->minDuration) {
            newStart = impl_->start + (impl_->duration - impl_->minDuration);
            newDuration = impl_->minDuration;
        }
    } else if (side == ResizeHandle::Right) {
        // Right handle: adjust duration only
        double newEnd = sceneX;
        newDuration = std::max(impl_->minDuration, newEnd - impl_->start);
    }
    
    setStartDuration(newStart, newDuration);
    update();
}

void ClipItem::setStartDuration(double start, double duration)
{
    if (!impl_) {
        return;
    }
    
    impl_->start = std::max(0.0, start);
    impl_->duration = std::max(impl_->minDuration, duration);
    
    // Update position
    setPos(impl_->start, pos().y());
    
    // Update right handle position
    if (impl_->rightHandle) {
        impl_->rightHandle->setPos(impl_->duration, 0);
    }
    
    // Update bounding rect
    prepareGeometryChange();
}

double ClipItem::getStart() const
{
    return impl_ ? impl_->start : 0.0;
}

double ClipItem::getDuration() const
{
    return impl_ ? impl_->duration : 0.0;
}

void ClipItem::setStart(double start)
{
    if (impl_) {
        impl_->start = std::max(0.0, start);
        setPos(impl_->start, pos().y());
        update();
    }
}

void ClipItem::setDuration(double duration)
{
    if (impl_) {
        impl_->duration = std::max(impl_->minDuration, duration);
        if (impl_->rightHandle) {
            impl_->rightHandle->setPos(impl_->duration, 0);
        }
        prepareGeometryChange();
        update();
    }
}

void ClipItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    impl_->dragStartScenePos = event->scenePos();
    impl_->dragStartItemPos = pos();
    impl_->isDragging = false;
    // toggle selection on click
    setSelected(!isSelected());
    QGraphicsObject::mousePressEvent(event);
}

void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    QPointF delta = event->scenePos() - impl_->dragStartScenePos;
    // start drag if moved beyond threshold
    if (!impl_->isDragging && (std::abs(delta.x()) > 4 || std::abs(delta.y()) > 4)) {
        impl_->isDragging = true;
        // create ghost rect
        if (!impl_->ghostRect && scene()) {
            impl_->ghostRect = new QGraphicsRectItem(boundingRect());
            impl_->ghostRect->setBrush(QBrush(QColor(70, 120, 180, 100)));
            impl_->ghostRect->setPen(Qt::NoPen);
            impl_->ghostRect->setZValue(1000);
            scene()->addItem(impl_->ghostRect);
        }
        Q_EMIT dragStarted(this);
    }

    if (impl_->isDragging && impl_->ghostRect) {
        QPointF newPos = impl_->dragStartItemPos + QPointF(delta.x(), delta.y());
        impl_->ghostRect->setPos(newPos);
        Q_EMIT dragMoved(this, impl_->ghostRect->scenePos().x(), impl_->ghostRect->scenePos().y());
    } else {
        QGraphicsObject::mouseMoveEvent(event);
    }
}

void ClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    if (impl_->isDragging) {
        // finalize drag
        if (impl_->ghostRect) {
            QPointF endPos = impl_->ghostRect->scenePos();
            Q_EMIT dragEnded(this, endPos.x(), endPos.y());
            scene()->removeItem(impl_->ghostRect);
            delete impl_->ghostRect;
            impl_->ghostRect = nullptr;
        }
        impl_->isDragging = false;
    }
    QGraphicsObject::mouseReleaseEvent(event);
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