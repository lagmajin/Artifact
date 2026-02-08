module;
#include <QToolBar>
export module Widgets.ToolBar;

export namespace Artifact {

 class ArtifactToolBar:public QToolBar{
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

  void homeRequested();
  void handToolRequested();
  
  // Zoom signals
  void zoomInRequested();
  void zoomOutRequested();
  void zoomFitRequested();
  void zoom100Requested();
  
  // Grid/Guide signals
  void gridToggled(bool visible);
  void guideToggled(bool visible);
  
  // View mode signals
  void viewModeChanged(const QString& mode);
 	
 };









};