module;

#include <QtWidgets/QtWidgets>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QGraphicsScene>
#include <vector>

#include <wobjectdefs.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Widgets.Timeline;

import Utils.Id;
import Artifact.Widgets.Hierarchy;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Timeline.Objects;

W_REGISTER_ARGTYPE(Artifact::ClipItem*)
W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact {
using namespace ArtifactCore;

 class TimelineScene : public QGraphicsScene
 {
	 W_OBJECT(TimelineScene)
 private:
  class Impl;
  Impl* impl_;
 public:
  enum class SnapStrength
  {
   Low = 0,
   Medium,
   High
  };

  explicit TimelineScene(QWidget* parent = nullptr);
  ~TimelineScene();
  
  void drawBackground(QPainter* painter, const QRectF& rect) override;
  
  // Track management
  int addTrack(double height = 20.0);
  void clearTracks();
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

  // Snap strength control
  void setSnapStrength(SnapStrength strength);
  [[nodiscard]] SnapStrength snapStrength() const;
  void setRippleEditEnabled(bool enabled);
  [[nodiscard]] bool rippleEditEnabled() const;
  
  // Visual helpers
  void highlightTrack(int trackIndex);
  void clearTrackHighlight();
 };

class ArtifactTimelineWidget :public QWidget {
  W_OBJECT(ArtifactTimelineWidget)
 private:
  class Impl;
  Impl* impl_;
  void syncWorkAreaFromCurrentComposition();
  void syncTimelineViewportFromNavigator(bool emitZoomSignal = true);
  void resetTimelineViewport();
  void updateSearchState();
  void updateKeyframeState();
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

  // Layer management
  void onLayerCreated(const CompositionID& compId, const LayerID& layerId);
  void onLayerRemoved(const CompositionID& compId, const LayerID& layerId);
  void onShyChanged(bool active);
  void refreshTracks();
  void setLayerNameEditable(bool enabled);
  bool isLayerNameEditable() const;
  void jumpToSearchHit(int step);
  void jumpToKeyframeHit(int step);

  /*signals:*/
 public:
  void zoomLevelChanged(double zoomPercent) W_SIGNAL(zoomLevelChanged, zoomPercent);
  void timelineDebugMessage(const QString& message) W_SIGNAL(timelineDebugMessage, message);
  
 public slots:
  void onSearchTextChanged(const QString& text);
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
