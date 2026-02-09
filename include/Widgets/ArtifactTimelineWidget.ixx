module;

#include <wobjectcpp.h>

#include <QtWidgets/QtWidgets>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>

#include <wobjectdefs.h>

export module Artifact.Widgets.Timeline;

import Artifact.Widgets.Hierarchy;
import Artifact.Widgets.LayerPanelWidget;
import std;


export namespace Artifact {

 

 

 class TimelineScene :public QGraphicsScene
 {
 private:
	 class Impl;
	 Impl* impl_;
 public:
  explicit TimelineScene(QWidget*parent=nullptr);
  void drawBackground(QPainter* painter, const QRectF& rect) override;


 };



 //#right
class TimelineTrackView :public QGraphicsView {
 W_OBJECT(TimelineTrackView)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void drawForeground(QPainter* painter, const QRectF& rect) override;
  void drawBackground(QPainter* painter, const QRectF& rect) override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 public:
  explicit TimelineTrackView(QWidget* parent = nullptr);
  ~TimelineTrackView();
  double position() const;
  void setPosition(double position);
  double duration() const;
  void setDuration(double duration);
  double zoomLevel() const;
  void setZoomLevel(double pixelsPerFrame);


  QSize minimumSizeHint() const override;

 public /*signals*/:
  void seekPositionChanged(double ratio) W_SIGNAL(seekPositionChanged,ratio);
 };


 class ArtifactTimelineWidget :public QWidget {
  W_OBJECT(ArtifactTimelineWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
 public:
  explicit ArtifactTimelineWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineWidget();
  void update();
  /*signals:*/
 public /*slots*/:
 };

 class ArtifactTimelineIconView :public QTreeView
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineIconView(QWidget* parent = nullptr);
  ~ArtifactTimelineIconView();
 };






};