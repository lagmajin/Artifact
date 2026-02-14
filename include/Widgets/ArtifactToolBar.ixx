module;
#include <QToolBar>
#include <wobjectdefs.h>
export module Widgets.ToolBar;

export namespace Artifact {

 class ArtifactToolBar:public QToolBar{
  W_OBJECT(ArtifactToolBar)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactToolBar(QWidget*parent =nullptr);
  ~ArtifactToolBar();
  void setActionEnabledAnimated(QAction* action, bool enabled);
  void setCompactMode(bool enabled);      // アイコンだけ
  void setTextUnderIcon(bool enabled);
  void lockHeight(bool locked = true);
  
  // Zoom controls
  void setZoomLevel(float zoomPercent);
  
  // Grid/Guide toggle
  void setGridVisible(bool visible);
  void setGuideVisible(bool visible);
  
 public:

  void homeRequested() W_SIGNAL(homeRequested);
  void handToolRequested() W_SIGNAL(handToolRequested);
  
  // Zoom signals
  void zoomInRequested() W_SIGNAL(zoomInRequested);
  void zoomOutRequested() W_SIGNAL(zoomOutRequested);
  void zoomFitRequested() W_SIGNAL(zoomFitRequested);
  void zoom100Requested() W_SIGNAL(zoom100Requested);
  
  // Grid/Guide signals
  void gridToggled(bool visible) W_SIGNAL(gridToggled, visible);
  void guideToggled(bool visible) W_SIGNAL(guideToggled, visible);
  
  // View mode signals
  void viewModeChanged(const QString& mode) W_SIGNAL(viewModeChanged, mode);
 	
 };






};