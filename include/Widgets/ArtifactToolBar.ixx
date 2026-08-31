module;
#include <utility>

#include <wobjectdefs.h>
#include <QString>
#include <QToolBar>

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
  Import,      // 取り込み重視
  Layout,      // レイアウト重視
  Animation,   // アニメーション重視
  VFX,         // エフェクト重視
  Compositing, // 合成重視
  Text,        // テキスト/キャプション重視
  Export,      // 書き出し重視
  Debug,       // 診断重視
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
  void refreshFromApplicationState();
  void refreshFromSettings();

public:
  void cameraToolRequested() W_SIGNAL(cameraToolRequested);

  // View mode signals
  void viewModeChanged(const QString &mode) W_SIGNAL(viewModeChanged, mode);

  // Workspace state notification
  void workspaceModeChanged(WorkspaceMode mode);

};

}; // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::ToolBarDisplayMode)
W_REGISTER_ARGTYPE(Artifact::WorkspaceMode)
