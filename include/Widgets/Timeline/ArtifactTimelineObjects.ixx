module ;
#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <wobjectdefs.h>

#include "qnamespace.h"

export module Artifact.Timeline.Objects;

import Utils.Id;

export namespace Artifact
{
 using namespace ArtifactCore;

 class ResizeHandle : public QGraphicsObject{
 	//W_OBJECT(ResizeHandle)
 protected:
 QVariant itemChange(GraphicsItemChange change,
  const QVariant& value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
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
  ClipItem(double start, double duration, double height);
  ~ClipItem();
  // Friend factory functions (declare before Verdigris macro to avoid access
  // specifier issues the macro may inject). These free functions are defined
  // in the implementation file and are visible to other translation units.
  friend ClipItem* createClipItem(double start, double duration, double height);
  friend void destroyClipItem(ClipItem* clip);
  W_OBJECT(ClipItem)
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
  void beginHandleResize(ResizeHandle::Side side, qreal sceneX);
  void endHandleResize(ResizeHandle::Side side);
  
  // Set size/position (start = x in scene coords, duration = width in pixels)
  void setStartDuration(double start, double duration);
  
  // Getter for internal data
  double getStart() const;
  double getDuration() const;
  void setStart(double start);
  void setDuration(double duration);
  void setLayerId(const LayerID& layerId);
  LayerID layerId() const;

  // Drag lifecycle signals (foundation): emit during drag so external code can react
  void dragStarted(ClipItem* clip) W_SIGNAL(dragStarted,clip);
  void dragMoved(ClipItem* clip, double sceneX, double sceneY) W_SIGNAL(dragMoved,clip,sceneX,sceneY);
  void dragEnded(ClipItem* clip, double sceneX, double sceneY) W_SIGNAL(dragEnded,clip,sceneX,sceneY);
  void geometryEdited(ClipItem* clip, double start, double duration) W_SIGNAL(geometryEdited, clip, start, duration);
 };

// Forward declarations of factory functions
ClipItem* createClipItem(double start, double duration, double height);
void destroyClipItem(ClipItem* clip);





};
