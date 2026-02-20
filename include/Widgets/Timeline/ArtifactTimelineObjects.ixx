module ;
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <wobjectdefs.h>

#include "qnamespace.h"

export module Artifact.Timeline.Objects;

export namespace Artifact
{
 class ResizeHandle : public QGraphicsObject{
 	//W_OBJECT(ResizeHandle)
 protected:
  QVariant itemChange(GraphicsItemChange change,
   const QVariant& value) override;
  public:
   QRectF boundingRect() const override;
   void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
 public:
  enum Side { Left, Right };
  Side side;

  ResizeHandle(Side s, QGraphicsItem* parent);


 };
	
 class ClipItem : public QGraphicsObject {
 private:
  class Impl;
  Impl* impl_;
 public:
  W_OBJECT(ClipItem)
  ClipItem(double start, double duration, double height);
  ~ClipItem();
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
 protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  // Mouse event overrides for drag handling
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

 public:
  // Called by child resize handles when moved
  void handleMoved(ResizeHandle::Side side, qreal sceneX);
  
  // Set size/position (start = x in scene coords, duration = width in pixels)
  void setStartDuration(double start, double duration);
  
  // Getter for internal data
  double getStart() const;
  double getDuration() const;
  void setStart(double start);
  void setDuration(double duration);

  // Drag lifecycle signals (foundation): emit during drag so external code can react
  void dragStarted(ClipItem* clip) W_SIGNAL(dragStarted,clip);
  void dragMoved(ClipItem* clip, double sceneX, double sceneY) W_SIGNAL(dragMoved,clip,sceneX,sceneY);
  void dragEnded(ClipItem* clip, double sceneX, double sceneY) W_SIGNAL(dragEnded,clip,sceneX,sceneY);
 };





};