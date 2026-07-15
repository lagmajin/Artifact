module;
#include <utility>

#include <QWidget>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QString>
#include <ads_globals.h>
#include <functional>
#include <memory> // Added for std::unique_ptr if used in Impl
#include <wobjectcpp.h>

export module Artifact.MainWindow;

import Artifact.Widgets.AudioMixer;               // Added
import Audio.Mixer;                               // Added
import Artifact.Widgets.AI.ArtifactAICloudWidget; // Added
import Widgets.ToolBar;

export namespace Artifact {

// struct ArtifactMainWindowPrivate;

class ArtifactMainWindow : public QWidget {
  // ReSharper disable CppInspection
  W_OBJECT(ArtifactMainWindow)
  // ReSharper restore CppInspection
private:
  class Impl;
  Impl *impl_;

#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
  // Command Palette (experimental, feature-flagged). Lazily created in the
  // constructor; toggled by Ctrl+Shift+P via keyPressEvent. No service
  // wiring is added; the palette reads the existing menu hierarchy.
  static QWidget *sPalette_instance();
  static void setPalette_instance(QWidget *palette);
  static QWidget *paletteInstance_;
#endif

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void showEvent(QShowEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

public:
  explicit ArtifactMainWindow(QWidget *parent = nullptr);
  ~ArtifactMainWindow();

public /*slots*/:
  void addWidget();
  void setCentralWorkspace(const QString &title, QWidget *widget);
  void addDockedWidget(const QString &title, ads::DockWidgetArea area,
                       QWidget *widget);
  void addDockedWidgetTabbed(const QString &title, ads::DockWidgetArea area,
                             QWidget *widget, const QString &tabGroupPrefix);
  void addDockedWidgetTabbedWithId(const QString &title, const QString &dockId,
                                   ads::DockWidgetArea area, QWidget *widget,
                                   const QString &tabGroupPrefix);
  void addLazyDockedWidgetTabbedWithId(const QString &title,
                                       const QString &dockId,
                                       ads::DockWidgetArea area,
                                       std::function<QWidget *()> factory,
                                       const QString &tabGroupPrefix);
  void addLazyDockedWidgetFloating(const QString &title, const QString &dockId,
                                   std::function<QWidget *()> factory,
                                   const QRect &floatingGeometry);
  void addDockedWidgetFloating(const QString &title, const QString &dockId,
                               QWidget *widget, const QRect &floatingGeometry);
  void moveDockToTabGroup(const QString &title, const QString &tabGroupPrefix);
  void setDockVisible(const QString &title, bool visible);
  void activateDock(const QString &title);
  bool closeDock(const QString &title);
  void setDockImmersive(QWidget *widget, bool immersive);
  void setStartupLayoutFrozen(bool frozen);

  void closeAllDocks();
  void showStatusMessage(const QString &message, int timeoutMs = 2000);
  void togglePanelsVisible(bool visible);
  void setWorkspaceMode(WorkspaceMode mode);
  WorkspaceMode workspaceMode() const;
  void applyUiFontSettings();
  void applyApplicationSettings();

  // Dock enumeration
  QStringList dockTitles() const;
  bool isDockVisible(const QString &title) const;
  bool hasDock(const QString &title) const;

  // Status bar update methods
  void setStatusZoomLevel(float zoomPercent);
  void setStatusCoordinates(int x, int y);
  void setStatusMemoryUsage(uint64_t memoryMB);
  void setStatusFPS(double fps);
  void setStatusPreviewResolution(int percent);
  void setStatusReady();
  void setStatusBar(QStatusBar *statusBar);
  void setDockSplitterSizes(const QString &dockTitle, const QList<int> &sizes);

  // ADS dock manager のレイアウト状態（dock 配置、タブグループ、splitter、floating 位置）の
  // 保存・復元。トップレベル QWidget の geometry と分離して扱う。
  // 保存はアプリ終了時、復元は起動時のレイアウト構築後に呼ぶ。
  // restore は「全ての dock が登録された後」でなければならない（ADS の制約）。
  QByteArray saveDockManagerState() const;
  bool restoreDockManagerState(const QByteArray &state);

  // AI Cloud widget access
  ArtifactAICloudWidget *aiCloudWidget() const;
};

}; // namespace Artifact
