module;
#include <utility>

#include <QWidget>
#include <QAction>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QString>
#include <QList>
#include <QIcon>
#include <functional>
#include <memory> // Added for std::unique_ptr if used in Impl
#include <wobjectcpp.h>

export module Artifact.MainWindow;

export import Dock.Manager;
import Dock.NativeDockSurface;
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
  void addDockedWidget(const QString &title, DockArea area,
                       QWidget *widget);
  void addDockedWidgetTabbed(const QString &title, DockArea area,
                             QWidget *widget, const QString &tabGroupPrefix);
  void addDockedWidgetTabbedWithId(const QString &title, const QString &dockId,
                                   DockArea area, QWidget *widget,
                                   const QString &tabGroupPrefix);
  void addLazyDockedWidgetTabbedWithId(const QString &title,
                                       const QString &dockId,
                                       DockArea area,
                                       std::function<QWidget *()> factory,
                                       const QString &tabGroupPrefix);
  void addLazyDockedWidgetFloating(const QString &title, const QString &dockId,
                                   std::function<QWidget *()> factory,
                                   const QRect &floatingGeometry);
  void addDockedWidgetFloating(const QString &title, const QString &dockId,
                               QWidget *widget, const QRect &floatingGeometry);
  void moveDockToTabGroup(const QString &title, const QString &tabGroupPrefix);
  void setDockVisible(const QString &title, bool visible);
  void setDockPinned(const QString &title, bool pinned);
  bool isDockPinned(const QString &title) const;
  void activateDock(const QString &title);
  bool closeDock(const QString &title);
  void setDockImmersive(QWidget *widget, bool immersive);
  void enterFocusMode();
  void exitFocusMode();
  void toggleFocusMode();
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
  QStringList dockIds() const;
  QString dockDisplayTitle(const QString &dockId) const;
  bool setDockTitle(const QString &dockIdOrTitle, const QString &title);
  QList<QAction *> dockTitleBarActions(const QString &dockIdOrTitle) const;
  bool setDockTitleBarActions(const QString &dockIdOrTitle,
                              const QList<QAction *> &actions);
  void setDockAreaDynamicTabsMenuButtonVisibility(bool enabled);
  bool dockAreaDynamicTabsMenuButtonVisibility() const;
  void setDockAreaAlwaysShowTabs(bool enabled);
  bool dockAreaAlwaysShowTabs() const;
  void setDockAreaDoubleClickUndocksWidget(bool enabled);
  bool dockAreaDoubleClickUndocksWidget() const;
  void setDockAreaHasTabsMenuButton(bool enabled);
  bool dockAreaHasTabsMenuButton() const;
  void setDockAreaHasCloseButton(bool enabled);
  bool dockAreaHasCloseButton() const;
  void setDockAreaCloseButtonClosesTab(bool enabled);
  bool dockAreaCloseButtonClosesTab() const;
  void setDockAreaHasUndockButton(bool enabled);
  bool dockAreaHasUndockButton() const;
  bool setDockAreaHideSingleWidgetTitleBar(const QString &dockIdOrTitle,
                                           bool enabled);
  bool dockAreaHideSingleWidgetTitleBar(
      const QString &dockIdOrTitle) const;
  void setDockAreaHideSingleCentralWidgetTitleBar(bool enabled);
  bool dockAreaHideSingleCentralWidgetTitleBar() const;
  void setDockEqualSplitOnInsertion(bool enabled);
  bool dockEqualSplitOnInsertion() const;
  void setDockOpaqueSplitterResize(bool enabled);
  bool dockOpaqueSplitterResize() const;
  void setDockAreaHideDisabledButtons(bool enabled);
  bool dockAreaHideDisabledButtons() const;
  void setFloatingContainerHasWidgetIcon(bool enabled);
  bool floatingContainerHasWidgetIcon() const;
  void setDockAreaActiveTabHasCloseButton(bool enabled);
  bool dockAreaActiveTabHasCloseButton() const;
  void setDockAreaAllTabsHaveCloseButton(bool enabled);
  bool dockAreaAllTabsHaveCloseButton() const;
  void setDockAreaRetainTabSizeWhenCloseButtonHidden(bool enabled);
  bool dockAreaRetainTabSizeWhenCloseButtonHidden() const;
  void setDockAreaDisableTabTextEliding(bool enabled);
  bool dockAreaDisableTabTextEliding() const;
  void setDockAreaShowTabTextOnlyForActiveTab(bool enabled);
  bool dockAreaShowTabTextOnlyForActiveTab() const;
  QIcon dockIcon(const QString &dockIdOrTitle) const;
  bool setDockIcon(const QString &dockIdOrTitle, const QIcon &icon);
  QString dockTabToolTip(const QString &dockIdOrTitle) const;
  bool setDockTabToolTip(const QString &dockIdOrTitle,
                         const QString &toolTip);
  QList<DockArea> dockAreaAllowedDropAreas(
      const QString &dockIdOrTitle) const;
  bool setDockAreaAllowedDropAreas(const QString &dockIdOrTitle,
                                   const QList<DockArea> &areas);
  bool setDockNoTab(const QString &dockIdOrTitle, bool noTab);
  bool dockClosable(const QString &dockIdOrTitle) const;
  bool setDockClosable(const QString &dockIdOrTitle, bool closable);
  bool dockMovable(const QString &dockIdOrTitle) const;
  bool setDockMovable(const QString &dockIdOrTitle, bool movable);
  bool dockFloatable(const QString &dockIdOrTitle) const;
  bool setDockFloatable(const QString &dockIdOrTitle, bool floatable);
  // QADS DockWidgetPinnable capability: whether Auto-Hide is allowed.
  bool dockAutoHideable(const QString &dockIdOrTitle) const;
  bool setDockAutoHideable(const QString &dockIdOrTitle, bool autoHideable);
  bool dockFocusable(const QString &dockIdOrTitle) const;
  bool setDockFocusable(const QString &dockIdOrTitle, bool focusable);
  // QADS AutoHideButtonTogglesArea equivalent for the active dock backend.
  void setAutoHideButtonTogglesArea(bool enabled);
  bool autoHideButtonTogglesArea() const;
  void setAutoHideButtonCheckable(bool enabled);
  bool autoHideButtonCheckable() const;
  void setAutoHideSideBarsIconOnly(bool enabled);
  bool autoHideSideBarsIconOnly() const;
  void setAutoHideShowOnMouseOver(bool enabled);
  bool autoHideShowOnMouseOver() const;
  QString resolveDockId(const QString &dockIdOrTitle) const;
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
  QByteArray savePortableDockLayoutState() const;
  bool restorePortableDockLayoutState(const QByteArray &state);
  bool restoreDockManagerState(const QByteArray &state);
  void captureDefaultDockManagerState();
  bool resetDockManagerStateToDefault();

  // AI Cloud widget access
  ArtifactAICloudWidget *aiCloudWidget() const;
};

}; // namespace Artifact
