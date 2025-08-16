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

 //#right
 class TimelineTrackView :public QGraphicsView {
  W_OBJECT(TimelineTrackView)
 private:

 public:
  explicit TimelineTrackView(QWidget* parent = nullptr);
  ~TimelineTrackView();
  double zoomLevel() const;
  void setZoomLevel(double pixelsPerFrame);

 };


 class ArtifactTimelineWidget :public QWidget{
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
  explicit ArtifactTimelineWidget(QWidget *parent=nullptr);
  ~ArtifactTimelineWidget();
  void update();
 signals:
 public slots:
 };









};