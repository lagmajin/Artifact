module;
#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QSet>
#include <QVector>
#include <QWidget>
#include <QString>
#include <wobjectdefs.h>

export module Artifact.Timeline.TrackPainterView;

import Utils.Id;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactTimelineTrackPainterView : public QWidget
 {
  W_OBJECT(ArtifactTimelineTrackPainterView)

 public:
  struct KeyframeMarkerVisual {
   LayerID layerId;
   int trackIndex = -1;
   double frame = 0.0;
   QColor color = QColor(247, 204, 83);
   QString label;
  };

  struct TrackClipVisual {
   QString clipId;
   LayerID layerId;
   int trackIndex = -1;
   double startFrame = 0.0;
   double durationFrame = 1.0;
   QString title;
   QColor fillColor = QColor(73, 126, 196);
   bool selected = false;
  };

 private:
  class Impl;
  Impl* impl_ = nullptr;

 protected:
 void paintEvent(QPaintEvent* event) override;
 void mousePressEvent(QMouseEvent* event) override;
 void mouseMoveEvent(QMouseEvent* event) override;
 void mouseReleaseEvent(QMouseEvent* event) override;
 void wheelEvent(QWheelEvent* event) override;
 void leaveEvent(QEvent* event) override;
 QSize minimumSizeHint() const override;

 public:
  explicit ArtifactTimelineTrackPainterView(QWidget* parent = nullptr);
  ~ArtifactTimelineTrackPainterView();

  void setDurationFrames(double frames);
  double durationFrames() const;

  void setCurrentFrame(double frame);
  double currentFrame() const;

  void setPixelsPerFrame(double value);
  double pixelsPerFrame() const;

  void setHorizontalOffset(double value);
  double horizontalOffset() const;

  void setVerticalOffset(double value);
  double verticalOffset() const;

  void setTrackCount(int count);
  int trackCount() const;

  void setTrackHeight(int trackIndex, int height);
  int trackHeight(int trackIndex) const;

  void clearClips();
  void setClips(const QVector<TrackClipVisual>& clips);
  void setKeyframeMarkers(const QVector<KeyframeMarkerVisual>& markers);
  void setSelectedLayerIds(const QSet<LayerID>& layerIds);
  QVector<TrackClipVisual> clips() const;

 public /*signals*/:
  void seekRequested(double frame) W_SIGNAL(seekRequested, frame);
  void clipSelected(const QString& clipId, const LayerID& layerId) W_SIGNAL(clipSelected, clipId, layerId);
  void clipDeselected() W_SIGNAL(clipDeselected);
  void clipMoved(const QString& clipId, double startFrame) W_SIGNAL(clipMoved, clipId, startFrame);
  void clipResized(const QString& clipId, double startFrame, double durationFrame) W_SIGNAL(clipResized, clipId, startFrame, durationFrame);
  void timelineDebugMessage(const QString& message) W_SIGNAL(timelineDebugMessage, message);
 };
}
