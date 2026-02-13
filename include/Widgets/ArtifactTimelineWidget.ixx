module;

#include <wobjectcpp.h>

#include <QtWidgets/QtWidgets>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsScene>
#include <vector>

#include <wobjectdefs.h>

export module Artifact.Widgets.Timeline;
import std;
import Utils.Id;
import Artifact.Widgets.Hierarchy;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Timeline.Objects;



export namespace Artifact {

 class TimelineScene : public QGraphicsScene
 {
	 W_OBJECT(TimelineScene)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit TimelineScene(QWidget* parent = nullptr);
  ~TimelineScene();
  
  void drawBackground(QPainter* painter, const QRectF& rect) override;
  
  // Track management
  int addTrack(double height = 20.0);
  void removeTrack(int trackIndex);
  int trackCount() const;
  double trackHeight(int trackIndex) const;
  void setTrackHeight(int trackIndex, double height);
  double getTrackYPosition(int trackIndex) const;
  
  // Clip management
  ClipItem* addClip(int trackIndex, double start, double duration);
  void removeClip(ClipItem* clip);
  const std::vector<ClipItem*>& getClips() const;
  int getTrackAtPosition(double yPos) const;
  
  // Selection
  void clearSelection();
  const std::vector<ClipItem*>& getSelectedClips() const;
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
  void resizeEvent(QResizeEvent* event) override;

 public:
  explicit TimelineTrackView(QWidget* parent = nullptr);
  ~TimelineTrackView();
  double position() const;
  void setPosition(double position);
  double duration() const;
  void setDuration(double duration);
  double zoomLevel() const;
  void setZoomLevel(double pixelsPerFrame);

  // Track and clip management
  TimelineScene* timelineScene() const;
  int addTrack(double height = 20.0);
  void removeTrack(int trackIndex);
  ClipItem* addClip(int trackIndex, double start, double duration);
  void removeClip(ClipItem* clip);
  void clearSelection();

  QSize minimumSizeHint() const override;

 public /*signals*/:
  void seekPositionChanged(double ratio) W_SIGNAL(seekPositionChanged,ratio);
  void clipSelected(ClipItem* clip) W_SIGNAL(clipSelected,clip);
  void clipDeselected(ClipItem* clip) W_SIGNAL(clipDeselected,clip);
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
  void setComposition(const CompositionID& id);
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