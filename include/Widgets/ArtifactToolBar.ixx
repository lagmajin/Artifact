module;
#include <QString>
#include <QToolBar>
#include <wobjectdefs.h>

export module Widgets.ToolBar;

export namespace Artifact {

// 表示モード列挙型
enum class ToolBarDisplayMode {
  Full,      // アイコン＋テキストラベル
  IconsOnly, // アイコンのみ
  Compact    // 主要ツールのみ
};

// ワークスペースモード列挙型
enum class WorkspaceMode {
  Default,     // 標準
  Animation,   // アニメーション重視
  VFX,         // エフェクト重視
  Compositing, // 合成重視
  Audio        // オーディオ重視
};

class ArtifactToolOptionsBar; // 前方宣言

class ArtifactToolBar : public QToolBar {
  W_OBJECT(ArtifactToolBar)
private:
  class Impl;
  Impl *impl_;

public:
  explicit ArtifactToolBar(QWidget *parent = nullptr);
  ~ArtifactToolBar();
  void setActionEnabledAnimated(QAction *action, bool enabled);
  void setCompactMode(bool enabled); // アイコンだけ
  void setTextUnderIcon(bool enabled);
  void lockHeight(bool locked = true);

  // Display mode
  void setDisplayMode(ToolBarDisplayMode mode);
  ToolBarDisplayMode displayMode() const;

  // Zoom controls
  void setZoomLevel(float zoomPercent);
  float zoomLevel() const;

  // Grid/Guide toggle
  void setGridVisible(bool visible);
  void setGuideVisible(bool visible);

  // Workspace
  void setWorkspaceMode(WorkspaceMode mode);
  WorkspaceMode workspaceMode() const;

  // Tool options bar
  void setToolOptionsBar(ArtifactToolOptionsBar *bar);
  void setCurrentTool(const QString &toolName);

public:
  void homeRequested() W_SIGNAL(homeRequested);
  void selectToolRequested() W_SIGNAL(selectToolRequested);
  void handToolRequested() W_SIGNAL(handToolRequested);
  void zoomToolRequested() W_SIGNAL(zoomToolRequested);
  void moveToolRequested() W_SIGNAL(moveToolRequested);
  void rotationToolRequested() W_SIGNAL(rotationToolRequested);
  void scaleToolRequested() W_SIGNAL(scaleToolRequested);
  void cameraToolRequested() W_SIGNAL(cameraToolRequested);
  void panBehindToolRequested() W_SIGNAL(panBehindToolRequested);
  void shapeToolRequested() W_SIGNAL(shapeToolRequested);
  void penToolRequested() W_SIGNAL(penToolRequested);
  void textToolRequested() W_SIGNAL(textToolRequested);
  void brushToolRequested() W_SIGNAL(brushToolRequested);
  void cloneStampToolRequested() W_SIGNAL(cloneStampToolRequested);
  void eraserToolRequested() W_SIGNAL(eraserToolRequested);
  void puppetToolRequested() W_SIGNAL(puppetToolRequested);

  // Zoom signals
  void zoomInRequested() W_SIGNAL(zoomInRequested);
  void zoomOutRequested() W_SIGNAL(zoomOutRequested);
  void zoomFitRequested() W_SIGNAL(zoomFitRequested);
  void zoom100Requested() W_SIGNAL(zoom100Requested);

  // Grid/Guide signals
  void gridToggled(bool visible) W_SIGNAL(gridToggled, visible);
  void guideToggled(bool visible) W_SIGNAL(guideToggled, visible);

  // View mode signals
  void viewModeChanged(const QString &mode) W_SIGNAL(viewModeChanged, mode);

  // Display mode signal
  void displayModeChanged(ToolBarDisplayMode mode)
      W_SIGNAL(displayModeChanged, mode);

  // Workspace signal
  void workspaceModeChanged(WorkspaceMode mode)
      W_SIGNAL(workspaceModeChanged, mode);

  // Current tool signal
  void currentToolChanged(const QString &toolName)
      W_SIGNAL(currentToolChanged, toolName);
};

}; // namespace Artifact