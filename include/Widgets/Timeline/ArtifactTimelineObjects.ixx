module;
#include <utility>

#include "qnamespace.h"

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <wobjectdefs.h>
export module Artifact.Timeline.Objects;

import Utils.Id;

export namespace Artifact
{
 using namespace ArtifactCore;

class ResizeHandle : public QGraphicsItem{
 	//W_OBJECT(ResizeHandle)
 protected:
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
	
 class ClipItem : public QGraphicsItem {
 private:
  class Impl;
  Impl* impl_;
 public:
  using DragStartedCallback = std::function<void(ClipItem*)>;
  using DragMovedCallback = std::function<void(ClipItem*, double, double)>;
  using DragEndedCallback = std::function<void(ClipItem*, double, double)>;
  using GeometryEditedCallback = std::function<void(ClipItem*, double, double)>;

  ClipItem(double start, double duration, double height);
  ~ClipItem();
  friend ClipItem* createClipItem(double start, double duration, double height);
  friend void destroyClipItem(ClipItem* clip);
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
 protected:
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
  void setDragStartedCallback(DragStartedCallback callback);
  void setDragMovedCallback(DragMovedCallback callback);
  void setDragEndedCallback(DragEndedCallback callback);
  void setGeometryEditedCallback(GeometryEditedCallback callback);
 };

// Forward declarations of factory functions
ClipItem* createClipItem(double start, double duration, double height);
void destroyClipItem(ClipItem* clip);





};

