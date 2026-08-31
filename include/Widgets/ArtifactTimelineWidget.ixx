module;

#include <wobjectdefs.h>
//#include <QtWidgets/QtWidgets>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPointF>
#include <QVector>
#include <QSet>

#include <QWidget>
export module Artifact.Widgets.Timeline;

import Utils.Id;
import Math.Interpolate;
import Animation.KeyframePatternGenerator;
import Artifact.Timeline.KeyBinding;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactTimelineWidget :public QWidget {
  W_OBJECT(ArtifactTimelineWidget)
 private:
 class Impl;
  Impl* impl_;
  void updateCacheVisuals();
  void syncPlayheadOverlay();
  void syncTimelineViewportFromNavigator();
  void syncTimelineHorizontalOffset(double offset);
  void syncTimelineVerticalOffset(double offset);
  void syncWorkAreaFromCurrentComposition();
  void syncPainterSelectionState(bool forceRefresh = false);
  void syncGpuTimelineSnapshot();
  void refreshCurveEditorTracks();
  void updateCurvePropertyList();
  void updateSearchState();
  void updateKeyframeState();
  void updateSelectionState();
  void toggleGraphEditorMode(bool active, Qt::FocusReason reason = Qt::OtherFocusReason);
  void advanceGraphEditorFocus(bool reverse);
  bool isGraphEditorFocusWidget(const QWidget* widget) const;
  void setCurrentFrameForAll(double frame);
 protected:
  void paintEvent(QPaintEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
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
  // Restrict curve/timeline property payloads to explicitly selected channels.
  // An empty set keeps the existing category/search behaviour.
  void setSelectedPropertyPaths(const QSet<QString>& propertyPaths);
  QSet<QString> selectedPropertyPaths() const;
  double currentFrame() const;
  void setGpuTimelinePreviewEnabled(bool enabled);
  bool gpuTimelinePreviewEnabled() const;
  bool gpuTimelinePreviewReady() const;

  // Layer management
  void onLayerCreated(const CompositionID& compId, const LayerID& layerId);
  void onLayerRemoved(const CompositionID& compId, const LayerID& layerId);
  void onShyChanged(bool active);
  void refreshTracks();
  void setLayerNameEditable(bool enabled);
  bool isLayerNameEditable() const;
  void addKeyframeAtPlayhead();
  void removeKeyframeAtPlayhead();
  void applyInterpolationToSelectedKeyframes(ArtifactCore::InterpolationType type);
  void selectAllKeyframes();
  void reverseSelectedKeyframes();
  void reverseAllKeyframesInCurrentLayer();
  void reverseAllKeyframesInSelectedLayers();
  void reverseAllKeyframesInComposition();
  void copySelectedKeyframes();
  bool saveKeyframeSnippet(const QString& name);
  bool applyKeyframeSnippet(const QString& name);
  bool removeKeyframeSnippet(const QString& name);
  void pasteKeyframesAtPlayhead();
  void copySelectedKeyframeEasing();
  void pasteKeyframeEasingToSelectedKeyframes();
  bool hasSelectedKeyframeArea() const;
  QString selectedKeyframeAreaSummary() const;
  bool applyValueToSelectedKeyframeArea(const LayerID& layerId,
                                        const QString& propertyPath,
                                        const QVariant& value);
  void showValueGraph();
  void showSpeedGraph();
  void showKeyPatternDialog();
  void applyAnimationPreset(const ArtifactCore::KeyframePatternPreset preset);
  void applyKeyPattern(const ArtifactCore::KeyframePatternRequest& request);
  bool applyTrajectoryToProperty(const LayerID& layerId,
                                 const QString& propertyPath,
                                 const QVector<QPointF>& trajectory,
                                 int startFrame,
                                 int numFrames);
  void jumpToSearchHit(int step);
  void jumpToKeyframeHit(int step);
  void jumpToFirstKeyframe();
  void jumpToLastKeyframe();
  bool handleTimelineAction(ArtifactTimelineAction action);

  // Internal timeline-to-parent notifications. Cross-widget consumers use
  // TimelineZoomLevelChangedEvent / TimelineDebugMessageEvent instead.
 public:
  void zoomLevelChanged(double zoomPercent);
  void timelineDebugMessage(const QString& message);
  
 public slots:
  void onSearchTextChanged(const QString& text);
 };

};
