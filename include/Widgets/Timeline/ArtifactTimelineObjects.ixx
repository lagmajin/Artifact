module ;
#include <QColor>
#include <QGraphicsItem>
#include "qnamespace.h"

export module Artifact.Timeline.Objects;

export namespace Artifact
{
 class ResizeHandle : public QGraphicsRectItem {
 protected:
  QVariant itemChange(GraphicsItemChange change,
   const QVariant& value) override;
 public:
  enum Side { Left, Right };
  Side side;

  ResizeHandle(Side s, QGraphicsItem* parent);


 };
	
 class ClipItem : public QGraphicsRectItem {
 private:
  class Impl;
  Impl* impl_;
 public:
  ClipItem(double start, double duration, double height);
  ~ClipItem();
 };





};