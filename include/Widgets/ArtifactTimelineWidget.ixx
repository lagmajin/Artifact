module;

#include <wobjectdefs.h>
//#include <QtWidgets/QtWidgets>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QWheelEvent>
#include <QKeyEvent>

#include <QWidget>
export module Artifact.Widgets.Timeline;

import std;
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
  void refreshCurveEditorTracks();
  void updateCurvePropertyList();
  void updateSearchState();
  void updateKeyframeState();
  void updateSelectionState();
  void toggleGraphEditorMode(bool active, Qt::FocusReason reason = Qt::OtherFocusReason);
  void advanceGraphEditorFocus(bool reverse);
  bool isGraphEditorFocusWidget(const QWidget* widget) const;
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
  void pasteKeyframesAtPlayhead();
  void showValueGraph();
  void showSpeedGraph();
  void showKeyPatternDialog();
  void applyKeyPattern(const ArtifactCore::KeyframePatternRequest& request);
  void jumpToSearchHit(int step);
  void jumpToKeyframeHit(int step);
  void jumpToFirstKeyframe();
  void jumpToLastKeyframe();
  bool handleTimelineAction(ArtifactTimelineAction action);

  /*signals:*/
 public:
  void zoomLevelChanged(double zoomPercent) W_SIGNAL(zoomLevelChanged, zoomPercent);
  void timelineDebugMessage(const QString& message) W_SIGNAL(timelineDebugMessage, message);
  
 public slots:
  void onSearchTextChanged(const QString& text);
 };

};
