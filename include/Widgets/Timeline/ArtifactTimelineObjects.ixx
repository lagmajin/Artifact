module ;
#include <QColor>
#include <QGraphicsItem>
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
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
 protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

 };





};