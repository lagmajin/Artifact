module;
#include <QColor>
#include <wobjectimpl.h>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QVariant>
#include <QPointF>
#include <QRectF>
#include <QBrush>

module Artifact.Timeline.Objects;
import std;
import Utils.Id;

namespace Artifact
{
using namespace ArtifactCore;

namespace {
// Prevent recursive resize notifications while ClipItem synchronizes geometry.
thread_local bool g_clipGeometrySyncInProgress = false;
constexpr double kResizeHandleHalfWidth = 4.0;
constexpr double kMinClipDuration = 10.0;
}

// Pimpl for ClipItem
class ClipItem::Impl {
public:
    enum class InteractionState {
        Idle,
        ResizingLeft,
        ResizingRight
    };

    double start = 0.0;
    double duration = 100.0;
    double height = 20.0;
    double minDuration = 10.0;
    LayerID layerId_{};
    ResizeHandle* leftHandle = nullptr;
    ResizeHandle* rightHandle = nullptr;
    InteractionState interaction = InteractionState::Idle;
    double resizePressSceneX = 0.0;
    double resizeInitialStart = 0.0;
    double resizeInitialDuration = 0.0;
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
  setFlag(ItemIsMovable, false);
  setFlag(ItemSendsGeometryChanges);
  // Keep handle size constant in screen space even when timeline view is zoomed.
  setFlag(ItemIgnoresTransformations, true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setCursor(Qt::SizeHorCursor);
 }

W_OBJECT_IMPL(ClipItem)

QRectF ResizeHandle::boundingRect() const {
    return QRectF(-kResizeHandleHalfWidth, 0, kResizeHandleHalfWidth * 2.0,
        parentItem() ? parentItem()->boundingRect().height() : 20.0);
}

void ResizeHandle::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->save();
    const QRectF r = boundingRect();
    painter->setPen(QPen(QColor(18, 18, 18, 160), 1));
    painter->setBrush(QColor(226, 226, 226, 220));
    painter->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 2.0, 2.0);
    painter->setPen(QPen(QColor(70, 70, 70, 180), 1));
    const qreal cx = 0.0;
    const qreal cy = r.center().y();
    painter->drawLine(QPointF(cx, cy - 5.0), QPointF(cx, cy + 5.0));
    painter->restore();
}

// (factory helpers are provided as free functions: createClipItem / destroyClipItem)

QVariant ResizeHandle::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (g_clipGeometrySyncInProgress) {
        return QGraphicsObject::itemChange(change, value);
    }
    if (change == QGraphicsItem::ItemPositionChange && parentItem()) {
        Q_UNUSED(value);
        if (auto* clip = dynamic_cast<ClipItem*>(parentItem())) {
            const qreal anchoredX = (side == Left) ? 0.0 : clip->getDuration();
            return QPointF(anchoredX, 0.0);
        }
        return QPointF(0.0, 0.0);
    }
    return QGraphicsObject::itemChange(change, value);
}

void ResizeHandle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (auto* clip = dynamic_cast<ClipItem*>(parentItem())) {
        clip->beginHandleResize(side, event->scenePos().x());
    }
    grabMouse();
    event->accept();
}

void ResizeHandle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (auto* clip = dynamic_cast<ClipItem*>(parentItem())) {
        clip->handleMoved(side, event->scenePos().x());
    }
    event->accept();
}

void ResizeHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (scene() && scene()->mouseGrabberItem() == this) {
        ungrabMouse();
    }
    if (auto* clip = dynamic_cast<ClipItem*>(parentItem())) {
        clip->endHandleResize(side);
    }
    event->accept();
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
  delete impl_;
  impl_ = nullptr;
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
  painter->setRenderHint(QPainter::Antialiasing);
  
  QRectF rect = boundingRect();
  double cornerRadius = 4.0;
  
  // Base colors with Glassmorphism feel
  QColor baseColor = QColor(70, 120, 180, 200);
  if (isSelected()) {
   baseColor = QColor(100, 150, 220, 230);
  }
  
  // Gradient for depth
  QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
  grad.setColorAt(0, baseColor.lighter(120));
  grad.setColorAt(0.5, baseColor);
  grad.setColorAt(1, baseColor.darker(110));
  
  // Shadow/Glow effect on selection
  if (isSelected()) {
      painter->setPen(QPen(QColor(255, 255, 255, 100), 1));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(rect.adjusted(-1, -1, 1, 1), cornerRadius + 1, cornerRadius + 1);
  }

  // Draw main body
  painter->setPen(Qt::NoPen);
  painter->setBrush(grad);
  painter->drawRoundedRect(rect, cornerRadius, cornerRadius);
  
  // Inner Top Highlight (Glass effect)
  QPainterPath path;
  path.addRoundedRect(rect.adjusted(1, 1, -1, -rect.height()/2), cornerRadius-1, cornerRadius-1);
  painter->fillPath(path, QColor(255, 255, 255, 40));
  
  // Border
  QPen borderPen(isSelected() ? QColor(255, 215, 0) : QColor(255, 255, 255, 60));
  borderPen.setWidthF(isSelected() ? 1.5 : 0.8);
  painter->setPen(borderPen);
  painter->setBrush(Qt::NoBrush);
  painter->drawRoundedRect(rect, cornerRadius, cornerRadius);
  
  painter->restore();
 }

// Free-function factory/destructor helpers
ClipItem* createClipItem(double start, double duration, double height) {
    return new ClipItem(start, duration, height);
}

void destroyClipItem(ClipItem* clip) {
    delete clip;
}

void ClipItem::handleMoved(ResizeHandle::Side side, qreal sceneX)
{
    if (!impl_) {
        return;
    }

    double newStart = impl_->start;
    double newDuration = impl_->duration;

    const bool resizingLeft = impl_->interaction == Impl::InteractionState::ResizingLeft && side == ResizeHandle::Left;
    const bool resizingRight = impl_->interaction == Impl::InteractionState::ResizingRight && side == ResizeHandle::Right;

    if (resizingLeft || resizingRight) {
        const double delta = static_cast<double>(sceneX) - impl_->resizePressSceneX;
        if (resizingLeft) {
            const double oldEnd = impl_->resizeInitialStart + impl_->resizeInitialDuration;
            const double maxStart = oldEnd - impl_->minDuration;
            newStart = std::clamp(impl_->resizeInitialStart + delta, 0.0, maxStart);
            newDuration = oldEnd - newStart;
        } else {
            newStart = impl_->resizeInitialStart;
            newDuration = std::max(impl_->minDuration, impl_->resizeInitialDuration + delta);
        }
    } else {
        const double oldStart = impl_->start;
        const double oldEnd = impl_->start + impl_->duration;
        if (side == ResizeHandle::Left) {
            const double maxStart = oldEnd - impl_->minDuration;
            newStart = std::clamp(static_cast<double>(sceneX), 0.0, maxStart);
            newDuration = oldEnd - newStart;
        } else if (side == ResizeHandle::Right) {
            const double minEnd = oldStart + impl_->minDuration;
            const double newEnd = std::max(static_cast<double>(sceneX), minEnd);
            newDuration = newEnd - oldStart;
        }
    }

    setStartDuration(newStart, newDuration);
    update();
}

void ClipItem::setStartDuration(double start, double duration)
{
    if (!impl_) {
        return;
    }

    const double newStart = std::max(0.0, start);
    const double newDuration = std::max(impl_->minDuration, duration);
    const double oldDuration = impl_->duration;
    const double keepY = pos().y();

    g_clipGeometrySyncInProgress = true;
    if (std::abs(newDuration - oldDuration) > 1e-9) {
        prepareGeometryChange();
    }
    impl_->start = newStart;
    impl_->duration = newDuration;

    // Keep clip on the exact same track Y while resizing.
    setPos(impl_->start, keepY);

    if (impl_->leftHandle) {
        impl_->leftHandle->setPos(0.0, 0.0);
    }
    if (impl_->rightHandle) {
        impl_->rightHandle->setPos(impl_->duration, 0);
    }
    g_clipGeometrySyncInProgress = false;
}

void ClipItem::beginHandleResize(ResizeHandle::Side side, qreal sceneX)
{
    if (!impl_) {
        return;
    }
    impl_->resizePressSceneX = static_cast<double>(sceneX);
    impl_->resizeInitialStart = impl_->start;
    impl_->resizeInitialDuration = impl_->duration;
    impl_->interaction = (side == ResizeHandle::Left)
        ? Impl::InteractionState::ResizingLeft
        : Impl::InteractionState::ResizingRight;
}

void ClipItem::endHandleResize(ResizeHandle::Side side)
{
    if (!impl_) {
        return;
    }
    if ((side == ResizeHandle::Left && impl_->interaction == Impl::InteractionState::ResizingLeft) ||
        (side == ResizeHandle::Right && impl_->interaction == Impl::InteractionState::ResizingRight)) {
        impl_->interaction = Impl::InteractionState::Idle;
    }
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
        g_clipGeometrySyncInProgress = true;
        impl_->start = std::max(0.0, start);
        setPos(impl_->start, pos().y());
        g_clipGeometrySyncInProgress = false;
        update();
    }
}

void ClipItem::setDuration(double duration)
{
    if (impl_) {
        const double newDuration = std::max(impl_->minDuration, duration);
        const double oldDuration = impl_->duration;
        g_clipGeometrySyncInProgress = true;
        if (std::abs(newDuration - oldDuration) > 1e-9) {
            prepareGeometryChange();
        }
        impl_->duration = newDuration;
        if (impl_->leftHandle) {
            impl_->leftHandle->setPos(0.0, 0.0);
        }
        if (impl_->rightHandle) {
            impl_->rightHandle->setPos(impl_->duration, 0);
        }
        g_clipGeometrySyncInProgress = false;
        update();
    }
}

void ClipItem::setLayerId(const LayerID& layerId)
{
    if (impl_) {
        impl_->layerId_ = layerId;
    }
}

LayerID ClipItem::layerId() const
{
    return impl_ ? impl_->layerId_ : LayerID();
}

void ClipItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    if (!flags().testFlag(QGraphicsItem::ItemIsMovable)) {
        event->accept();
        return;
    }
    impl_->dragStartScenePos = event->scenePos();
    impl_->dragStartItemPos = pos();
    impl_->isDragging = false;
    // Selection state is managed by TimelineTrackView mouse handling to avoid
    // double-toggle between view-level and item-level press events.
    QGraphicsObject::mousePressEvent(event);
}

void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    if (impl_->interaction != Impl::InteractionState::Idle) {
        event->accept();
        return;
    }
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
    }
    event->accept();
}

void ClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!impl_) return;
    if (!flags().testFlag(QGraphicsItem::ItemIsMovable)) {
        event->accept();
        return;
    }
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
