module;
#include <utility>

#include <QColor>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QDropEvent>
#include <QMenu>
#include <QWheelEvent>
#include <QSet>
#include <QVector>
#include <QVariant>
#include <QWidget>
#include <QString>
#include <wobjectdefs.h>
export module Artifact.Timeline.TrackPainterView;

import Utils.Id;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Composition.Abstract;
import Artifact.Layers.Selection.Manager;
import Geometry.Interpolate;
import Property.Abstract;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactTimelineTrackPainterView : public QWidget
 {
  W_OBJECT(ArtifactTimelineTrackPainterView)

 public:
  enum class PropertyChannelFilter { All, Transform, Audio, Effect };
 struct KeyframeMarkerVisual {
   LayerID layerId;
   QString propertyPath;
   int trackIndex = -1;
   double frame = 0.0;
   QVariant value;
   int laneIndex = 0;
   int laneCount = 1;
   bool selectedLayer = false;
   bool selected = false;
   bool eased = false;
   bool incomingEased = false;
   bool outgoingEased = false;
   bool incomingBezier = false;
   bool outgoingBezier = false;
    bool bezier = false;
    bool brokenTangents = false;
    bool roving = false;
    double inHandleFrameOffset = 0.0;
    double inHandleValueOffset = 0.0;
    double outHandleFrameOffset = 0.0;
    double outHandleValueOffset = 0.0;
    ArtifactCore::InterpolationType interpolation =
       ArtifactCore::InterpolationType::Linear;
   ArtifactCore::KeyFrame::Anchor anchor =
       ArtifactCore::KeyFrame::Anchor::Absolute;
   QColor color = QColor(247, 204, 83);
   QColor labelColor;
   QString label;
  };

  struct CompositionMarkerVisual {
   double frame = 0.0;
   QString comment;
   QColor color = QColor(247, 204, 83);
   bool chapter = false;
  };

  struct TrackClipVisual {
   enum class Kind {
    Generic,
    Audio,
    Video
   };
   enum class SourceState { Ready, Missing, Proxy, Unreadable, SequenceGap };

   QString clipId;
   LayerID layerId;
   int trackIndex = -1;
   double startFrame = 0.0;
   double durationFrame = 1.0;
   double trimMinStartFrame = 0.0;
   double trimMaxEndFrame = 0.0;
   bool hasTrimSourceRange = false;
   QString title;
   QColor fillColor = QColor(73, 126, 196);
   Kind kind = Kind::Generic;
   SourceState sourceState = SourceState::Ready;
   double audioFadeInFrames = 0.0;
   double audioFadeOutFrames = 0.0;
   double audioClipGainDb = 0.0;
   double audioPan = 0.0;
   double audioPlaybackRate = 1.0;
   bool audioReversed = false;
   bool audioMuted = false;
   bool selected = false;
   QVector<float> waveformPeaks;
   QVector<float> waveformRms;
  };

 private:
  class Impl;
  Impl* impl_ = nullptr;
  void drawPlayhead(QPainter& painter) const;

 protected:
 void paintEvent(QPaintEvent* event) override;
 void mousePressEvent(QMouseEvent* event) override;
 void mouseMoveEvent(QMouseEvent* event) override;
 void mouseReleaseEvent(QMouseEvent* event) override;
 void keyPressEvent(QKeyEvent* event) override;
 void contextMenuEvent(QContextMenuEvent* event) override;
 void wheelEvent(QWheelEvent* event) override;
 void leaveEvent(QEvent* event) override;
 void dragEnterEvent(QDragEnterEvent* event) override;
 void dragMoveEvent(QDragMoveEvent* event) override;
 void dragLeaveEvent(QDragLeaveEvent* event) override;
 void dropEvent(QDropEvent* event) override;
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
  void setKeyframeContext(const LayerID& layerId, const QString& propertyPath);
  void setPropertyChannelFilter(PropertyChannelFilter filter);
  PropertyChannelFilter propertyChannelFilter() const;
  void setSelectedPropertyPaths(const QSet<QString>& propertyPaths);
  QSet<QString> selectedPropertyPaths() const;

  void setTrackCount(int count);
  int trackCount() const;

  void setTrackHeights(const QVector<int>& heights);
  void setTrackHeight(int trackIndex, int height);
  int trackHeight(int trackIndex) const;

  void clearClips();
  void setClips(const QVector<TrackClipVisual>& clips);
  void setKeyframeMarkers(const QVector<KeyframeMarkerVisual>& markers);
  void setCompositionMarkers(const QVector<CompositionMarkerVisual>& markers);
  QVector<KeyframeMarkerVisual> keyframeMarkers() const;
  QVector<KeyframeMarkerVisual> selectedKeyframeMarkers() const;
  KeyframeMarkerVisual hoveredKeyframeMarker() const;
  void selectAllKeyframeMarkers();
  bool reverseSelectedKeyframeMarkers();
  bool reverseAllKeyframesInCurrentLayer();
  bool reverseAllKeyframesInSelectedLayers();
  bool reverseAllKeyframesInComposition();
  void selectSamePropertyKeyframeMarkers();
  void selectNeighborKeyframeMarkers();
  void clearKeyframeSelection();
  void setSelectedKeyframeKeys(const QSet<QString>& selectedKeys);
  bool setSelectedKeyframeAnchor(ArtifactCore::KeyFrame::Anchor anchor);
  bool setSelectedKeyframeColorLabel(ArtifactCore::KeyFrame::ColorLabel label);
  bool hasNumericSelectedKeyframes() const;
  bool promptSetSelectedKeyframeValue();
  bool deleteSelectedKeyframeMarkers();
  bool duplicateSelectedKeyframeMarkersAtCurrentFrame();
  bool distributeSelectedKeyframeMarkersEvenly();
  bool repeatSelectedKeyframeMarkersAtCurrentFrame();
  bool hasSelectedKeyframes() const;
  void syncSelectionState(const ArtifactCompositionPtr& composition,
                          ArtifactLayerSelectionManager* selectionManager,
                          const QVector<TimelineRowDescriptor>& trackRows,
                          bool forceRefresh = false);
  QVector<TrackClipVisual> clips() const;

 public:
  void clipSelected(const QString& clipId, const LayerID& layerId);
  void clipMoved(const QString& clipId, double startFrame);
  void clipSlid(const QString& clipId, double startFrame);
  void clipResized(const QString& clipId, double startFrame, double durationFrame);
  void keyframeMoveRequested(const LayerID& layerId, const QString& propertyPath, qint64 fromFrame, qint64 toFrame);
  void keyframeSelectionChanged(int selectedCount);
  void timelineDebugMessage(const QString& message);
 };
}
