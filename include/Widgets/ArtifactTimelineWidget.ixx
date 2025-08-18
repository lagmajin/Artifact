module;

#include <wobjectcpp.h>

#include <QtWidgets/QtWidgets>

#include <QGraphicsView>

#include <wobjectdefs.h>

export module Artifact.Widgets.Timeline;

import Artifact.Widgets.Hierarchy;

export namespace Artifact {

 class ArtifactTimeCodeWidget :public QWidget {
  W_OBJECT(ArtifactTimeCodeWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimeCodeWidget(QWidget* parent = nullptr);
  ~ArtifactTimeCodeWidget();

  void updateTimeCode(int frame);
 };

 class TimelineScene :public QGraphicsScene
 {
 private:

 public:
  void drawBackground(QPainter* painter, const QRectF& rect) override;


 };

 //#right
 class TimelineTrackView :public QGraphicsView {
  W_OBJECT(TimelineTrackView)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit TimelineTrackView(QWidget* parent = nullptr);
  ~TimelineTrackView();
  double zoomLevel() const;
  void setZoomLevel(double pixelsPerFrame);

 protected:
  void drawBackground(QPainter* painter, const QRectF& rect) override;


  void drawForeground(QPainter* painter, const QRectF& rect) override;

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
 signals:
 public slots:
 };

 class ArtifactTimelineIconView:public QTreeView
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineIconView(QWidget* parent = nullptr);
  ~ArtifactTimelineIconView();
 };

 class TimelineLabel:public QWidget
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit TimelineLabel(QWidget* parent = nullptr);
  ~TimelineLabel();
 };





};