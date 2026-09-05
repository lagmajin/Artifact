module;
#include <algorithm>
#include <utility>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef MessageBox
#endif
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockOverlay.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>
#include <FloatingDockContainer.h>
#endif
#include <QAbstractScrollArea>
#include <QAbstractButton>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QList>
#include <cmath>
#include <QMetaObject>
#include <QMessageBox>
#include <QMenu>
#include <QObject>
#include <QPushButton>
#include <QPointer>
#include <QPixmap>
#include <QSet>
#include <QSettings>
#include <QShowEvent>
#include <QStatusBar>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <Diagnostics/WidgetCreationDiagnostics.hpp>
#include <wobjectimpl.h>

module Artifact.MainWindow;

import Widgets.Common.DialogPlacement;
import Artifact.Application.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Tool.Brush;
import Artifact.Tool.Manager;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Event.Bus;
import Text.Style;
import Utils.String.UniString;
import Widgets.ToolOptionsBar;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.Welcome;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Composition.InitParams;
import Artifact.Widgets.ImportAssetsDialog;
import Menu.MenuBar;
import Artifact.Menu.View;
import Widgets.ToolBar;
import Widgets.Dock.StyleManager;
import Widgets.Utils.CSS;
import Utils.Path;
import Artifact.Widgets.AppDialogs;
import Artifact.Widgets.StartupScreenshot;
import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Artifact.Workspace.Modes;
import Application.AppSettings;
import Memory.SharedPtr;
import Settings.Accessibility;
import Undo.UndoManager;
#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
import Command.Palette;
#endif

namespace Artifact {

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
class ArtifactQadsDockAdapter;
void syncTrackedDockState(ArtifactQadsDockAdapter *backend,
                          ads::CDockWidget *dock);

using namespace ads;

static DockWidgetArea toAdsDockArea(DockArea area) {
  switch (area) {
  case DockArea::Left: return LeftDockWidgetArea;
  case DockArea::Right: return RightDockWidgetArea;
  case DockArea::Top: return TopDockWidgetArea;
  case DockArea::Bottom: return BottomDockWidgetArea;
  case DockArea::Center: return CenterDockWidgetArea;
  }
  return CenterDockWidgetArea;
}
#endif

#if defined(_WIN32)
using DwmSetWindowAttributeFn = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);

void applyDarkNativeTitleBar(QWidget *widget) {
  if (!widget) {
    return;
  }

  const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
  if (!hwnd) {
    return;
  }

  static HMODULE dwmModule = ::LoadLibraryW(L"dwmapi.dll");
  if (!dwmModule) {
    return;
  }

  static const auto setWindowAttribute =
      reinterpret_cast<DwmSetWindowAttributeFn>(
          ::GetProcAddress(dwmModule, "DwmSetWindowAttribute"));
  if (!setWindowAttribute) {
    return;
  }

  const BOOL darkModeEnabled = TRUE;
  const DWORD darkModeAttributes[] = {20u, 19u};
  for (const DWORD attribute : darkModeAttributes) {
    setWindowAttribute(hwnd, attribute, &darkModeEnabled,
                       sizeof(darkModeEnabled));
  }

  const COLORREF captionColor = RGB(40, 40, 40);
  const COLORREF textColor = RGB(187, 187, 187);
  const COLORREF borderColor = RGB(24, 24, 24);
  setWindowAttribute(hwnd, 35u, &captionColor, sizeof(captionColor));
  setWindowAttribute(hwnd, 36u, &textColor, sizeof(textColor));
  setWindowAttribute(hwnd, 34u, &borderColor, sizeof(borderColor));

  // Force frame recalculation
  ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
}
#else
void applyDarkNativeTitleBar(QWidget *) {}
#endif

namespace {
constexpr auto kStartupScreenshotDelayKey =
    "UI/Diagnostics/StartupScreenshotDelayMs";

bool isTimelineDockTitle(const QString &title) {
  return title.startsWith(QStringLiteral("Timeline"), Qt::CaseInsensitive);
}

struct WorkspaceVisibilityRule {
  WorkspaceMode mode;
  QStringList visibleTitles;
  QStringList hiddenTitles;
};

const WorkspaceVisibilityRule *workspaceVisibilityRuleFor(WorkspaceMode mode) {
  static const WorkspaceVisibilityRule rules[] = {
      {WorkspaceMode::Default,
       {"Composition Viewer", "Project", "Inspector"},


       {"Asset Browser", "Effects", "Properties", "Audio Mixer",
        "Contents Viewer", "AI Chat", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"}},
      {WorkspaceMode::Import,
       {"Project", "Asset Browser", "Inspector", "Effects", "Properties"},
       {"Audio Mixer", "Contents Viewer", "AI Chat", "Composition Viewer"}},
      {WorkspaceMode::Layout,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Effects", "Properties"},
       {"Audio Mixer", "Contents Viewer", "AI Chat"}},
      {WorkspaceMode::Animation,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Effects", "Properties", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control"}},
      {WorkspaceMode::VFX,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Effects", "Properties", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"},
       {"Audio Mixer", "Contents Viewer", "AI Chat", "Playback Control"}},
      {WorkspaceMode::Compositing,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Effects", "Properties", "Layer Solo View"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control", "Composition View (Software)",
        "Layer View (Software)"}},
      {WorkspaceMode::Text,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Effects", "Properties", "Contents Viewer"},
       {"Audio Mixer", "AI Cloud", "AI Chat", "Playback Control"}},
      {WorkspaceMode::Export,
       {"Project", "Asset Browser", "Inspector", "Effects", "Properties",
        "Composition Viewer"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control"}},
      {WorkspaceMode::Debug,
       {"Project", "Asset Browser", "Inspector", "Effects", "Properties",
        "Contents Viewer", "AI Chat", "Playback Control"},
       {"Audio Mixer"}},
      {WorkspaceMode::Audio,
       {"Contents Viewer", "Audio Mixer", "Project", "Asset Browser",
        "Inspector", "Effects", "Properties"},
       {"AI Cloud", "AI Chat", "Composition Viewer",
        "Composition View (Software)", "Layer Solo View",
        "Layer View (Software)"}},
  };
  for (const auto &rule : rules) {
    if (rule.mode == mode) {
      return &rule;
    }
  }
  return &rules[0];
}

QWidget *createLazyDockPlaceholder(QWidget *parent,
                                   const QString &panelTitle = {}) {
  auto *placeholder = new QWidget(parent);
  placeholder->setObjectName(QStringLiteral("ArtifactLazyDockPlaceholder"));
  placeholder->setAutoFillBackground(true);
  QPalette palette = placeholder->palette();
  palette.setColor(QPalette::Window, QColor(32, 34, 38));
  placeholder->setPalette(palette);
  auto *layout = new QVBoxLayout(placeholder);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(6);
  auto *title = new QLabel(
      panelTitle.trimmed().isEmpty() ? QStringLiteral("Panel") : panelTitle,
      placeholder);
  title->setAlignment(Qt::AlignCenter);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  title->setFont(titleFont);
  auto *hint = new QLabel(QStringLiteral("Open this panel to initialize it."),
                          placeholder);
  hint->setAlignment(Qt::AlignCenter);
  hint->setWordWrap(true);
  layout->addStretch(1);
  layout->addWidget(title);
  layout->addWidget(hint);
  layout->addStretch(1);
  return placeholder;
}

QPalette dockSurfacePalette(const QPalette &basePalette) {
  const auto &theme = ArtifactCore::currentDCCTheme();
  QPalette palette = basePalette;
  const QColor background = QColor(theme.backgroundColor);
  const QColor surface = QColor(theme.secondaryBackgroundColor);
  const QColor text = QColor(theme.textColor);
  const QColor accent = QColor(theme.selectionColor);
  palette.setColor(QPalette::Window, surface);
  palette.setColor(QPalette::Base, background);
  palette.setColor(QPalette::AlternateBase, surface.darker(108));
  palette.setColor(QPalette::Button, surface);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::PlaceholderText, text.darker(145));
  palette.setColor(QPalette::Highlight, accent);
  palette.setColor(QPalette::HighlightedText, background);
  return palette;
}

void applyLazyDockSurfacePalette(QWidget *widget) {
  if (!widget) {
    return;
  }

  const QPalette palette = dockSurfacePalette(widget->palette());
  widget->setAutoFillBackground(true);
  widget->setPalette(palette);
  for (auto *child : widget->findChildren<QWidget *>()) {
    if (!child || child->testAttribute(Qt::WA_PaintOnScreen)) {
      continue;
    }
    child->setPalette(dockSurfacePalette(child->palette()));
    if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(child)) {
      if (auto *viewport = scrollArea->viewport()) {
        viewport->setAutoFillBackground(true);
        viewport->setPalette(dockSurfacePalette(viewport->palette()));
      }
    }
  }
}

void restoreInheritedUpdates(QWidget *widget) {
  if (!widget) {
    return;
  }

  const auto restoreOne = [](QWidget *candidate) {
    if (!candidate || candidate->testAttribute(Qt::WA_ForceUpdatesDisabled)) {
      return;
    }
    if (!candidate->updatesEnabled()) {
      candidate->setUpdatesEnabled(true);
    }
    candidate->update();
  };

  restoreOne(widget);
  for (auto *child : widget->findChildren<QWidget *>()) {
    if (!child || child->isWindow()) {
      continue;
    }
    restoreOne(child);
    if (auto *scrollArea = qobject_cast<QAbstractScrollArea *>(child)) {
      if (auto *viewport = scrollArea->viewport()) {
        restoreOne(viewport);
      }
    }
  }
}

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
void enableDockDropPreview(QObject *root);

void prepareDockDropOverlayWindow(QWidget *widget) {
  if (!widget) {
    return;
  }

  widget->setAttribute(Qt::WA_ShowWithoutActivating, true);
#if defined(_WIN32)
  widget->setWindowFlag(Qt::WindowStaysOnTopHint, true);
#endif
}

void prepareDockDropOverlays(ads::CDockManager *dockManager) {
  if (!dockManager) {
    return;
  }

  // Dock registration calls this repeatedly while the startup layout is being
  // assembled.  A full descendant scan after every registration makes startup
  // cost grow with the square of the dock count.  Coalesce the work and inspect
  // the final QADS tree once when control returns to the event loop.
  if (dockManager->property("artifactOverlayRefreshScheduled").toBool()) {
    return;
  }
  dockManager->setProperty("artifactOverlayRefreshScheduled", true);
  QTimer::singleShot(0, dockManager, [dockManager]() {
    dockManager->setProperty("artifactOverlayRefreshScheduled", false);
    enableDockDropPreview(dockManager);
    for (auto *widget : dockManager->findChildren<QWidget *>()) {
      if (!widget) {
        continue;
      }
      const QString className =
          QString::fromLatin1(widget->metaObject()->className());
      if (className.contains(QStringLiteral("DockOverlay"),
                             Qt::CaseInsensitive)) {
        prepareDockDropOverlayWindow(widget);
      }
    }
  });
}
#endif

void applyWorkspaceVisibility(ArtifactMainWindow *window, WorkspaceMode mode) {
  if (!window) {
    return;
  }

  const QStringList dockTitles = window->dockTitles();
  const WorkspaceVisibilityRule *rule = workspaceVisibilityRuleFor(mode);
  QSet<QString> visibleTitles;
  for (const QString &title : rule->visibleTitles) {
    visibleTitles.insert(title);
  }
  for (const QString &title : rule->hiddenTitles) {
    visibleTitles.remove(title);
  }
  for (const QString &title : dockTitles) {
    if (isTimelineDockTitle(title)) {
      if (mode == WorkspaceMode::Animation) {
        visibleTitles.insert(title);
      } else {
        visibleTitles.remove(title);
      }
    }
  }

  // Apply each dock's final state once. The previous hide-all/show-selected
  // sequence toggled every visible dock twice and serialized the full ADS graph
  // for each toggle.
  for (const QString &title : dockTitles) {
    if (!title.isEmpty()) {
      window->setDockVisible(title, visibleTitles.contains(title));
    }
  }
}

void applyWorkspaceMode(ArtifactMainWindow *window, WorkspaceMode mode) {
  if (!window) {
    return;
  }
  constexpr auto kWorkspaceBatchDepth =
      "artifactWorkspaceVisibilityBatchDepth";
  const int previousDepth = window->property(kWorkspaceBatchDepth).toInt();
  window->setProperty(kWorkspaceBatchDepth, previousDepth + 1);
  applyWorkspaceVisibility(window, mode);
  window->setProperty(kWorkspaceBatchDepth, previousDepth);
}

void refreshFloatingWidgetTree(QWidget *widget) {
  if (!widget) {
    return;
  }

  restoreInheritedUpdates(widget);
  applyLazyDockSurfacePalette(widget);

  // With WA_OpaquePaintEvent removed from the project panel hierarchy,
  // Qt's backing store now properly clears newly exposed areas during
  // resize.  We only need to ensure QTreeView's internal item layout
  // is up-to-date and schedule a normal (deferred) repaint.
  //
  // Avoid layout->activate(), forced repaint(), or updateGeometry()
  // on children — these fight against Qt's own layout propagation
  // and can produce stale-geometry artifacts during live resize.

  for (auto *projectView :
       widget->findChildren<Artifact::ArtifactProjectView *>()) {
    if (projectView) {
      projectView->refreshVisibleContent();
    }
  }

  widget->update();
}

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
void enableDockDropPreview(QObject *root) {
  if (!root) {
    return;
  }

  const auto overlays = root->findChildren<ads::CDockOverlay *>();
  for (auto *overlay : overlays) {
    if (overlay) {
      overlay->enableDropPreview(true);
    }
  }
}

ads::CFloatingDockContainer *findFloatingDockContainer(QWidget *widget) {
  QWidget *cursor = widget;
  while (cursor) {
    if (auto *floatingWidget =
            qobject_cast<ads::CFloatingDockContainer *>(cursor)) {
      return floatingWidget;
    }
    cursor = cursor->parentWidget();
  }
  return nullptr;
}

void refreshDockWidgetSurface(ads::CDockWidget *dock) {
  if (!dock) {
    return;
  }

  dock->update();

  if (auto *tab = dock->tabWidget()) {
    tab->updateStyle();
    tab->update();
  }

  if (auto *content = dock->widget()) {
    applyLazyDockSurfacePalette(content);
    content->ensurePolished();
    if (auto *layout = content->layout()) {
      layout->activate();
    }
    content->updateGeometry();
    content->update();
  }
}

void scheduleFloatingRefresh(ads::CFloatingDockContainer *floatingWidget) {
  if (!floatingWidget) {
    return;
  }

  if (floatingWidget->property("artifactFloatingRefreshScheduled").toBool()) {
    return;
  }

  floatingWidget->setProperty("artifactFloatingRefreshScheduled", true);
  QTimer::singleShot(0, floatingWidget, [floatingWidget]() {
    floatingWidget->setProperty("artifactFloatingRefreshScheduled", false);
    refreshFloatingWidgetTree(floatingWidget);
    QTimer::singleShot(16, floatingWidget, [floatingWidget]() {
      refreshFloatingWidgetTree(floatingWidget);
    });
  });
}
#endif

void pushDockLayoutSnapshot(ArtifactMainWindow *window,
                            const QByteArray &beforeState,
                            const QString &label) {
  if (!window || beforeState.isEmpty()) {
    return;
  }

  const QByteArray afterState = window->saveDockManagerState();
  if (afterState.isEmpty() || afterState == beforeState) {
    return;
  }

  const QPointer<ArtifactMainWindow> windowGuard(window);
  if (auto *mgr = UndoManager::instance()) {
    const bool pushed = mgr->push(std::make_unique<LayoutSnapshotCommand>(
        label, beforeState, afterState,
        [windowGuard](const QByteArray &state) -> bool {
          return windowGuard ? windowGuard->restoreDockManagerState(state)
                             : false;
        }));
    if (!pushed && windowGuard) {
      windowGuard->restoreDockManagerState(beforeState);
    }
  }
}

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
void prepareFloatingDockContainer(ads::CFloatingDockContainer *floatingWidget,
                                  QObject *eventFilterOwner);

void wireDockWidgetSignals(ads::CDockWidget *dock, QObject *owner,
                           ArtifactQadsDockAdapter *backend = nullptr) {
  if (!dock || !owner ||
      dock->property("artifactFloatingHooksInstalled").toBool()) {
    return;
  }

  dock->setProperty("artifactFloatingHooksInstalled", true);

  QObject::connect(
      dock, &ads::CDockWidget::topLevelChanged, owner,
      [dock, owner, backend](bool) {
        refreshDockWidgetSurface(dock);
        syncTrackedDockState(backend, dock);
        if (auto *floatingWidget = findFloatingDockContainer(dock)) {
          prepareFloatingDockContainer(floatingWidget, owner);
        }
      });

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, owner,
      [dock, owner, backend](bool) {
        refreshDockWidgetSurface(dock);
        syncTrackedDockState(backend, dock);
        if (auto *floatingWidget = findFloatingDockContainer(dock)) {
          scheduleFloatingRefresh(floatingWidget);
        }
      });
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer *floatingWidget,
                                  QObject *eventFilterOwner) {
  if (!floatingWidget) {
    return;
  }

  qInfo() << "[MainWindow][Floating] prepare container"
          << "object=" << floatingWidget
          << "visible=" << floatingWidget->isVisible()
          << "minimized=" << floatingWidget->isMinimized()
          << "geometry=" << floatingWidget->geometry();

  if (eventFilterOwner) {
    floatingWidget->removeEventFilter(eventFilterOwner);
    floatingWidget->installEventFilter(eventFilterOwner);
  }

  restoreInheritedUpdates(floatingWidget);
  applyDarkNativeTitleBar(floatingWidget);
  applyLazyDockSurfacePalette(floatingWidget);
  floatingWidget->ensurePolished();
  if (auto *layout = floatingWidget->layout()) {
    layout->activate();
  }
  floatingWidget->updateGeometry();
  floatingWidget->update();
  scheduleFloatingRefresh(floatingWidget);
}
#endif

constexpr auto kUnsavedCloseGuardSatisfiedProperty =
    "artifactUnsavedCloseGuardSatisfied";

bool confirmUnsavedChangesForClose(QWidget *parent)
{
  auto *service = ArtifactProjectService::instance();
  if (!service || !service->hasProject()) {
    return true;
  }

  auto project = service->getCurrentProjectSharedPtr();
  if (!project) {
    return true;
  }

  bool hasUnsavedChanges = project->isDirty();
  if (!hasUnsavedChanges) {
    if (auto *undoManager = UndoManager::instance()) {
      hasUnsavedChanges = undoManager->hasUnsavedChanges();
    }
  }
  if (!hasUnsavedChanges) {
    return true;
  }

  QMessageBox box(parent);
  box.setWindowTitle(QStringLiteral("保存の確認"));
  box.setIcon(QMessageBox::Warning);
  box.setText(QStringLiteral(
      "プロジェクトに変更があります。終了前に保存しますか？"));
  box.setInformativeText(QStringLiteral(
      "未保存の変更は失われる可能性があります。"));
  auto *saveButton = box.addButton(QStringLiteral("保存"),
                                    QMessageBox::AcceptRole);
  auto *discardButton = box.addButton(QStringLiteral("破棄"),
                                       QMessageBox::DestructiveRole);
  auto *cancelButton = box.addButton(QStringLiteral("キャンセル"),
                                      QMessageBox::RejectRole);
  box.setDefaultButton(saveButton);
  box.exec();

  if (box.clickedButton() == static_cast<QAbstractButton *>(cancelButton)) {
    return false;
  }
  if (box.clickedButton() == static_cast<QAbstractButton *>(discardButton)) {
    return true;
  }

  auto &manager = ArtifactProjectManager::getInstance();
  QString path = manager.currentProjectPath();
  if (path.isEmpty()) {
    path = QFileDialog::getSaveFileName(
        parent, QStringLiteral("プロジェクトを保存"), QString(),
        QStringLiteral("Artifact Project (*.artifact *.json);;All Files (*.*)"));
    if (path.isEmpty()) {
      return false;
    }
  }

  const auto result = manager.saveToFile(path);
  if (result.success) {
    return true;
  }

  const QString error = result.errorMessage.trimmed();
  QMessageBox::warning(
      parent, QStringLiteral("保存できませんでした"),
      error.isEmpty()
          ? QStringLiteral(
                "変更を保存できませんでした。保存先とプロジェクトの状態を確認してください。")
          : QStringLiteral("変更を保存できませんでした。\n%1").arg(error));
  return false;
}
} // namespace

W_OBJECT_IMPL(ArtifactMainWindow)

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
// Temporary QADS compatibility adapter. Callers express layout intent with
// Artifact::DockArea; only this adapter knows the QADS area enum.
class ArtifactQadsDockAdapter {
public:
  ArtifactQadsDockAdapter() = default;

  DockBackendKind backendKind() const { return DockBackendKind::Native; }

  DockBackendCapabilities capabilities() const {
    return DockBackendCapabilities{true, true, true};
  }

  CDockManager *createManager(QWidget *parent) {
    if (!manager_) {
      manager_ = new CDockManager(parent);
    }
    return manager_;
  }

  CDockManager *manager() const { return manager_; }

  QList<CDockWidget *> &dockWidgets() { return dockWidgets_; }

  void syncDockState() {
    QHash<CDockAreaWidget *, QStringList> areaDockIds;
    for (auto it = layoutEntries_.begin(); it != layoutEntries_.end(); ++it) {
      auto *dock = it.key();
      if (!dock) {
        continue;
      }
      auto &entry = it.value();
      entry.visible = dock->isVisible() && !dock->isClosed();
      entry.pinned = dock->property("artifactDockPinned").toBool();
      if (auto *floating = findFloatingDockContainer(dock)) {
        entry.floating = true;
        entry.floatingGeometry = floating->geometry();
      } else {
        entry.floating = false;
        if (auto *area = dock->dockAreaWidget()) {
          areaDockIds[area].push_back(entry.dockId);
        }
      }
      layoutRegistry_.upsert(entry);
    }
    for (auto it = areaDockIds.cbegin(); it != areaDockIds.cend(); ++it) {
      QStringList dockIds = it.value();
      dockIds.removeDuplicates();
      dockIds.sort();
      const QString tabGroup =
          QStringLiteral("tabs:") + dockIds.join(QStringLiteral("|"));
      for (auto entryIt = layoutEntries_.begin();
           entryIt != layoutEntries_.end(); ++entryIt) {
        if (entryIt.key() && entryIt.key()->dockAreaWidget() == it.key()) {
          entryIt.value().tabGroup = tabGroup;
          layoutRegistry_.upsert(entryIt.value());
        }
      }
    }
  }

  QByteArray portableLayoutState() {
    syncDockState();
    auto entries = layoutRegistry_.values();
    std::sort(entries.begin(), entries.end(),
              [](const DockLayoutEntry &left, const DockLayoutEntry &right) {
                return left.dockId < right.dockId;
              });
    DockLayoutDocument document;
    document.entries = entries;
    return QJsonDocument(document.toJson()).toJson(QJsonDocument::Compact);
  }

  bool restorePortableLayoutState(const QByteArray &state) {
    const QJsonDocument document = QJsonDocument::fromJson(state);
    QList<DockLayoutEntry> entries;
    if (document.isObject()) {
      const DockLayoutDocument portableDocument =
          DockLayoutDocument::fromJson(document.object());
      if (portableDocument.version != kDockLayoutDocumentVersion) {
        return false;
      }
      entries = portableDocument.entries;
    } else if (document.isArray()) {
      // Accept the pre-envelope format written by the first migration build.
      for (const auto &value : document.array()) {
        if (value.isObject()) {
          const DockLayoutEntry entry =
              dockLayoutEntryFromJson(value.toObject());
          if (isValidDockLayoutEntry(entry)) {
            entries.push_back(entry);
          }
        }
      }
    } else {
      return false;
    }

    // Normalize the registry before touching QADS.  This makes tab-group
    // restoration independent of JSON order and prevents duplicate dock IDs
    // from applying placement twice.
    QList<DockLayoutEntry> normalizedEntries;
    QHash<QString, bool> seenDockIds;
    for (const auto &entry : entries) {
      if (entry.dockId.isEmpty() || seenDockIds.contains(entry.dockId)) {
        continue;
      }
      seenDockIds.insert(entry.dockId, true);
      normalizedEntries.push_back(entry);
      for (auto *dock : dockWidgets_) {
        if (dock && dock->objectName() == entry.dockId) {
          layoutEntries_[dock] = entry;
          layoutRegistry_.upsert(entry);
          break;
        }
      }
    }

    bool restoredAny = false;
    for (const auto &entry : normalizedEntries) {
      if (entry.dockId.isEmpty()) {
        continue;
      }
      for (auto *dock : dockWidgets_) {
        if (!dock || dock->objectName() != entry.dockId) {
          continue;
        }
        if (entry.floating) {
          if (!findFloatingDockContainer(dock)) {
            auto *container = addFloatingDockWidget(dock);
            setFloatingGeometry(container, entry.floatingGeometry);
          }
        } else {
          CDockAreaWidget *tabArea = nullptr;
          if (!entry.tabGroup.isEmpty()) {
            for (auto *candidate : dockWidgets_) {
              if (!candidate || candidate == dock) {
                continue;
              }
              const auto candidateEntry =
                  layoutRegistry_.value(candidate->objectName());
              if (candidateEntry.tabGroup == entry.tabGroup &&
                  candidate->dockAreaWidget()) {
                tabArea = candidate->dockAreaWidget();
                break;
              }
            }
          }
          if (tabArea) {
            addDockWidgetToArea(dock, tabArea);
          } else if (manager_) {
            manager_->addDockWidget(toAdsDockArea(entry.area), dock);
          }
        }
        dock->setProperty("artifactDockPinned", entry.pinned);
        setPinned(dock, entry.pinned);
        setVisible(dock, entry.visible);
        restoredAny = true;
        break;
      }
    }
    return restoredAny;
  }

  void trackDock(CDockWidget *dock, DockArea area,
                 const QString &tabGroup = {}, bool floating = false) {
    if (!dock) {
      return;
    }
    const QString dockId = dock->objectName().trimmed();
    if (dockId.isEmpty()) {
      qWarning() << "[ArtifactQadsDockAdapter] refusing dock without objectName"
                 << dock;
      return;
    }
    for (auto it = layoutEntries_.cbegin(); it != layoutEntries_.cend(); ++it) {
      if (it.key() != dock && it.value().dockId == dockId) {
        qWarning() << "[ArtifactQadsDockAdapter] refusing duplicate dock ID"
                   << dockId;
        return;
      }
    }
    if (layoutEntries_.contains(dock)) {
      layoutRegistry_.remove(layoutEntries_.value(dock).dockId);
    }
    DockLayoutEntry entry;
    entry.dockId = dockId;
    entry.area = area;
    entry.tabGroup = tabGroup;
    entry.floating = floating;
    entry.floatingGeometry = dock->property("artifactDeferredFloatingGeometry").toRect();
    layoutEntries_[dock] = entry;
    layoutRegistry_.upsert(entry);
  }

  void prepareOverlays() {
    if (manager_) {
      prepareDockDropOverlays(manager_);
    }
  }

  void connectFloatingWidgetCreated(QObject *context,
                                    QObject *eventFilterOwner) {
    if (!manager_ || !context || !eventFilterOwner) {
      return;
    }
    QObject::connect(
        manager_, &CDockManager::floatingWidgetCreated, context,
        [this, eventFilterOwner](CFloatingDockContainer *floatingWidget) {
          prepareFloatingDockContainer(floatingWidget, eventFilterOwner);
          prepareOverlays();
        });
  }

  void addDockWidget(DockArea area, CDockWidget *dock) {
    if (manager_ && dock) {
      manager_->addDockWidget(toAdsDockArea(area), dock);
    }
  }

  void addDockWidgetToArea(CDockWidget *dock, CDockAreaWidget *targetArea) {
    if (manager_ && dock && targetArea) {
      manager_->addDockWidget(CenterDockWidgetArea, dock, targetArea);
    }
  }

  void addDockWidgetToTab(CDockWidget *dock, CDockAreaWidget *targetArea) {
    if (manager_ && dock && targetArea) {
      manager_->addDockWidgetTabToArea(dock, targetArea);
    }
  }

  void setSplitterSizes(CDockAreaWidget *area, const QList<int> &sizes) {
    if (manager_ && area) {
      manager_->setSplitterSizes(area, sizes);
    }
  }

  void setCentralWidget(CDockWidget *dock) {
    if (manager_ && dock) {
      manager_->setCentralWidget(dock);
    }
  }

  QList<CFloatingDockContainer *> floatingWidgets() const {
    return manager_ ? manager_->floatingWidgets()
                    : QList<CFloatingDockContainer *>{};
  }

  CFloatingDockContainer *addFloatingDockWidget(
      CDockWidget *dock, const QRect &geometry = {}) {
    auto *container = manager_ && dock ? manager_->addDockWidgetFloating(dock)
                                       : nullptr;
    if (container && geometry.isValid()) {
      setFloatingGeometry(container, geometry);
      updateTrackedFloatingGeometry(dock, geometry);
    }
    return container;
  }

  void setFloatingGeometry(CFloatingDockContainer *container,
                           const QRect &geometry) {
    if (container && geometry.isValid()) {
      container->setGeometry(geometry);
    }
  }

  void updateTrackedFloatingGeometry(CDockWidget *dock,
                                      const QRect &geometry) {
    if (dock && layoutEntries_.contains(dock)) {
      layoutEntries_[dock].floatingGeometry = geometry;
      layoutRegistry_.upsert(layoutEntries_.value(dock));
    }
  }

  bool materializeDeferredFloatingDock(CDockWidget *dock) {
    if (!manager_ || !dock ||
        !dock->property("artifactDeferredFloatingContainer").toBool() ||
        dock->property("artifactDeferredFloatingMaterialized").toBool() ||
        dock->property("artifactRespectRestoredDockPlacement").toBool()) {
      return false;
    }
    if (findFloatingDockContainer(dock)) {
      dock->setProperty("artifactDeferredFloatingMaterialized", true);
      return true;
    }
    auto *container = addFloatingDockWidget(dock);
    if (!container) {
      return false;
    }
    setFloatingGeometry(
        container, dock->property("artifactDeferredFloatingGeometry").toRect());
    updateTrackedFloatingGeometry(
        dock, dock->property("artifactDeferredFloatingGeometry").toRect());
    dock->setProperty("artifactDeferredFloatingMaterialized", true);
    return true;
  }

  QByteArray saveState() const {
    return manager_ ? manager_->saveState() : QByteArray{};
  }

  bool restoreState(const QByteArray &state) {
    return manager_ && manager_->restoreState(state);
  }

  void setVisible(CDockWidget *dock, bool visible) {
    if (dock) {
      dock->toggleView(visible);
      if (layoutEntries_.contains(dock)) {
        layoutEntries_[dock].visible = visible;
        layoutRegistry_.upsert(layoutEntries_.value(dock));
      }
    }
  }

  void activate(CDockWidget *dock) {
    if (dock) {
      dock->setAsCurrentTab();
      dock->raise();
    }
  }

  bool close(CDockWidget *dock) {
    if (!dock) {
      return false;
    }
    dock->closeDockWidget();
    if (layoutEntries_.contains(dock)) {
      layoutEntries_[dock].visible = false;
      layoutRegistry_.upsert(layoutEntries_.value(dock));
    }
    return true;
  }

  void setPinned(CDockWidget *dock, bool pinned) {
    if (!dock) {
      return;
    }
    dock->setProperty("artifactDockPinned", pinned);
    if (layoutEntries_.contains(dock)) {
      layoutEntries_[dock].pinned = pinned;
      layoutRegistry_.upsert(layoutEntries_.value(dock));
    }
    auto features = dock->features();
    features.setFlag(CDockWidget::DockWidgetClosable, !pinned);
    dock->setFeatures(features);
  }

private:
  CDockManager *manager_ = nullptr;
  QList<CDockWidget *> dockWidgets_;
  QHash<CDockWidget *, DockLayoutEntry> layoutEntries_;
  DockLayoutRegistry layoutRegistry_;
};

void syncTrackedDockState(ArtifactQadsDockAdapter *backend,
                          ads::CDockWidget *dock) {
  if (backend) {
    Q_UNUSED(dock);
    backend->syncDockState();
  }
}
#endif // ARTIFACT_QADS_COMPAT_BACKEND

class ArtifactMainWindow::Impl {
public:
  Impl() = default;

  ~Impl() {
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
    delete dockBackend;
#endif
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  ArtifactQadsDockAdapter *dockBackend = nullptr;
#endif
  DockStyleManager *dockStyleManager = nullptr;
  ArtifactToolBar *toolBar = nullptr;
  ArtifactToolOptionsBar *toolOptionsBar = nullptr;
  QToolBar *toolOptionsHost = nullptr;
  QToolButton *workspaceButton = nullptr;
  QVBoxLayout *rootLayout = nullptr;
  ArtifactMenuBar *menuBar = nullptr;
  QStatusBar *statusBar = nullptr;
  QWidget *centralWidgetHost = nullptr;
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  CDockWidget *primaryCenterDock = nullptr;
  bool primaryCenterDockAssigned = false;
#endif
  QVBoxLayout *centralWorkspaceLayout = nullptr;
  QWidget *centralWorkspaceWidget = nullptr;
  QString centralWorkspaceTitle;
  QByteArray defaultDockManagerState;
  NativeDockSurface *nativeDockSurface = nullptr;
  QHash<QString, QPointer<QWidget>> nativeDockWidgets;
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  QList<CDockWidget *> dockWidgets;
#endif
  WorkspaceMode workspaceMode_ = WorkspaceMode::Default;
  bool immersiveMode_ = false;
  Qt::WindowStates immersivePreviousWindowState_ = Qt::WindowNoState;
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  QHash<CDockWidget *, bool> immersiveDockVisibility_;
  QPointer<CDockWidget> immersiveTargetDock_;
#endif
  QHash<QString, bool> nativeImmersiveDockVisibility_;
  QString nativeImmersiveTargetDock_;
  bool focusMode_ = false;
  QHash<QWidget *, bool> focusChromeVisibility_;
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  QHash<CDockWidget *, bool> focusDockVisibility_;
#endif
  QHash<QString, bool> nativeFocusDockVisibility_;
  ArtifactWelcomeWidget* welcomeWidget = nullptr;
  bool menuBarInitialized = false;
  bool initialLayoutApplied = false;
  bool startupRefreshScheduled = false;
  bool startupLayoutFrozen = true;
  bool startupLayoutApplying = false;
  bool recordLayoutMutations = true;
  ArtifactAICloudWidget *aiCloudWidget_ = nullptr;
  QLabel *previewResolutionLabel = nullptr;
  QPointer<QLabel> statusZoomLabel;
  QPointer<QLabel> statusCoordinatesLabel;
  QPointer<QLabel> statusMemoryLabel;
  QPointer<QLabel> statusFpsLabel;
  bool hasStatusZoomLevel = false;
  float statusZoomLevel = 100.0f;
  bool hasStatusCoordinates = false;
  int statusCoordinateX = 0;
  int statusCoordinateY = 0;
  bool hasStatusMemoryUsage = false;
  uint64_t statusMemoryUsageMB = 0;
  bool hasStatusFPS = false;
  double statusFPS = 0.0;

  QLabel *ensureStatusLabel(QPointer<QLabel> &label,
                            const QString &objectName,
                            const QString &accessibleName) {
    if (!statusBar) {
      return nullptr;
    }
    if (!label) {
      label = statusBar->findChild<QLabel *>(objectName);
    }
    if (!label || label->parent() != statusBar) {
      label = new QLabel(statusBar);
      label->setObjectName(objectName);
      label->setAccessibleName(accessibleName);
      statusBar->addPermanentWidget(label);
    }
    return label;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  QHash<CDockWidget *, std::function<QWidget *()>> lazyDockFactories;
#endif
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QString nativeDockIdForWidget(const QWidget *widget) const {
    if (!widget) {
      return {};
    }
    for (auto it = nativeDockWidgets.cbegin();
         it != nativeDockWidgets.cend(); ++it) {
      if (it.value() == widget ||
          (it.value() && it.value()->isAncestorOf(widget))) {
        return it.key();
      }
    }
    return {};
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  bool createLazyDockWidgetNow(ArtifactMainWindow *owner, CDockWidget *dock,
                               const QString &reason) {
    if (!owner || !dock || dock->property("artifactLazyWidgetCreated").toBool()) {
      if (dock) {
        dock->setProperty("artifactLazyWidgetCreationPending", false);
      }
      return false;
    }
    if (!lazyDockFactories.contains(dock)) {
      dock->setProperty("artifactLazyWidgetCreationPending", false);
      dock->setProperty("artifactLazyFactoryAvailable", false);
      dock->setProperty("artifactLazyWidgetLastError",
                        QStringLiteral("factory is not registered"));
      WidgetCreationDiagnostics::record(
          nullptr, dock->windowTitle(), QStringLiteral("lazy-dock-factory"),
          reason, 0.0, 0.0, QStringLiteral("factory-missing"),
          dock->windowTitle(), dock->objectName(),
          QStringLiteral("factory is not registered"));
      return false;
    }

    QElapsedTimer totalTimer;
    totalTimer.start();
    dock->setProperty("artifactLazyWidgetCreationPending", true);
    auto factory = lazyDockFactories.take(dock);
    QElapsedTimer factoryTimer;
    factoryTimer.start();
    QWidget *widget = factory ? factory() : nullptr;
    const double factoryMs =
        static_cast<double>(factoryTimer.nsecsElapsed()) / 1000000.0;
    if (!widget) {
      // A failed factory must remain retryable. Previously take() permanently
      // removed it, leaving an uncreated dock with no way to recover.
      const bool factoryRestored = static_cast<bool>(factory);
      if (factoryRestored) {
        lazyDockFactories.insert(dock, std::move(factory));
      }
      dock->setProperty("artifactLazyFactoryAvailable", factoryRestored);
      dock->setProperty("artifactLazyWidgetCreationPending", false);
      dock->setProperty("artifactLazyWidgetLastError",
                        QStringLiteral("factory returned null"));
      const double totalMs =
          static_cast<double>(totalTimer.nsecsElapsed()) / 1000000.0;
      dock->setProperty("artifactLazyWidgetCreateReason", reason);
      dock->setProperty("artifactLazyWidgetFactoryMs", factoryMs);
      dock->setProperty("artifactLazyWidgetTotalMs", totalMs);
      WidgetCreationDiagnostics::record(
          nullptr, dock->windowTitle(), QStringLiteral("lazy-dock-factory"),
          reason, factoryMs, totalMs, QStringLiteral("factory-null"),
          dock->windowTitle(), dock->objectName(),
          QStringLiteral("factory returned null; factory retained for retry"));
      return false;
    }

    QWidget *placeholder = dock->widget();
    dock->setProperty("artifactLazyWidgetCreated", true);
    dock->setProperty("artifactLazyFactoryAvailable", false);
    dock->setProperty("artifactLazyWidgetCreationPending", false);
    dock->setProperty("artifactLazyWidgetStartupPending", false);
    dock->setProperty("artifactLazyWidgetLastError", QVariant());
    applyLazyDockSurfacePalette(widget);
    const bool forceNoScrollArea =
        dock->property("artifactLazyForceNoScrollArea").toBool();
    const auto insertMode =
        dock->property("artifactLazyFloatingDock").toBool() ||
                forceNoScrollArea
            ? CDockWidget::ForceNoScrollArea
            : CDockWidget::AutoScrollArea;
    dock->setWidget(widget, insertMode);
    if (auto *layout = widget->layout()) {
      layout->activate();
    }
    widget->ensurePolished();
    widget->show();
    widget->updateGeometry();
    widget->update();
    QTimer::singleShot(0, owner, [owner, dock]() {
      if (!owner || !dock) {
        return;
      }
      refreshDockWidgetSurface(dock);
      dock->updateGeometry();
      dock->update();
    });
    if (dock->windowTitle() == QStringLiteral("AI Cloud")) {
      aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
    }
    if (placeholder && placeholder != widget) {
      placeholder->deleteLater();
    }
    const double totalMs =
        static_cast<double>(totalTimer.nsecsElapsed()) / 1000000.0;
    dock->setProperty("artifactLazyWidgetCreateReason", reason);
    dock->setProperty("artifactLazyWidgetFactoryMs", factoryMs);
    dock->setProperty("artifactLazyWidgetTotalMs", totalMs);
    dock->setProperty("artifactLazyWidgetClass",
                      QString::fromLatin1(widget->metaObject()->className()));
    dock->setProperty("artifactLazyWidgetCreatedAtUtc",
                      QDateTime::currentDateTimeUtc().toString(
                          Qt::ISODateWithMs));
    WidgetCreationDiagnostics::record(
        widget, dock->windowTitle(), QStringLiteral("lazy-dock-factory"),
        reason, factoryMs, totalMs, QStringLiteral("created"),
        dock->windowTitle(), dock->objectName());
    return true;
  }

#endif // ARTIFACT_QADS_COMPAT_BACKEND

  void syncTextToolOptions(ArtifactMainWindow *owner) {
    if (!owner || !toolOptionsBar) {
      return;
    }

    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(current);
    if (!textLayer) {
      toolOptionsBar->clearTextOptions();
      return;
    }

    toolOptionsBar->setTextOptions(
        textLayer->fontFamily().toQString(),
        static_cast<int>(std::max(1.0, static_cast<double>(textLayer->fontSize()))),
        textLayer->isBold(),
        textLayer->isItalic(), textLayer->isUnderline(),
        static_cast<int>(textLayer->horizontalAlignment()),
        static_cast<int>(textLayer->verticalAlignment()),
        static_cast<int>(textLayer->wrapMode()),
        static_cast<int>(textLayer->layoutMode()), true);

  }

  void syncShapeToolOptions(ArtifactMainWindow *owner) {
    if (!owner || !toolOptionsBar) {
      return;
    }

    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const auto shapeLayer =
        ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(current);
    if (!shapeLayer) {
      toolOptionsBar->clearShapeOptions();
      return;
    }

    auto fmtDash = [](const std::vector<float> &pat) {
      QStringList parts;
      for (auto v : pat)
        parts << QString::number(static_cast<double>(v), 'f', 1);
      return parts.join(QStringLiteral(","));
    };

    toolOptionsBar->setShapeOptions(
        static_cast<int>(shapeLayer->shapeType()),
        std::max(1, shapeLayer->shapeWidth()),
        std::max(1, shapeLayer->shapeHeight()), shapeLayer->fillEnabled(),
        shapeLayer->strokeEnabled(),
        static_cast<int>(std::lround(std::max(0.0f, shapeLayer->strokeWidth()))),
        static_cast<int>(shapeLayer->strokeCap()),
        static_cast<int>(shapeLayer->strokeJoin()),
        static_cast<int>(shapeLayer->strokeAlign()),
        fmtDash(shapeLayer->dashPattern()),
        static_cast<int>(std::lround(std::max(0.0f, shapeLayer->cornerRadius()))),
        std::max(3, shapeLayer->starPoints()),
        std::clamp(static_cast<int>(
                       std::lround(shapeLayer->starInnerRadius() * 100.0f)),
                   0, 100),
        std::max(3, shapeLayer->polygonSides()), true);

  }
};

ArtifactMainWindow::ArtifactMainWindow(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  ArtifactWidgets::installDialogCentering(this);
  setAccessibleName(QStringLiteral("Artifact Studio main window"));
  setAccessibleDescription(QStringLiteral(
      "Main workspace containing the project, timeline, inspector, and render views."));
  setUpdatesEnabled(false);
  impl_->rootLayout = new QVBoxLayout(this);
  impl_->rootLayout->setContentsMargins(0, 0, 0, 0);
  impl_->rootLayout->setSpacing(0);
  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || impl_->menuBarInitialized)
      return;
    auto *menuBar = new ArtifactMenuBar(this, this);
    impl_->menuBar = menuBar;
    // The menu is created lazily to preserve the existing action bootstrap,
    // but it must remain the first chrome row in the QWidget root layout.
    impl_->rootLayout->insertWidget(0, menuBar);

    // Pass main window reference to view menu for dynamic panel listing
    if (auto *viewMenu = menuBar->findChild<ArtifactViewMenu *>()) {
      viewMenu->setMainWindow(this);
    }

    impl_->menuBarInitialized = true;
    if (impl_->focusMode_) {
      impl_->focusChromeVisibility_.insert(menuBar, true);
      menuBar->hide();
    }
  });

  auto *toolBar = new ArtifactToolBar(this);
  impl_->rootLayout->addWidget(toolBar);
  impl_->toolBar = toolBar;

  auto *workspaceButton = new QToolButton(this);
  impl_->workspaceButton = workspaceButton;
  workspaceButton->setAccessibleName(QStringLiteral("Workspace mode"));
  workspaceButton->setAccessibleDescription(QStringLiteral(
      "Choose the active workspace layout and editing mode."));
  workspaceButton->setText(Artifact::workspaceModeInfo(WorkspaceMode::Default).label);
  workspaceButton->setPopupMode(QToolButton::InstantPopup);
  auto *workspaceMenu = new QMenu(workspaceButton);
  for (const auto &info : Artifact::workspaceModeInfos()) {
    QAction *action = workspaceMenu->addAction(info.label);
    action->setIcon(QIcon(resolveIconPath(info.iconPath)));
    action->setData(static_cast<int>(info.mode));
    QObject::connect(action, &QAction::triggered, this, [this, mode = info.mode]() {
      setWorkspaceMode(mode);
    });
    if (info.mode == WorkspaceMode::Default) {
      workspaceButton->setText(info.label);
    }
  };
  workspaceButton->setMenu(workspaceMenu);
  toolBar->addWidget(workspaceButton);

  impl_->toolOptionsBar = new ArtifactToolOptionsBar(this);
  impl_->toolOptionsBar->clearTextOptions();
  impl_->toolOptionsBar->clearShapeOptions();
  impl_->toolOptionsHost = new QToolBar(this);
  impl_->toolOptionsHost->setObjectName(QStringLiteral("ArtifactToolOptionsBar"));
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    impl_->workspaceMode_ = Artifact::workspaceModeInfoForText(
                                settings->projectDefaultWorkspaceModeText())
                                .mode;
  }
  if (impl_->workspaceButton) {
    impl_->workspaceButton->setText(Artifact::workspaceModeText(impl_->workspaceMode_));
  }
  impl_->toolOptionsHost->setMovable(false);
  impl_->toolOptionsHost->setFloatable(false);
  impl_->toolOptionsHost->setIconSize(QSize(16, 16));
  {
    QPalette pal = impl_->toolOptionsHost->palette();
    pal.setColor(QPalette::Window,
                 QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::Button,
                 QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText,
                 QColor(ArtifactCore::currentDCCTheme().textColor));
    impl_->toolOptionsHost->setPalette(pal);
  }
  impl_->toolOptionsHost->addWidget(impl_->toolOptionsBar);
  impl_->rootLayout->addWidget(impl_->toolOptionsHost);
  toolBar->setToolOptionsBar(impl_->toolOptionsBar);
  toolBar->refreshFromApplicationState();
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<WorkspaceModeChangedEvent>(
          [this](const WorkspaceModeChangedEvent &event) {
            const int modeValue = event.mode;
            const auto apply = [this, modeValue]() {
              if (!impl_ ||
                  modeValue < static_cast<int>(WorkspaceMode::Default) ||
                  modeValue > static_cast<int>(WorkspaceMode::Audio)) {
                return;
              }
              const auto mode = static_cast<WorkspaceMode>(modeValue);
              impl_->workspaceMode_ = mode;
              if (impl_->workspaceButton) {
                impl_->workspaceButton->setText(
                    Artifact::workspaceModeText(mode));
              }
            };
            if (QThread::currentThread() == thread()) {
              apply();
            } else {
              QMetaObject::invokeMethod(this, apply, Qt::QueuedConnection);
            }
          }));

  // Tool diagnostics use the existing internal tool state event. The toolbar
  // action itself remains a local widget boundary.
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ToolChangedEvent>(
          [](const ToolChangedEvent &event) {
            switch (event.toolType) {
            case ToolType::Move:
              qDebug() << "[MainWindow] Move tool selected (W)";
              break;
            case ToolType::Rotation:
              qDebug() << "[MainWindow] Rotate tool selected (E)";
              break;
            case ToolType::Scale:
              qDebug() << "[MainWindow] Scale tool selected (R)";
              break;
            default:
              break;
            }
          }));

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ToolOptionChangedEvent>(
          [this](const ToolOptionChangedEvent &event) {
        const QString &toolName = event.toolName;
        const QString &optionName = event.optionName;
        const QVariant &value = event.value;
        auto *app = ArtifactApplicationManager::instance();
        auto *selection = app ? app->layerSelectionManager() : nullptr;
        const auto current =
            selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};

        if (toolName == QStringLiteral("ブラシ") ||
            toolName == QStringLiteral("消しゴム") ||
            toolName == QStringLiteral("コピースタンプ")) {
          auto *brushTool = app ? app->brushTool() : nullptr;
          if (!brushTool) {
            return;
          }
          if (optionName == QStringLiteral("brushSize") ||
              optionName == QStringLiteral("size") ||
              (toolName == QStringLiteral("コピースタンプ") &&
               optionName == QStringLiteral("radius"))) {
            brushTool->setRadius(
                std::clamp(value.toFloat(), 1.0f, 2500.0f));
          } else if (toolName == QStringLiteral("コピースタンプ") &&
                     optionName == QStringLiteral("aligned")) {
            brushTool->setCloneAligned(value.toBool());
          } else if (toolName == QStringLiteral("コピースタンプ") &&
                     optionName == QStringLiteral("timeOffset")) {
            brushTool->setCloneTimeOffset(value.toInt());
          } else if (optionName == QStringLiteral("brushOpacity") ||
                     optionName == QStringLiteral("opacity") ||
                     optionName == QStringLiteral("strength")) {
            brushTool->setOpacity(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("lastStrokeOnly")) {
            brushTool->setLastStrokeOnly(value.toBool());
          } else if (optionName == QStringLiteral("mode")) {
            brushTool->setEraserModeKind(value.toInt());
          } else if (optionName == QStringLiteral("brushFlow")) {
            brushTool->setFlow(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("brushHardness")) {
            brushTool->setHardness(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("hardness")) {
            brushTool->setHardness(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("angle")) {
            brushTool->setAngle(value.toFloat());
          } else if (optionName == QStringLiteral("roundness")) {
            brushTool->setRoundness(
                std::clamp(value.toFloat() / 100.0f, 0.01f, 1.0f));
          } else if (optionName == QStringLiteral("brushSpacing")) {
            brushTool->setSpacing(
                std::clamp(value.toFloat() / 100.0f, 0.01f, 10.0f));
          } else if (optionName == QStringLiteral("brushAngle")) {
            brushTool->setAngle(value.toFloat());
          } else if (optionName == QStringLiteral("brushRoundness")) {
            brushTool->setRoundness(
                std::clamp(value.toFloat() / 100.0f, 0.01f, 1.0f));
          } else if (optionName == QStringLiteral("sizeJitter")) {
            brushTool->setSizeJitter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("opacityJitter")) {
            brushTool->setOpacityJitter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("scatter")) {
            brushTool->setScatter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("angleJitter")) {
            brushTool->setAngleJitter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("roundnessJitter")) {
            brushTool->setRoundnessJitter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("flowJitter")) {
            brushTool->setFlowJitter(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          } else if (optionName == QStringLiteral("pressureAffectsFlow")) {
            brushTool->setPressureAffectsFlow(value.toBool());
          } else if (optionName == QStringLiteral("pressureAffectsSize")) {
            brushTool->setPressureAffectsSize(value.toBool());
          } else if (optionName == QStringLiteral("pressureAffectsOpacity")) {
            brushTool->setPressureAffectsOpacity(value.toBool());
          } else if (optionName == QStringLiteral("tiltAffectsAngle")) {
            brushTool->setTiltAffectsAngle(value.toBool());
          } else if (optionName == QStringLiteral("tiltAffectsRoundness")) {
            brushTool->setTiltAffectsRoundness(value.toBool());
          } else if (optionName == QStringLiteral("brushColor")) {
            const QStringList channels = value.toString().split(
                QLatin1Char(','), Qt::KeepEmptyParts);
            if (channels.size() == 4) {
              bool okR = false, okG = false, okB = false, okA = false;
              const float r = channels[0].toFloat(&okR);
              const float g = channels[1].toFloat(&okG);
              const float b = channels[2].toFloat(&okB);
              const float a = channels[3].toFloat(&okA);
              if (okR && okG && okB && okA) {
                brushTool->setColor(
                    FloatRGBA(std::clamp(r, 0.0f, 1.0f),
                              std::clamp(g, 0.0f, 1.0f),
                              std::clamp(b, 0.0f, 1.0f),
                              std::clamp(a, 0.0f, 1.0f)));
              }
            }
          }
          QSettings brushSettings;
          brushSettings.setValue(QStringLiteral("brush/%1").arg(optionName),
                                 value);
          return;
        }

        if (toolName == QStringLiteral("モーションスケッチ") &&
            optionName == QStringLiteral("smoothing")) {
          QSettings motionSettings;
          motionSettings.setValue(QStringLiteral("motionSketch/smoothing"), value);
          if (app && app->motionSketchTool()) {
            app->motionSketchTool()->setSmoothing(
                std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f));
          }
          return;
        }

        if (toolName == QStringLiteral("モーションスケッチ") &&
            optionName == QStringLiteral("sampleRate")) {
          QSettings motionSettings;
          motionSettings.setValue(QStringLiteral("motionSketch/sampleRate"), value);
          if (app && app->motionSketchTool()) {
            app->motionSketchTool()->setSampleRate(
                std::clamp(value.toFloat(), 1.0f, 60.0f));
          }
          return;
        }

        if (toolName == QStringLiteral("モーションスケッチ") &&
            optionName == QStringLiteral("showWireframe")) {
          QSettings motionSettings;
          motionSettings.setValue(QStringLiteral("motionSketch/showWireframe"), value);
          if (app && app->motionSketchTool()) {
            app->motionSketchTool()->setShowWireframe(value.toBool());
          }
          return;
        }

        if (toolName == QStringLiteral("テキスト")) {
          const auto textLayer =
              ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(current);
          if (!textLayer) {
            return;
          }

          bool changed = false;
          if (optionName == QStringLiteral("font")) {
            const QString family = value.toString().trimmed();
            if (!family.isEmpty() &&
                textLayer->fontFamily().toQString() != family) {
              textLayer->setFontFamily(
                  ArtifactCore::UniString::fromQString(family));
              changed = true;
            }
          } else if (optionName == QStringLiteral("fontSize")) {
            const float fontSize = std::max(1.0, static_cast<double>(value.toFloat()));
            if (std::abs(textLayer->fontSize() - fontSize) > 0.001f) {
              textLayer->setFontSize(fontSize);
              changed = true;
            }
          } else if (optionName == QStringLiteral("bold")) {
            const bool enabled = value.toBool();
            if (textLayer->isBold() != enabled) {
              textLayer->setBold(enabled);
              changed = true;
            }
          } else if (optionName == QStringLiteral("italic")) {
            const bool enabled = value.toBool();
            if (textLayer->isItalic() != enabled) {
              textLayer->setItalic(enabled);
              changed = true;
            }
          } else if (optionName == QStringLiteral("underline")) {
            const bool enabled = value.toBool();
            if (textLayer->isUnderline() != enabled) {
              textLayer->setUnderline(enabled);
              changed = true;
            }
          } else if (optionName == QStringLiteral("horizontalAlignment")) {
            const auto alignment =
                static_cast<ArtifactCore::TextHorizontalAlignment>(value.toInt());
            if (textLayer->horizontalAlignment() != alignment) {
              textLayer->setHorizontalAlignment(alignment);
              changed = true;
            }
          } else if (optionName == QStringLiteral("verticalAlignment")) {
            const auto alignment =
                static_cast<ArtifactCore::TextVerticalAlignment>(value.toInt());
            if (textLayer->verticalAlignment() != alignment) {
              textLayer->setVerticalAlignment(alignment);
              changed = true;
            }
          } else if (optionName == QStringLiteral("wrapMode")) {
            const auto wrapMode =
                static_cast<ArtifactCore::TextWrapMode>(value.toInt());
            if (textLayer->wrapMode() != wrapMode) {
              textLayer->setWrapMode(wrapMode);
              changed = true;
            }
          } else if (optionName == QStringLiteral("layoutMode")) {
            const auto layoutMode = static_cast<TextLayoutMode>(value.toInt());
            if (textLayer->layoutMode() != layoutMode) {
              textLayer->setLayoutMode(layoutMode);
              changed = true;
            }
          }

          if (!changed) {
            return;
          }

          textLayer->setDirty(LayerDirtyFlag::Property);
          textLayer->addDirtyReason(LayerDirtyReason::UserEdit);
          textLayer->changed();
          if (auto *comp = static_cast<ArtifactAbstractComposition *>(
                  textLayer->composition())) {
            ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                LayerChangedEvent{comp->id().toString(),
                                  textLayer->id().toString(),
                                  LayerChangedEvent::ChangeType::Modified});
          }
          return;
        }

        if (toolName != QStringLiteral("シェイプ")) {
          return;
        }

        const auto shapeLayer =
            ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(current);
        if (!shapeLayer) {
          return;
        }

        bool changed = false;
        if (optionName == QStringLiteral("shapeType")) {
          const auto shapeType = static_cast<Artifact::ShapeType>(value.toInt());
          if (shapeLayer->shapeType() != shapeType) {
            shapeLayer->setShapeType(shapeType);
            changed = true;
          }
        } else if (optionName == QStringLiteral("shapeWidth")) {
          const int width = std::max(1, value.toInt());
          if (shapeLayer->shapeWidth() != width) {
            shapeLayer->setSize(width, shapeLayer->shapeHeight());
            changed = true;
          }
        } else if (optionName == QStringLiteral("shapeHeight")) {
          const int height = std::max(1, value.toInt());
          if (shapeLayer->shapeHeight() != height) {
            shapeLayer->setSize(shapeLayer->shapeWidth(), height);
            changed = true;
          }
        } else if (optionName == QStringLiteral("fillEnabled")) {
          const bool enabled = value.toBool();
          if (shapeLayer->fillEnabled() != enabled) {
            shapeLayer->setFillEnabled(enabled);
            changed = true;
          }
        } else if (optionName == QStringLiteral("strokeEnabled")) {
          const bool enabled = value.toBool();
          if (shapeLayer->strokeEnabled() != enabled) {
            shapeLayer->setStrokeEnabled(enabled);
            changed = true;
          }
        } else if (optionName == QStringLiteral("strokeWidth")) {
          const float strokeWidth = std::max(0.0f, value.toFloat());
          if (std::abs(shapeLayer->strokeWidth() - strokeWidth) > 0.001f) {
            shapeLayer->setStrokeWidth(strokeWidth);
            changed = true;
          }
        } else if (optionName == QStringLiteral("strokeCap")) {
          const auto cap = static_cast<Artifact::StrokeCap>(value.toInt());
          if (shapeLayer->strokeCap() != cap) {
            shapeLayer->setStrokeCap(cap);
            changed = true;
          }
        } else if (optionName == QStringLiteral("strokeJoin")) {
          const auto join = static_cast<Artifact::StrokeJoin>(value.toInt());
          if (shapeLayer->strokeJoin() != join) {
            shapeLayer->setStrokeJoin(join);
            changed = true;
          }
        } else if (optionName == QStringLiteral("strokeAlign")) {
          const auto align = static_cast<Artifact::StrokeAlign>(value.toInt());
          if (shapeLayer->strokeAlign() != align) {
            shapeLayer->setStrokeAlign(align);
            changed = true;
          }
        } else if (optionName == QStringLiteral("dashPattern")) {
          const QString text = value.toString();
          std::vector<float> pattern;
          const auto parts = text.split(QStringLiteral(","),
                                        Qt::SkipEmptyParts);
          for (const auto &p : parts) {
            bool ok = false;
            const float v = p.trimmed().toFloat(&ok);
            if (ok && v > 0.0f)
              pattern.push_back(v);
          }
          shapeLayer->setDashPattern(pattern);
          changed = true;
        } else if (optionName == QStringLiteral("shapePrimary")) {
          switch (shapeLayer->shapeType()) {
          case Artifact::ShapeType::Rect:
          case Artifact::ShapeType::Square: {
            const float radius = std::max(0.0f, value.toFloat());
            if (std::abs(shapeLayer->cornerRadius() - radius) > 0.001f) {
              shapeLayer->setCornerRadius(radius);
              changed = true;
            }
            break;
          }
          case Artifact::ShapeType::Star: {
            const int points = std::max(3, value.toInt());
            if (shapeLayer->starPoints() != points) {
              shapeLayer->setStarPoints(points);
              changed = true;
            }
            break;
          }
          case Artifact::ShapeType::Polygon: {
            const int sides = std::max(3, value.toInt());
            if (shapeLayer->polygonSides() != sides) {
              shapeLayer->setPolygonSides(sides);
              changed = true;
            }
            break;
          }
          default:
            break;
          }
        } else if (optionName == QStringLiteral("shapeSecondary") &&
                   shapeLayer->shapeType() == Artifact::ShapeType::Star) {
          const float innerRadius =
              std::clamp(value.toFloat() / 100.0f, 0.0f, 1.0f);
          if (std::abs(shapeLayer->starInnerRadius() - innerRadius) > 0.001f) {
            shapeLayer->setStarInnerRadius(innerRadius);
            changed = true;
          }
        }

        if (!changed) {
          return;
        }

        shapeLayer->setDirty(LayerDirtyFlag::Property);
        shapeLayer->addDirtyReason(LayerDirtyReason::UserEdit);
        if (auto *comp = static_cast<ArtifactAbstractComposition *>(
                shapeLayer->composition())) {
          ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
              LayerChangedEvent{comp->id().toString(),
                                shapeLayer->id().toString(),
                                LayerChangedEvent::ChangeType::Modified});
        }
      }));

  // Restore the shared brush profile after the option route is established.
  // The toolbar widgets may be recreated independently, so the BrushTool is
  // the single source of truth for the restored values.
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *brushTool = app->brushTool()) {
      QSettings brushSettings;
      brushTool->setRadius(brushSettings.value(QStringLiteral("brush/brushSize"),
                                               brushTool->radius()).toFloat());
      brushTool->setOpacity(brushSettings.value(QStringLiteral("brush/brushOpacity"),
                                                brushTool->opacity() * 100.0f).toFloat() /
                            100.0f);
      brushTool->setFlow(brushSettings.value(QStringLiteral("brush/brushFlow"),
                                             brushTool->flow() * 100.0f).toFloat() /
                        100.0f);
      brushTool->setHardness(brushSettings.value(QStringLiteral("brush/brushHardness"),
                                                 brushTool->hardness() * 100.0f).toFloat() /
                             100.0f);
      brushTool->setSpacing(brushSettings.value(QStringLiteral("brush/brushSpacing"),
                                                brushTool->spacing() * 100.0f).toFloat() /
                            100.0f);
      brushTool->setAngle(brushSettings.value(QStringLiteral("brush/brushAngle"),
                                              brushTool->angle()).toFloat());
      brushTool->setRoundness(brushSettings.value(QStringLiteral("brush/brushRoundness"),
                                                  brushTool->roundness() * 100.0f).toFloat() /
                              100.0f);
      brushTool->setSizeJitter(brushSettings.value(QStringLiteral("brush/sizeJitter"),
                                                   brushTool->sizeJitter() * 100.0f).toFloat() /
                               100.0f);
      brushTool->setOpacityJitter(brushSettings.value(QStringLiteral("brush/opacityJitter"),
                                                      brushTool->opacityJitter() * 100.0f).toFloat() /
                                  100.0f);
      brushTool->setScatter(brushSettings.value(QStringLiteral("brush/scatter"),
                                                brushTool->scatter() * 100.0f).toFloat() /
                            100.0f);
      brushTool->setAngleJitter(brushSettings.value(QStringLiteral("brush/angleJitter"),
                                                    brushTool->angleJitter() * 100.0f).toFloat() /
                                100.0f);
      brushTool->setRoundnessJitter(brushSettings.value(QStringLiteral("brush/roundnessJitter"),
                                                        brushTool->roundnessJitter() * 100.0f).toFloat() /
                                    100.0f);
      brushTool->setFlowJitter(brushSettings.value(QStringLiteral("brush/flowJitter"),
                                                   brushTool->flowJitter() * 100.0f).toFloat() /
                               100.0f);
      brushTool->setPressureAffectsFlow(brushSettings.value(QStringLiteral("brush/pressureAffectsFlow"),
                                                             brushTool->pressureAffectsFlow()).toBool());
      brushTool->setPressureAffectsSize(brushSettings.value(QStringLiteral("brush/pressureAffectsSize"),
                                                             brushTool->pressureAffectsSize()).toBool());
      brushTool->setPressureAffectsOpacity(brushSettings.value(QStringLiteral("brush/pressureAffectsOpacity"),
                                                                brushTool->pressureAffectsOpacity()).toBool());
      brushTool->setTiltAffectsAngle(brushSettings.value(QStringLiteral("brush/tiltAffectsAngle"),
                                                         brushTool->tiltAffectsAngle()).toBool());
      brushTool->setTiltAffectsRoundness(brushSettings.value(QStringLiteral("brush/tiltAffectsRoundness"),
                                                             brushTool->tiltAffectsRoundness()).toBool());
      brushTool->setEraserModeKind(
          brushSettings.value(QStringLiteral("brush/mode"),
                              brushSettings.value(QStringLiteral("eraser/mode"),
                                                  brushTool->eraserModeKind()))
              .toInt());
      brushTool->setLastStrokeOnly(
          brushSettings.value(QStringLiteral("brush/lastStrokeOnly"),
                              brushSettings.value(QStringLiteral("eraser/lastStrokeOnly"),
                                                  brushTool->lastStrokeOnly()))
              .toBool());
      if (!brushSettings.contains(QStringLiteral("brush/brushSize"))) {
        brushTool->setRadius(brushSettings.value(
            QStringLiteral("brush/radius"), brushTool->radius()).toFloat());
      }
      brushTool->setCloneAligned(brushSettings.value(
          QStringLiteral("brush/aligned"), brushTool->cloneAligned()).toBool());
      brushTool->setCloneTimeOffset(brushSettings.value(
          QStringLiteral("brush/timeOffset"), brushTool->cloneTimeOffset()).toInt());
      const QString colorText =
          brushSettings.value(QStringLiteral("brush/brushColor")).toString();
      const QStringList channels = colorText.split(QLatin1Char(','),
                                                    Qt::KeepEmptyParts);
      if (channels.size() == 4) {
        bool okR = false, okG = false, okB = false, okA = false;
        const float r = channels[0].toFloat(&okR);
        const float g = channels[1].toFloat(&okG);
        const float b = channels[2].toFloat(&okB);
        const float a = channels[3].toFloat(&okA);
        if (okR && okG && okB && okA) {
          brushTool->setColor(FloatRGBA(std::clamp(r, 0.0f, 1.0f),
                                        std::clamp(g, 0.0f, 1.0f),
                                        std::clamp(b, 0.0f, 1.0f),
                                        std::clamp(a, 0.0f, 1.0f)));
        }
      }
    }
  }
  impl_->toolOptionsBar->syncBrushOptionsFromTool();
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *motion = app->motionSketchTool()) {
      QSettings motionSettings;
      motion->setSmoothing(motionSettings.value(
          QStringLiteral("motionSketch/smoothing"), motion->smoothing() * 100.0f).toFloat() /
                           100.0f);
      motion->setSampleRate(motionSettings.value(
          QStringLiteral("motionSketch/sampleRate"), motion->sampleRate()).toFloat());
      motion->setShowWireframe(motionSettings.value(
          QStringLiteral("motionSketch/showWireframe"), motion->showWireframe()).toBool());
      motion->setShowBackground(motionSettings.value(
          QStringLiteral("motionSketch/showBackground"), motion->showBackground()).toBool());
    }
  }
  impl_->toolOptionsBar->syncMotionSketchOptionsFromTool();

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>([this](const LayerSelectionChangedEvent&) {
        if (impl_) {
          impl_->syncTextToolOptions(this);
          impl_->syncShapeToolOptions(this);
        }
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent &event) {
            const auto refresh = [this, layerId = event.layerId]() {
              if (!impl_) {
                return;
              }
              auto *app = ArtifactApplicationManager::instance();
              auto *selection = app ? app->layerSelectionManager() : nullptr;
              const auto current = selection ? selection->currentLayer()
                                             : ArtifactAbstractLayerPtr{};
              if (!current || current->id().toString() != layerId) {
                return;
              }
              impl_->syncTextToolOptions(this);
              impl_->syncShapeToolOptions(this);
            };
            if (QThread::currentThread() == thread()) {
              refresh();
            } else {
              QMetaObject::invokeMethod(this, refresh, Qt::QueuedConnection);
            }
          }));
  impl_->syncTextToolOptions(this);
  impl_->syncShapeToolOptions(this);

  if (qApp) {
    qApp->installEventFilter(this);
  }
  impl_->centralWidgetHost = new QWidget(this);
  impl_->centralWidgetHost->setObjectName(QStringLiteral("ArtifactCentralWidgetHost"));
  impl_->centralWidgetHost->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
  impl_->centralWorkspaceLayout = new QVBoxLayout(impl_->centralWidgetHost);
  impl_->centralWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
  impl_->centralWorkspaceLayout->setSpacing(0);
  impl_->nativeDockSurface = new NativeDockSurface(impl_->centralWidgetHost);
  impl_->nativeDockSurface->setObjectName(
      QStringLiteral("ArtifactNativeDockMvpSurface"));
  impl_->centralWorkspaceLayout->addWidget(impl_->nativeDockSurface);
  impl_->rootLayout->addWidget(impl_->centralWidgetHost, 1);
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }

  impl_->statusBar = new QStatusBar(this);
  impl_->rootLayout->addWidget(impl_->statusBar);

  // Let Composition Viewer own the startup empty state so its
  // Create Composition action stays actionable.
  impl_->welcomeWidget = new Artifact::ArtifactWelcomeWidget(impl_->centralWidgetHost);
  impl_->welcomeWidget->setGeometry(impl_->centralWidgetHost->rect());
  impl_->welcomeWidget->hide();
  // Sync welcome widget size with central host when it resizes
  impl_->centralWidgetHost->installEventFilter(this);
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<OpenRecentProjectRequestedEvent>(
      [this](const OpenRecentProjectRequestedEvent& event) {
          const QString path = event.path;
          if (!path.isEmpty()) {
              const QPointer<ArtifactMainWindow> windowGuard(this);
              ArtifactProjectManager::getInstance().loadFromFileAsync(
                  path,
                  [windowGuard, path](const ArtifactProjectImporterResult& result) {
                      if (!windowGuard || result.success) {
                          return;
                      }
                      const QString error = result.errorMessage.toQString();
                      QMessageBox::warning(
                          windowGuard, QStringLiteral("Open Project"),
                          error.isEmpty()
                              ? QStringLiteral("Failed to open project.\n%1").arg(path)
                              : QStringLiteral("Failed to open project.\n%1").arg(error));
                  });
          }
      }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CreateCompositionRequestedEvent>(
          [this](const CreateCompositionRequestedEvent&) {
            auto* svc = ArtifactProjectService::instance();
            if (!svc) return;
            if (!svc->ensureProject()) return;
            svc->createComposition(ArtifactCompositionInitParams::hdPreset());
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ImportAssetsRequestedEvent>(
          [this](const ImportAssetsRequestedEvent&) {
            auto* svc = ArtifactProjectService::instance();
            if (!svc || !svc->ensureProject()) return;
            if (svc) {
                const QStringList files = QFileDialog::getOpenFileNames(
                    this, QStringLiteral("Import Assets"));
                if (!files.isEmpty()) {
                    ArtifactImportAssetsDialog dialog(files, this);
                    if (dialog.exec() == QDialog::Accepted) {
                        const QStringList filtered = dialog.selectedPaths();
                        if (!filtered.isEmpty()) {
                            const QPointer<ArtifactMainWindow> windowGuard(this);
                            svc->importAssetsFromPathsAsync(
                                filtered,
                                [windowGuard](const QStringList& imported) {
                                    if (!windowGuard || !imported.isEmpty()) {
                                        return;
                                    }
                                    QMessageBox::warning(
                                        windowGuard, QStringLiteral("Import Assets"),
                                        QStringLiteral("No assets could be imported."));
                                });
                        }
                    }
                }
            }
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<OpenProjectRequestedEvent>(
          [this](const OpenProjectRequestedEvent&) {
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Open Project"));
            if (!path.isEmpty()) {
                const QPointer<ArtifactMainWindow> windowGuard(this);
                ArtifactProjectManager::getInstance().loadFromFileAsync(
                    path,
                    [windowGuard, path](const ArtifactProjectImporterResult& result) {
                        if (!windowGuard || result.success) {
                            return;
                        }
                        const QString error = result.errorMessage.toQString();
                        QMessageBox::warning(
                            windowGuard, QStringLiteral("Open Project"),
                            error.isEmpty()
                                ? QStringLiteral("Failed to open project.\n%1").arg(path)
                                : QStringLiteral("Failed to open project.\n%1").arg(error));
                    });
            }
          }));
  // Keep recent-project data fresh without covering Composition Viewer.
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
              if (!impl_ || !impl_->welcomeWidget) return;
              const auto& mgr = ArtifactProjectManager::getInstance();
              const bool noProject = mgr.currentProjectPath().isEmpty()
                                     && !mgr.isProjectCreated();
              impl_->welcomeWidget->hide();
              if (noProject) {
                  impl_->welcomeWidget->refreshRecentProjects();
              }
          }));

  resize(2000,
         1200); // Increased initial window size to give central area more space

#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
  {
    // Command Palette: lazy-init a single application-scope widget the first
    // time the main window is constructed. It self-registers the dummy
    // actions on first boot. The hotkey is handled in keyPressEvent() below
    // (no new QObject signal/slot connection is introduced by this block).
    if (!sPalette_instance()) {
      auto *palette = new Artifact::ArtifactCommandPaletteWidget(qApp);
      palette->setMainWindow(this);
      Artifact::ArtifactCommandPaletteWidget::bootDummyCommandPaletteActions();
      setPalette_instance(palette);
    }
  }
#endif
}

ArtifactMainWindow::~ArtifactMainWindow() {
  if (qApp) {
    qApp->removeEventFilter(this);
  }
  delete impl_;
}

void ArtifactMainWindow::addWidget() {}

void ArtifactMainWindow::setCentralWorkspace(const QString &title,
                                             QWidget *widget) {
  if (!impl_ || !impl_->centralWidgetHost ||
      !impl_->centralWorkspaceLayout || !widget) {
    return;
  }
  if (impl_->centralWorkspaceWidget == widget) {
    return;
  }
  if (impl_->centralWorkspaceWidget) {
    impl_->centralWorkspaceLayout->removeWidget(impl_->centralWorkspaceWidget);
    impl_->centralWorkspaceWidget->hide();
  }
  impl_->centralWorkspaceTitle = title;
  impl_->centralWorkspaceWidget = widget;
  if (impl_->nativeDockSurface) {
    if (impl_->nativeDockSurface->addDockWidget(
        QStringLiteral("Composition Viewer"), title, widget,
        DockArea::Center)) {
      impl_->nativeDockWidgets.insert(QStringLiteral("Composition Viewer"),
                                      widget);
    }
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  else {
    widget->setParent(impl_->centralWidgetHost);
    impl_->centralWorkspaceLayout->addWidget(widget);
  }
#endif
  widget->show();
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  impl_->primaryCenterDockAssigned = true;
#endif
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
  if (impl_->welcomeWidget) {
    impl_->welcomeWidget->raise();
  }
}

void ArtifactMainWindow::applyUiFontSettings() {
  if (!impl_) {
    return;
  }
  if (impl_->menuBar) {
    impl_->menuBar->refreshFontFromSettings();
  }
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
}

void ArtifactMainWindow::applyApplicationSettings() {
  if (!impl_) {
    return;
  }
  applyUiFontSettings();

  // —— accessibility ——
  if (impl_->toolBar) {
    impl_->toolBar->refreshFromSettings();
    const int sz = Accessibility::scaledSize(24);
    impl_->toolBar->setIconSize(QSize(sz, sz));
  }

  // font scale
  const float fs = Accessibility::fontScale();
  if (qAbs(fs - 1.0f) > 0.01f) {
    QFont appFont = QApplication::font();
    appFont.setPointSizeF(appFont.pointSizeF() * fs);
    QApplication::setFont(appFont);
  }
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    setStatusPreviewResolution(settings->previewResolutionPercent());
    ArtifactAbstractLayer::setGlobalLayerCacheEnabled(settings->layerCacheEnabled());
  }
  if (impl_->toolOptionsHost) {
    impl_->toolOptionsHost->updateGeometry();
    impl_->toolOptionsHost->update();
  }
}

void ArtifactMainWindow::addDockedWidget(const QString &title,
                                         DockArea area,
                                         QWidget *widget) {
  if (!impl_ || !impl_->nativeDockSurface || !widget)
    return;
  if (!impl_->nativeDockSurface->addDockWidget(title, title, widget, area))
    return;
  impl_->nativeDockWidgets.insert(title, widget);
  if (title == "AI Cloud") {
    impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
  }
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::addDockedWidgetTabbed(const QString &title,
                                               DockArea area,
                                               QWidget *widget,
                                               const QString &tabGroupPrefix) {
  addDockedWidgetTabbedWithId(title, title, area, widget, tabGroupPrefix);
}

void ArtifactMainWindow::addDockedWidgetTabbedWithId(
    const QString &title, const QString &dockId, DockArea area,
    QWidget *widget, const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->nativeDockSurface || !widget)
    return;

  const QString nativeDockId = dockId.isEmpty() ? title : dockId;
  if (!impl_->nativeDockSurface->containsDock(nativeDockId)) {
    bool added = false;
    if (!tabGroupPrefix.isEmpty() &&
        impl_->nativeDockSurface->containsDockPrefix(tabGroupPrefix)) {
      const QString targetDockId =
          impl_->nativeDockSurface->dockIdWithPrefix(tabGroupPrefix);
      added = impl_->nativeDockSurface->addDockWidgetToTab(
          nativeDockId, title, widget, targetDockId);
    }
    if (!added) {
      added = impl_->nativeDockSurface->addDockWidget(
          nativeDockId, title, widget, area);
    }
    if (!added) {
      return;
    }
    impl_->nativeDockWidgets.insert(nativeDockId, widget);
    if (!impl_->startupLayoutFrozen) {
      applyWorkspaceMode(this, impl_->workspaceMode_);
    }
    return;
  }
  return;
}

void ArtifactMainWindow::addLazyDockedWidgetTabbedWithId(
    const QString &title, const QString &dockId, DockArea area,
    std::function<QWidget *()> factory, const QString &tabGroupPrefix) {
  if (!impl_ || !factory) {
    return;
  }
  if (impl_->nativeDockSurface) {
    if (auto *widget = factory()) {
      addDockedWidgetTabbedWithId(title, dockId, area, widget,
                                  tabGroupPrefix);
    }
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  const bool compositionScopedDock =
      dock->objectName().startsWith(QStringLiteral("timeline::")) ||
      dock->objectName().startsWith(QStringLiteral("dopesheet::"));
  dock->setProperty("artifactLazyDock", true);
  if (dock->objectName().startsWith(QStringLiteral("timeline::"))) {
    dock->setProperty("artifactLazyForceNoScrollArea", true);
  }
  auto *placeholder = createLazyDockPlaceholder(dock, title);
  dock->setWidget(placeholder);
  impl_->lazyDockFactories.insert(dock, std::move(factory));
  dock->setProperty("artifactLazyFactoryAvailable", true);

  ads::CDockAreaWidget *targetArea = nullptr;
  if (!tabGroupPrefix.isEmpty()) {
    for (auto it = impl_->dockWidgets.crbegin();
         it != impl_->dockWidgets.crend(); ++it) {
      auto *existingDock = *it;
      if (!existingDock)
        continue;
      const QString objectName = existingDock->objectName();
      const QString windowTitle = existingDock->windowTitle();
      if ((objectName == tabGroupPrefix || windowTitle == tabGroupPrefix) &&
          existingDock->dockAreaWidget()) {
        targetArea = existingDock->dockAreaWidget();
        break;
      }
    }
  }

  if (!targetArea && !tabGroupPrefix.isEmpty()) {
    for (auto it = impl_->dockWidgets.crbegin();
         it != impl_->dockWidgets.crend(); ++it) {
      auto *existingDock = *it;
      if (!existingDock)
        continue;
      const QString objectName = existingDock->objectName();
      const QString windowTitle = existingDock->windowTitle();
      if ((objectName.startsWith(tabGroupPrefix) ||
           windowTitle.startsWith(tabGroupPrefix)) &&
          existingDock->dockAreaWidget()) {
        targetArea = existingDock->dockAreaWidget();
        break;
      }
    }
  }

  if (targetArea) {
    impl_->dockBackend->addDockWidgetToArea(dock, targetArea);
  } else {
    impl_->dockBackend->addDockWidget(area, dock);
  }

  impl_->dockWidgets.push_back(dock);
  impl_->dockBackend->trackDock(dock, area, tabGroupPrefix);
  wireDockWidgetSignals(dock, this, impl_->dockBackend);
  dock->toggleView(false);
  impl_->dockBackend->prepareOverlays();

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, this,
      [this, dock, placeholder](bool visible) mutable {
        if (!visible || !impl_ ||
            dock->property("artifactLazyWidgetCreated").toBool() ||
            dock->property("artifactLazyWidgetCreationPending").toBool()) {
          return;
        }

        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
          return;
        }

        auto createNow = [this, dock, placeholder]() {
          Q_UNUSED(placeholder);
          if (impl_) {
            impl_->createLazyDockWidgetNow(
                this, dock, QStringLiteral("dock-visibility-request"));
          }
        };

        dock->setProperty("artifactLazyWidgetCreationPending", true);
        createNow();
  });

  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
  if (!impl_->startupLayoutFrozen && !compositionScopedDock) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
#else
  Q_UNUSED(title);
  Q_UNUSED(dockId);
  Q_UNUSED(area);
  Q_UNUSED(tabGroupPrefix);
#endif
}

void ArtifactMainWindow::addDockedWidgetFloating(
    const QString &title, const QString &dockId, QWidget *widget,
    const QRect &floatingGeometry) {
  if (!impl_ || !impl_->nativeDockSurface || !widget)
    return;

  const QString nativeDockId = dockId.isEmpty() ? title : dockId;
  if (!impl_->nativeDockSurface->addFloatingDockWidget(
          nativeDockId, title, widget, floatingGeometry,
          !impl_->startupLayoutFrozen)) {
    return;
  }
  impl_->nativeDockWidgets.insert(nativeDockId, widget);
  if (auto *aiWidget = qobject_cast<ArtifactAICloudWidget *>(widget)) {
    impl_->aiCloudWidget_ = aiWidget;
  }
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
 }

 void ArtifactMainWindow::addLazyDockedWidgetFloating(
    const QString &title, const QString &dockId,
    std::function<QWidget *()> factory, const QRect &floatingGeometry) {
  if (!impl_ || !factory) {
    return;
  }
  if (impl_->nativeDockSurface) {
    if (auto *widget = factory()) {
      addDockedWidgetFloating(title, dockId, widget, floatingGeometry);
    }
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setProperty("artifactLazyDock", true);
  dock->setProperty("artifactLazyFloatingDock", true);
  dock->setProperty("artifactDeferredFloatingContainer", true);
  dock->setProperty("artifactDeferredFloatingGeometry", floatingGeometry);
  auto *placeholder = createLazyDockPlaceholder(dock, title);
  dock->setWidget(placeholder, CDockWidget::ForceNoScrollArea);
  impl_->lazyDockFactories.insert(dock, std::move(factory));
  dock->setProperty("artifactLazyFactoryAvailable", true);

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, this,
      [this, dock, placeholder](bool visible) mutable {
        if (!visible || !impl_ ||
            dock->property("artifactLazyWidgetCreated").toBool() ||
            dock->property("artifactLazyWidgetCreationPending").toBool()) {
          return;
        }

        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
          return;
        }

        dock->setProperty("artifactLazyWidgetCreationPending", true);
        [this, dock, placeholder]() mutable {
          Q_UNUSED(placeholder);
          if (impl_) {
            impl_->createLazyDockWidgetNow(
                this, dock, QStringLiteral("floating-dock-visibility-request"));
          }
        }();
      });

  // Register a restorable dock shell without creating a native floating
  // window. The floating container is materialized on first explicit use, or
  // supplied by ADS when a saved layout restores this dock as floating.
  impl_->dockBackend->addDockWidget(DockArea::Right, dock);

  impl_->dockWidgets.push_back(dock);
  impl_->dockBackend->trackDock(dock, DockArea::Right, {}, true);
  wireDockWidgetSignals(dock, this, impl_->dockBackend);
  impl_->dockBackend->prepareOverlays();
  if (!impl_->startupLayoutFrozen) {
    dock->toggleView(true);
  } else {
    dock->toggleView(false);
  }
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
#else
  Q_UNUSED(title);
  Q_UNUSED(dockId);
  Q_UNUSED(floatingGeometry);
#endif
}

void ArtifactMainWindow::moveDockToTabGroup(const QString &title,
                                            const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->nativeDockSurface || title.isEmpty() ||
      tabGroupPrefix.isEmpty())
    return;

  if (!impl_->nativeDockSurface->containsDock(title) ||
      !impl_->nativeDockSurface->containsDock(tabGroupPrefix)) {
    return;
  }
  const QByteArray beforeState = saveDockManagerState();
  if (!impl_->nativeDockSurface->moveDockWidgetToTab(title, tabGroupPrefix)) {
    return;
  }
  pushDockLayoutSnapshot(this, beforeState,
                         QStringLiteral("Move Dock: %1").arg(title));
  return;


#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  CDockWidget *dockToMove = nullptr;
  ads::CDockAreaWidget *targetArea = nullptr;

  for (auto it = impl_->dockWidgets.crbegin(); it != impl_->dockWidgets.crend();
       ++it) {
    auto *existingDock = *it;
    if (!existingDock)
      continue;

    const QString objectName = existingDock->objectName();
    const QString windowTitle = existingDock->windowTitle();
    if (!dockToMove && (objectName == title || windowTitle == title)) {
      dockToMove = existingDock;
    }
    if (!targetArea &&
        (objectName == tabGroupPrefix || windowTitle == tabGroupPrefix) &&
        existingDock->dockAreaWidget()) {
      targetArea = existingDock->dockAreaWidget();
    }
  }

  if (!targetArea) {
    for (auto it = impl_->dockWidgets.crbegin();
         it != impl_->dockWidgets.crend(); ++it) {
      auto *existingDock = *it;
      if (!existingDock)
        continue;

      const QString objectName = existingDock->objectName();
      const QString windowTitle = existingDock->windowTitle();
      if ((objectName.startsWith(tabGroupPrefix) ||
           windowTitle.startsWith(tabGroupPrefix)) &&
          existingDock->dockAreaWidget()) {
        targetArea = existingDock->dockAreaWidget();
        break;
      }
    }
  }

  if (!dockToMove || !targetArea ||
      dockToMove->dockAreaWidget() == targetArea) {
    return;
  }

  impl_->dockBackend->addDockWidgetToTab(dockToMove, targetArea);
  dockToMove->toggleView(true);
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
  pushDockLayoutSnapshot(this, beforeState,
                         QStringLiteral("Move Dock: %1").arg(title));
#endif
}

void ArtifactMainWindow::setDockVisible(const QString &title,
                                        const bool visible) {
  if (!impl_)
    return;
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    impl_->nativeDockSurface->setDockVisible(title, visible);
    if (visible) {
      impl_->nativeDockSurface->activateDock(title);
    }
    return;
  }
  if (impl_->nativeDockSurface) {
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (title == impl_->centralWorkspaceTitle &&
      impl_->centralWorkspaceWidget) {
    // The central workspace is structural, not a toggleable tool panel.
    // Keeping it visible prevents workspace-mode changes from leaving the
    // main window with an empty QADS central area.
    if (!impl_->centralWorkspaceWidget->isVisible()) {
      impl_->centralWorkspaceWidget->show();
    }
    if (impl_->primaryCenterDock) {
      const bool centerDockVisible = impl_->primaryCenterDock->isVisible() &&
                                     !impl_->primaryCenterDock->isClosed();
      if (!centerDockVisible) {
        impl_->dockBackend->setVisible(impl_->primaryCenterDock, true);
      }
    }
    return;
  }

  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      if (impl_->startupLayoutFrozen) {
        dock->setProperty("artifactStartupVisibilityOverride", visible);
        return;
      }
      const bool isVisible = dock->isVisible() && !dock->isClosed();
      const bool visibilityChanged = isVisible != visible;
      const bool needsLazyWidget =
          visible && impl_->lazyDockFactories.contains(dock) &&
          !dock->property("artifactLazyWidgetCreated").toBool() &&
          !dock->property("artifactLazyWidgetCreationPending").toBool();
      if (!visibilityChanged && !needsLazyWidget) {
        return;
      }

      const bool recordMutation =
          visibilityChanged && impl_->recordLayoutMutations &&
          property("artifactWorkspaceVisibilityBatchDepth").toInt() == 0 &&
          property("artifactProgrammaticDockMutationDepth").toInt() == 0;
      const QByteArray beforeState =
          recordMutation ? saveDockManagerState() : QByteArray{};

      if (visible) {
        impl_->dockBackend->materializeDeferredFloatingDock(dock);
      }
      if (visibilityChanged) {
        impl_->dockBackend->setVisible(dock, visible);
      }

      if (needsLazyWidget) {
        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
        } else {
          impl_->createLazyDockWidgetNow(
              this, dock, QStringLiteral("set-dock-visible"));
        }
      }

      if (visible) {
        impl_->dockBackend->activate(dock);
        refreshDockWidgetSurface(dock);
        if (auto *floatingWidget = findFloatingDockContainer(dock)) {
          scheduleFloatingRefresh(floatingWidget);
        }
      }
      if (recordMutation) {
        pushDockLayoutSnapshot(
            this, beforeState,
            visible ? QStringLiteral("Show Dock: %1").arg(title)
                    : QStringLiteral("Hide Dock: %1").arg(title));
      }
      return;
    }
  }
#endif
}

void ArtifactMainWindow::setDockPinned(const QString &title, bool pinned) {
  if (!impl_ || title.isEmpty()) {
    return;
  }
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    impl_->nativeDockSurface->setDockPinned(title, pinned);
    return;
  }
  if (impl_->nativeDockSurface) {
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (!dock || (dock->objectName() != title && dock->windowTitle() != title)) {
      continue;
    }
    if (dock == impl_->primaryCenterDock) {
      return;
    }
    impl_->dockBackend->setPinned(dock, pinned);
    return;
  }
#endif
}

bool ArtifactMainWindow::isDockPinned(const QString &title) const {
  if (!impl_ || title.isEmpty()) {
    return false;
  }
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    return impl_->nativeDockSurface->dockPinned(title);
  }
  if (impl_->nativeDockSurface) {
    return false;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (dock && (dock->objectName() == title || dock->windowTitle() == title)) {
      return dock->property("artifactDockPinned").toBool();
    }
  }
#endif
  return false;
}

void ArtifactMainWindow::activateDock(const QString &title) {
  if (!impl_)
    return;
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    impl_->nativeDockSurface->activateDock(title);
    return;
  }
  if (impl_->nativeDockSurface) {
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (title == impl_->centralWorkspaceTitle &&
      impl_->centralWorkspaceWidget) {
    impl_->centralWorkspaceWidget->show();
    impl_->centralWorkspaceWidget->raise();
    impl_->centralWorkspaceWidget->setFocus(Qt::OtherFocusReason);
    return;
  }
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      if (impl_->startupLayoutFrozen) {
        dock->setProperty("artifactStartupVisibilityOverride", true);
        return;
      }
      const bool isVisible = dock->isVisible() && !dock->isClosed();
      if (!isVisible) {
        impl_->dockBackend->materializeDeferredFloatingDock(dock);
        impl_->dockBackend->setVisible(dock, true);
      }
      if (!dock->property("artifactLazyWidgetCreated").toBool() &&
          !dock->property("artifactLazyWidgetCreationPending").toBool()) {
        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
        } else if (impl_->lazyDockFactories.contains(dock)) {
          impl_->createLazyDockWidgetNow(
              this, dock, QStringLiteral("activate-dock"));
        }
      }
      impl_->dockBackend->activate(dock);
      if (impl_->dockStyleManager) {
        impl_->dockStyleManager->applyStyle();
      }
      return;
    }
  }
#endif
}

bool ArtifactMainWindow::closeDock(const QString &title) {
  if (!impl_ || title.isEmpty())
    return false;
  if (title == impl_->centralWorkspaceTitle) {
    return false;
  }
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    const QByteArray beforeState = saveDockManagerState();
    const bool closed = impl_->nativeDockSurface->setDockVisible(title, false);
    if (closed) {
      pushDockLayoutSnapshot(this, beforeState,
                             QStringLiteral("Close Dock: %1").arg(title));
    }
    return closed;
  }
  if (impl_->nativeDockSurface) {
    return false;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)

  const QByteArray beforeState = saveDockManagerState();
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      impl_->dockBackend->close(dock);
      if (impl_->dockStyleManager) {
        impl_->dockStyleManager->applyStyle();
      }
      pushDockLayoutSnapshot(this, beforeState,
                             QStringLiteral("Close Dock: %1").arg(title));
      return true;
    }
  }
  return false;
#endif
}

void ArtifactMainWindow::closeAllDocks() {
  if (!impl_)
    return;
  if (impl_->nativeDockSurface) {
    const QByteArray beforeState = saveDockManagerState();
    for (auto it = impl_->nativeDockWidgets.cbegin();
         it != impl_->nativeDockWidgets.cend(); ++it) {
      const auto &dockId = it.key();
      if (dockId != QStringLiteral("Composition Viewer")) {
        impl_->nativeDockSurface->setDockVisible(dockId, false);
      }
    }
    pushDockLayoutSnapshot(this, beforeState,
                           QStringLiteral("Close All Docks"));
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (dock && dock != impl_->primaryCenterDock)
      impl_->dockBackend->close(dock);
  }
#endif
}

void ArtifactMainWindow::setDockImmersive(QWidget *widget, bool immersive) {
  if (!impl_ || !widget) {
    return;
  }

  if (impl_->nativeDockSurface) {
    const QString targetId = impl_->nativeDockIdForWidget(widget);
    if (targetId.isEmpty()) {
      if (!immersive && impl_->immersiveMode_) {
        for (auto it = impl_->nativeImmersiveDockVisibility_.cbegin();
             it != impl_->nativeImmersiveDockVisibility_.cend(); ++it) {
          impl_->nativeDockSurface->setDockVisible(it.key(), it.value());
        }
        impl_->nativeImmersiveDockVisibility_.clear();
        impl_->nativeImmersiveTargetDock_.clear();
        impl_->immersiveMode_ = false;
        showNormal();
      }
      return;
    }
    if (!immersive) {
      if (impl_->immersiveMode_) {
        for (auto it = impl_->nativeImmersiveDockVisibility_.cbegin();
             it != impl_->nativeImmersiveDockVisibility_.cend(); ++it) {
          impl_->nativeDockSurface->setDockVisible(it.key(), it.value());
        }
        impl_->nativeImmersiveDockVisibility_.clear();
        impl_->nativeImmersiveTargetDock_.clear();
        impl_->immersiveMode_ = false;
        showNormal();
      }
      return;
    }
    if (impl_->immersiveMode_ &&
        impl_->nativeImmersiveTargetDock_ == targetId) {
      return;
    }
    if (impl_->immersiveMode_) {
      for (auto it = impl_->nativeImmersiveDockVisibility_.cbegin();
           it != impl_->nativeImmersiveDockVisibility_.cend(); ++it) {
        impl_->nativeDockSurface->setDockVisible(it.key(), it.value());
      }
      impl_->nativeImmersiveDockVisibility_.clear();
    }
    impl_->nativeImmersiveTargetDock_ = targetId;
    impl_->nativeImmersiveDockVisibility_.clear();
    for (auto it = impl_->nativeDockWidgets.cbegin();
         it != impl_->nativeDockWidgets.cend(); ++it) {
      const auto &dockId = it.key();
      const bool visible = impl_->nativeDockSurface->dockVisible(dockId);
      impl_->nativeImmersiveDockVisibility_.insert(dockId, visible);
      impl_->nativeDockSurface->setDockVisible(dockId, dockId == targetId);
    }
    impl_->nativeDockSurface->activateDock(targetId);
    impl_->immersiveMode_ = true;
    showFullScreen();
    return;
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  auto findDockForWidget = [this](QWidget *target) -> CDockWidget * {
    if (!impl_ || !target) {
      return nullptr;
    }
    for (auto *dock : impl_->dockWidgets) {
      if (!dock) {
        continue;
      }
      QWidget *dockWidget = dock->widget();
      if (dockWidget == target ||
          (dockWidget && dockWidget->isAncestorOf(target))) {
        return dock;
      }
    }
    return nullptr;
  };

  auto restoreVisibility = [this]() {
    if (!impl_) {
      return;
    }
    for (auto *dock : impl_->dockWidgets) {
      if (!dock) {
        continue;
      }
      if (impl_->immersiveDockVisibility_.contains(dock)) {
        dock->toggleView(impl_->immersiveDockVisibility_.value(dock));
      }
    }
    const Qt::WindowStates restoreState = impl_->immersivePreviousWindowState_;
    impl_->immersiveDockVisibility_.clear();
    impl_->immersiveMode_ = false;
    impl_->immersiveTargetDock_.clear();
    impl_->immersivePreviousWindowState_ = Qt::WindowNoState;
    if (restoreState.testFlag(Qt::WindowFullScreen)) {
      showFullScreen();
    } else if (restoreState.testFlag(Qt::WindowMaximized)) {
      showMaximized();
    } else {
      showNormal();
    }
  };

  auto *targetDock = findDockForWidget(widget);
  if (!targetDock) {
    if (!immersive && impl_->immersiveMode_) {
      restoreVisibility();
    }
    return;
  }

  if (immersive) {
    if (impl_->immersiveMode_ && impl_->immersiveTargetDock_ == targetDock) {
      return;
    }

    if (impl_->immersiveMode_) {
      restoreVisibility();
    }

    impl_->immersiveDockVisibility_.clear();
    for (auto *dock : impl_->dockWidgets) {
      if (!dock) {
        continue;
      }
      impl_->immersiveDockVisibility_.insert(dock, dock->isVisible());
    }
    impl_->immersiveMode_ = true;
    impl_->immersivePreviousWindowState_ = windowState();
    impl_->immersiveTargetDock_ = targetDock;
    for (auto *dock : impl_->dockWidgets) {
      if (!dock) {
        continue;
      }
      dock->toggleView(dock == targetDock);
    }
    targetDock->toggleView(true);
    targetDock->setAsCurrentTab();
    targetDock->raise();
    showFullScreen();
    return;
  }

  if (impl_->immersiveMode_) {
    restoreVisibility();
  }
#endif
}

void ArtifactMainWindow::showStatusMessage(const QString &message,
                                           int timeoutMs) {
  if (impl_ && impl_->statusBar) impl_->statusBar->showMessage(message, timeoutMs);
}

void ArtifactMainWindow::togglePanelsVisible(bool visible) {
  if (!impl_)
    return;
  qDebug() << "[MainWindow] togglePanelsVisible visible=" << visible;
  if (impl_->nativeDockSurface) {
    for (auto it = impl_->nativeDockWidgets.cbegin();
         it != impl_->nativeDockWidgets.cend(); ++it) {
      const auto &dockId = it.key();
      if (dockId != QStringLiteral("Composition Viewer")) {
        impl_->nativeDockSurface->setDockVisible(dockId, visible);
      }
    }
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (dock && dock != impl_->primaryCenterDock)
      dock->setVisible(visible);
  }
#endif
}

void ArtifactMainWindow::enterFocusMode() {
  if (!impl_ || impl_->focusMode_) {
    return;
  }

  impl_->focusMode_ = true;
  impl_->focusChromeVisibility_.clear();
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  impl_->focusDockVisibility_.clear();
#endif

  const auto hideChrome = [this](QWidget *widget) {
    if (!impl_ || !widget) {
      return;
    }
    impl_->focusChromeVisibility_.insert(widget, widget->isVisible());
    widget->hide();
  };
  hideChrome(impl_->menuBar);
  hideChrome(impl_->toolBar);
  hideChrome(impl_->toolOptionsHost);
  hideChrome(impl_->statusBar);

  if (impl_->nativeDockSurface) {
    impl_->nativeFocusDockVisibility_.clear();
    for (auto it = impl_->nativeDockWidgets.cbegin();
         it != impl_->nativeDockWidgets.cend(); ++it) {
      const auto &dockId = it.key();
      const bool isVisible = impl_->nativeDockSurface->dockVisible(dockId);
      impl_->nativeFocusDockVisibility_.insert(dockId, isVisible);
      if (dockId != QStringLiteral("Composition Viewer")) {
        impl_->nativeDockSurface->setDockVisible(dockId, false);
      }
    }
    if (impl_->centralWorkspaceWidget) {
      impl_->centralWorkspaceWidget->show();
    }
    showStatusMessage(QStringLiteral("Focus mode enabled — Ctrl+Shift+F to restore"),
                      2500);
    return;
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (!dock || dock == impl_->primaryCenterDock) {
      continue;
    }
    impl_->focusDockVisibility_.insert(
        dock, dock->isVisible() && !dock->isClosed());
    dock->toggleView(false);
  }

  if (impl_->primaryCenterDock) {
    impl_->primaryCenterDock->toggleView(true);
  }
#endif
  if (impl_->centralWorkspaceWidget) {
    impl_->centralWorkspaceWidget->show();
  }
  showStatusMessage(QStringLiteral("Focus mode enabled — Ctrl+Shift+F to restore"),
                    2500);
}

void ArtifactMainWindow::exitFocusMode() {
  if (!impl_ || !impl_->focusMode_) {
    return;
  }

  impl_->focusMode_ = false;
  if (impl_->nativeDockSurface) {
    for (auto it = impl_->nativeFocusDockVisibility_.cbegin();
         it != impl_->nativeFocusDockVisibility_.cend(); ++it) {
      impl_->nativeDockSurface->setDockVisible(it.key(), it.value());
    }
    impl_->nativeFocusDockVisibility_.clear();
    for (auto it = impl_->focusChromeVisibility_.cbegin();
         it != impl_->focusChromeVisibility_.cend(); ++it) {
      if (it.key()) {
        it.key()->setVisible(it.value());
      }
    }
    impl_->focusChromeVisibility_.clear();
    showStatusMessage(QStringLiteral("Focus mode disabled"), 1500);
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto it = impl_->focusDockVisibility_.cbegin();
       it != impl_->focusDockVisibility_.cend(); ++it) {
    if (it.key()) {
      it.key()->toggleView(it.value());
    }
  }
  for (auto it = impl_->focusChromeVisibility_.cbegin();
       it != impl_->focusChromeVisibility_.cend(); ++it) {
    if (it.key()) {
      it.key()->setVisible(it.value());
    }
  }
#endif
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  impl_->focusDockVisibility_.clear();
#endif
  impl_->focusChromeVisibility_.clear();
  showStatusMessage(QStringLiteral("Focus mode disabled"), 1500);
}

void ArtifactMainWindow::toggleFocusMode() {
  if (!impl_) {
    return;
  }
  if (impl_->focusMode_) {
    exitFocusMode();
  } else {
    enterFocusMode();
  }
}

void ArtifactMainWindow::setWorkspaceMode(WorkspaceMode mode) {
  if (!impl_) {
    return;
  }

  const bool workspaceModeChanged = impl_->workspaceMode_ != mode;
  const bool shouldApplyWorkspace =
      workspaceModeChanged || impl_->startupLayoutFrozen ||
      impl_->startupLayoutApplying;
  qDebug() << "[MainWindow] setWorkspaceMode mode=" << static_cast<int>(mode)
           << "apply=" << shouldApplyWorkspace;
  impl_->workspaceMode_ = mode;
  if (shouldApplyWorkspace) {
    applyWorkspaceMode(this, mode);
  }
  if (impl_->toolBar && impl_->toolBar->workspaceMode() != mode) {
    impl_->toolBar->setWorkspaceMode(mode);
  }
  if (workspaceModeChanged) {
    if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
      QString modeText = QStringLiteral("Default");
      switch (mode) {
      case WorkspaceMode::Default:
        modeText = QStringLiteral("Default");
        break;
      case WorkspaceMode::Import:
        modeText = QStringLiteral("Import");
        break;
      case WorkspaceMode::Layout:
        modeText = QStringLiteral("Layout");
        break;
      case WorkspaceMode::Animation:
        modeText = QStringLiteral("Animation");
        break;
      case WorkspaceMode::VFX:
        modeText = QStringLiteral("VFX");
        break;
      case WorkspaceMode::Compositing:
        modeText = QStringLiteral("Compositing");
        break;
      case WorkspaceMode::Text:
        modeText = QStringLiteral("Text");
        break;
      case WorkspaceMode::Export:
        modeText = QStringLiteral("Export");
        break;
      case WorkspaceMode::Debug:
        modeText = QStringLiteral("Debug");
        break;
      case WorkspaceMode::Audio:
        modeText = QStringLiteral("Audio");
        break;
      }
      settings->setProjectDefaultWorkspaceModeText(modeText);
    }
  }
}

WorkspaceMode ArtifactMainWindow::workspaceMode() const {
  return impl_ ? impl_->workspaceMode_ : WorkspaceMode::Default;
}

QStringList ArtifactMainWindow::dockTitles() const {
  QStringList titles;
  for (const QString &dockId : dockIds()) {
    const QString title = dockDisplayTitle(dockId);
    if (!title.isEmpty()) {
      titles.append(title);
    }
  }
  titles.removeDuplicates();
  return titles;
}

QStringList ArtifactMainWindow::dockIds() const {
  QStringList ids;
  if (!impl_)
    return ids;
  if (impl_->nativeDockSurface) {
    for (auto it = impl_->nativeDockWidgets.cbegin();
         it != impl_->nativeDockWidgets.cend(); ++it) {
      if (!it.key().isEmpty()) {
        ids.append(it.key());
      }
    }
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  else {
    for (auto *dock : impl_->dockWidgets) {
      if (!dock)
        continue;
      const QString dockId = dock->objectName().trimmed();
      const QString title = dock->windowTitle().trimmed();
      if (!dockId.isEmpty()) {
        ids.append(dockId);
      } else if (!title.isEmpty()) {
        ids.append(title);
      }
    }
  }
#endif
  ids.removeDuplicates();
  return ids;
}

QString ArtifactMainWindow::dockDisplayTitle(const QString &dockId) const {
  if (!impl_ || dockId.isEmpty())
    return {};
  if (impl_->nativeDockSurface &&
      impl_->nativeDockWidgets.contains(dockId)) {
    const QString title = impl_->nativeDockSurface->dockTitle(dockId);
    return title.isEmpty() ? dockId : title;
  }
  if (impl_->nativeDockSurface)
    return {};
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (dock && dock->objectName() == dockId) {
      const QString title = dock->windowTitle();
      return title.isEmpty() ? dockId : title;
    }
  }
#endif
  return {};
}

QString ArtifactMainWindow::resolveDockId(const QString &dockIdOrTitle) const {
  if (dockIdOrTitle.isEmpty())
    return {};
  const QStringList ids = dockIds();
  if (ids.contains(dockIdOrTitle))
    return dockIdOrTitle;
  QString matchedId;
  for (const QString &dockId : ids) {
    if (dockDisplayTitle(dockId) != dockIdOrTitle)
      continue;
    if (!matchedId.isEmpty())
      return {};
    matchedId = dockId;
  }
  return matchedId;
}

bool ArtifactMainWindow::isDockVisible(const QString &title) const {
  if (!impl_)
    return false;
  if (title == impl_->centralWorkspaceTitle &&
      impl_->centralWorkspaceWidget) {
    return impl_->centralWorkspaceWidget->isVisible();
  }
  if (impl_->nativeDockSurface) {
    const QString resolvedId = impl_->nativeDockSurface->resolveDockId(title);
    if (!resolvedId.isEmpty()) {
      const auto widget = impl_->nativeDockWidgets.value(resolvedId);
      return widget && impl_->nativeDockSurface->dockVisible(resolvedId);
    }
    return false;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    const QString name = dock->objectName();
    const QString dockTitle = dock->windowTitle();
    if (name == title || dockTitle == title) {
      return dock->isVisible();
    }
  }
#endif
  return false;
}

bool ArtifactMainWindow::hasDock(const QString &title) const {
  if (!impl_)
    return false;
  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(title)) {
    return true;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    const QString name = dock->objectName();
    const QString dockTitle = dock->windowTitle();
    if (name == title || dockTitle == title) {
      return true;
    }
  }
#endif
  return false;
}

void ArtifactMainWindow::setStatusZoomLevel(float zoomPercent) {
  if (!impl_) return;
  const float normalized = std::isfinite(zoomPercent)
      ? std::clamp(zoomPercent, 0.0f, 100000.0f)
      : 100.0f;
  impl_->statusZoomLevel = normalized;
  impl_->hasStatusZoomLevel = true;
  if (!impl_->statusBar) return;
  if (auto *label = impl_->ensureStatusLabel(
          impl_->statusZoomLabel, QStringLiteral("ZoomStatusLabel"),
          QStringLiteral("Viewport zoom"))) {
    label->setText(QStringLiteral("Zoom: %1%").arg(
        QString::number(normalized, 'f', normalized < 100.0f ? 1 : 0)));
  }
}

void ArtifactMainWindow::setStatusCoordinates(int x, int y) {
  if (!impl_) return;
  impl_->statusCoordinateX = x;
  impl_->statusCoordinateY = y;
  impl_->hasStatusCoordinates = true;
  if (!impl_->statusBar) return;
  if (auto *label = impl_->ensureStatusLabel(
          impl_->statusCoordinatesLabel, QStringLiteral("CoordinatesStatusLabel"),
          QStringLiteral("Viewport coordinates"))) {
    label->setText(QStringLiteral("X: %1  Y: %2").arg(x).arg(y));
  }
}

void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB) {
  if (!impl_) return;
  impl_->statusMemoryUsageMB = memoryMB;
  impl_->hasStatusMemoryUsage = true;
  if (!impl_->statusBar) return;
  if (auto *label = impl_->ensureStatusLabel(
          impl_->statusMemoryLabel, QStringLiteral("MemoryStatusLabel"),
          QStringLiteral("Memory usage"))) {
    label->setText(QStringLiteral("Memory: %1 MB").arg(memoryMB));
  }
}

void ArtifactMainWindow::setStatusFPS(double fps) {
  if (!impl_) return;
  const double normalized = std::isfinite(fps) ? std::max(0.0, fps) : 0.0;
  impl_->statusFPS = normalized;
  impl_->hasStatusFPS = true;
  if (!impl_->statusBar) return;
  if (auto *label = impl_->ensureStatusLabel(
          impl_->statusFpsLabel, QStringLiteral("FpsStatusLabel"),
          QStringLiteral("Frames per second"))) {
    label->setText(QStringLiteral("FPS: %1")
                       .arg(QString::number(normalized, 'f', 1)));
  }
}

void ArtifactMainWindow::setStatusPreviewResolution(int percent) {
  if (!impl_) {
    return;
  }
  auto *status = impl_->statusBar;
  if (!status) return;
  if (!impl_->previewResolutionLabel || impl_->previewResolutionLabel->parent() != status) {
    impl_->previewResolutionLabel = new QLabel(status);
    impl_->previewResolutionLabel->setObjectName(
        QStringLiteral("PreviewResolutionStatusLabel"));
    status->addPermanentWidget(impl_->previewResolutionLabel);
  }
  const int normalized = std::clamp(percent, 1, 100);
  impl_->previewResolutionLabel->setText(
      QStringLiteral("Preview: %1%").arg(normalized));
}

void ArtifactMainWindow::setStatusReady() {
  if (!impl_ || !impl_->statusBar) return;
  impl_->statusBar->showMessage(QStringLiteral("Ready"), 1500);
}

void ArtifactMainWindow::setStatusBar(QStatusBar *statusBar) {
  if (!impl_ || !impl_->rootLayout || !statusBar ||
      impl_->statusBar == statusBar) {
    return;
  }
  if (impl_->statusBar) {
    impl_->rootLayout->removeWidget(impl_->statusBar);
    impl_->statusBar->deleteLater();
  }
  statusBar->setParent(this);
  impl_->statusBar = statusBar;
  impl_->rootLayout->addWidget(statusBar);
  if (impl_->hasStatusZoomLevel) {
    setStatusZoomLevel(impl_->statusZoomLevel);
  }
  if (impl_->hasStatusCoordinates) {
    setStatusCoordinates(impl_->statusCoordinateX, impl_->statusCoordinateY);
  }
  if (impl_->hasStatusMemoryUsage) {
    setStatusMemoryUsage(impl_->statusMemoryUsageMB);
  }
  if (impl_->hasStatusFPS) {
    setStatusFPS(impl_->statusFPS);
  }
}

void ArtifactMainWindow::setDockSplitterSizes(const QString &dockTitle,
                                              const QList<int> &sizes) {
  if (!impl_)
    return;

  if (impl_->nativeDockSurface &&
      impl_->nativeDockSurface->containsDock(dockTitle)) {
    const bool recordMutation =
        impl_->recordLayoutMutations &&
        property("artifactWorkspaceVisibilityBatchDepth").toInt() == 0 &&
        property("artifactProgrammaticDockMutationDepth").toInt() == 0;
    const QByteArray beforeState =
        recordMutation ? saveDockManagerState() : QByteArray{};
    const bool changed = impl_->nativeDockSurface->setSplitterSizes(
        impl_->nativeDockSurface->dockArea(dockTitle), sizes);
    if (changed && recordMutation) {
      pushDockLayoutSnapshot(this, beforeState,
                             QStringLiteral("Resize Dock Splitter: %1")
                                 .arg(dockTitle));
    }
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (!impl_->dockBackend->manager())
    return;

  const bool recordMutation =
      impl_->recordLayoutMutations &&
      property("artifactWorkspaceVisibilityBatchDepth").toInt() == 0 &&
      property("artifactProgrammaticDockMutationDepth").toInt() == 0;
  const QByteArray beforeState =
      recordMutation ? saveDockManagerState() : QByteArray{};
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == dockTitle || dock->windowTitle() == dockTitle) {
      if (auto *area = dock->dockAreaWidget()) {
        impl_->dockBackend->setSplitterSizes(area, sizes);
        if (recordMutation) {
          pushDockLayoutSnapshot(
              this, beforeState,
              QStringLiteral("Resize Dock Splitter: %1").arg(dockTitle));
        }
      }
      return;
    }
  }
#else
  Q_UNUSED(dockTitle);
  Q_UNUSED(sizes);
#endif
}

QByteArray ArtifactMainWindow::saveDockManagerState() const {
  if (!impl_)
    return {};
  if (impl_->nativeDockSurface) {
    return impl_->nativeDockSurface->saveLayoutState();
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (!impl_->dockBackend->manager())
    return {};
  return impl_->dockBackend ? impl_->dockBackend->saveState() : QByteArray{};
#else
  return {};
#endif
}

QByteArray ArtifactMainWindow::savePortableDockLayoutState() const {
  if (!impl_ || !impl_->nativeDockSurface) {
    return {};
  }
  return impl_->nativeDockSurface->saveLayoutState();
}

bool ArtifactMainWindow::restorePortableDockLayoutState(const QByteArray &state) {
  if (!impl_ || !impl_->nativeDockSurface) {
    return false;
  }
  return impl_->nativeDockSurface->restoreLayoutState(state);
}

bool ArtifactMainWindow::restoreDockManagerState(const QByteArray &state) {
  if (impl_ && impl_->nativeDockSurface) {
    return impl_->nativeDockSurface->restoreLayoutState(state);
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (!impl_ || !impl_->dockBackend || !impl_->dockBackend->manager() ||
      state.isEmpty())
    return false;
  // ADS は「全ての dock が DockManager に登録された後」でないと restore できない。
  // 呼び出し側（AppMain）がレイアウト構築完了後に呼ぶことを前提とする。
  const bool restored = impl_->dockBackend && impl_->dockBackend->restoreState(state);
  if (!restored) {
    return false;
  }

  for (auto *dock : impl_->dockWidgets) {
    if (!dock ||
        !dock->property("artifactDeferredFloatingContainer").toBool()) {
      continue;
    }
    if (findFloatingDockContainer(dock)) {
      dock->setProperty("artifactDeferredFloatingMaterialized", true);
    } else {
      // A saved docked placement is authoritative. Do not force the default
      // floating geometry when the user opens this surface later.
      dock->setProperty("artifactRespectRestoredDockPlacement", true);
    }
  }
  return true;
#else
  Q_UNUSED(state);
  return false;
#endif
}

void ArtifactMainWindow::captureDefaultDockManagerState() {
  if (!impl_) {
    return;
  }
  if (impl_->nativeDockSurface) {
    impl_->defaultDockManagerState =
        impl_->nativeDockSurface->saveLayoutState();
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (!impl_->dockBackend->manager()) {
    return;
  }
  impl_->defaultDockManagerState = impl_->dockBackend
                                       ? impl_->dockBackend->saveState()
                                       : QByteArray{};
#endif
}

bool ArtifactMainWindow::resetDockManagerStateToDefault() {
  if (!impl_ ||
      impl_->defaultDockManagerState.isEmpty()) {
    return false;
  }
  const QByteArray beforeState = saveDockManagerState();
  if (impl_->nativeDockSurface) {
    if (!impl_->nativeDockSurface->restoreLayoutState(
            impl_->defaultDockManagerState)) {
      return false;
    }
    pushDockLayoutSnapshot(this, beforeState,
                           QStringLiteral("Reset Dock Layout"));
    setWorkspaceMode(WorkspaceMode::Default);
    return true;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  if (!impl_->dockBackend->manager()) {
    return false;
  }
  if (!impl_->dockBackend ||
      !impl_->dockBackend->restoreState(impl_->defaultDockManagerState)) {
    return false;
  }
  pushDockLayoutSnapshot(this, beforeState,
                         QStringLiteral("Reset Dock Layout"));
  for (auto *dock : impl_->dockWidgets) {
    if (!dock || !dock->property("artifactDeferredFloatingContainer").toBool()) {
      continue;
    }
    dock->setProperty("artifactDeferredFloatingMaterialized",
                      findFloatingDockContainer(dock) != nullptr);
  }
  setWorkspaceMode(WorkspaceMode::Default);
  return true;
#else
  return false;
#endif
}

void ArtifactMainWindow::setStartupLayoutFrozen(bool frozen) {
  if (!impl_ || impl_->startupLayoutFrozen == frozen) {
    return;
  }

  if (frozen) {
    impl_->startupLayoutFrozen = true;
    impl_->startupLayoutApplying = false;
    setUpdatesEnabled(false);
    return;
  }

  // Apply the restored workspace as one non-recording transaction.  Startup
  // visibility changes are not user edits and must not serialize the full ADS
  // state or populate layout undo history for every dock.
  impl_->startupLayoutFrozen = false;
  impl_->startupLayoutApplying = true;
  impl_->recordLayoutMutations = false;
  impl_->startupRefreshScheduled = true;
  applyWorkspaceMode(this, impl_->workspaceMode_);

  if (impl_->nativeDockSurface) {
    impl_->startupLayoutApplying = false;
    impl_->recordLayoutMutations = true;
    impl_->startupRefreshScheduled = false;
    setUpdatesEnabled(true);
    return;
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  for (auto *dock : impl_->dockWidgets) {
    if (!dock ||
        !dock->property("artifactStartupVisibilityOverride").isValid()) {
      continue;
    }
    const bool visible =
        dock->property("artifactStartupVisibilityOverride").toBool();
    dock->setProperty("artifactStartupVisibilityOverride", QVariant());
    dock->toggleView(visible);
    if (visible) {
      dock->setAsCurrentTab();
    }
  }
  impl_->startupLayoutApplying = false;
  impl_->recordLayoutMutations = true;
  setUpdatesEnabled(true);

  for (auto *dock : impl_->dockWidgets) {
    if (!dock) {
      continue;
    }
    // visibilityChanged may fire transiently while QADS restores its graph.
    // Discard that historical hint and decide solely from the final layout.
    dock->setProperty("artifactLazyWidgetStartupPending", false);
    bool shouldCreateLazyWidget = false;
    if (!dock->property("artifactLazyWidgetCreated").toBool()) {
      if (dock->property("artifactLazyFloatingDock").toBool()) {
        shouldCreateLazyWidget = dock->isVisible() && !dock->isClosed();
      } else if (auto *area = dock->dockAreaWidget()) {
        shouldCreateLazyWidget =
            area->currentDockWidget() == dock && !dock->isClosed();
      } else {
        shouldCreateLazyWidget = dock->isVisible() && !dock->isClosed();
      }
    }
    if (!dock->property("artifactLazyWidgetCreated").toBool() &&
        shouldCreateLazyWidget) {
      if (!impl_->lazyDockFactories.contains(dock)) {
        dock->setProperty("artifactLazyWidgetCreationPending", false);
        continue;
      }
      impl_->createLazyDockWidgetNow(
          this, dock, QStringLiteral("startup-layout-final-visible-tab"));
    }
  }

  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || !impl_->dockBackend->manager()) {
      if (impl_) {
        impl_->startupRefreshScheduled = false;
      }
      return;
    }
    for (auto *dock : impl_->dockWidgets) {
      if (!dock || dock->property("artifactLazyWidgetCreated").toBool() ||
          dock->property("artifactLazyWidgetCreationPending").toBool() ||
          !impl_->lazyDockFactories.contains(dock)) {
        continue;
      }
      bool shouldCreateLazyWidget = false;
      if (dock->property("artifactLazyFloatingDock").toBool()) {
        shouldCreateLazyWidget = dock->isVisible() && !dock->isClosed();
      } else if (auto *area = dock->dockAreaWidget()) {
        shouldCreateLazyWidget =
            area->currentDockWidget() == dock && !dock->isClosed();
      } else {
        shouldCreateLazyWidget = dock->isVisible() && !dock->isClosed();
      }
      if (!shouldCreateLazyWidget) {
        continue;
      }
      impl_->createLazyDockWidgetNow(
          this, dock, QStringLiteral("startup-layout-deferred-visible-tab"));
    }
    applyDarkNativeTitleBar(this);
    for (auto *dock : impl_->dockWidgets) {
      if (!dock || dock->isClosed()) {
        continue;
      }
      const bool isVisibleSurface =
          dock->property("artifactLazyFloatingDock").toBool()
              ? dock->isVisible()
              : (dock->dockAreaWidget() &&
                 dock->dockAreaWidget()->currentDockWidget() == dock);
      if (isVisibleSurface) {
        refreshDockWidgetSurface(dock);
      }
    }
    const auto floatingWidgets = impl_->dockBackend->floatingWidgets();
    for (auto *floatingWidget : floatingWidgets) {
      if (floatingWidget && floatingWidget->isVisible() &&
          !floatingWidget->isMinimized()) {
        prepareFloatingDockContainer(floatingWidget, this);
      }
    }
    if (!impl_->initialLayoutApplied) {
      impl_->initialLayoutApplied = true;
      for (auto *dock : impl_->dockWidgets) {
        if (!dock)
          continue;
        if (dock->windowTitle() == QStringLiteral("Project") ||
            dock->objectName() == QStringLiteral("Project")) {
          if (auto *area = dock->dockAreaWidget()) {
            const int totalW = this->width();
            const int sideW = qBound(240, totalW / 7, 360);
            const int centerW = qMax(400, totalW - 2 * sideW);
            impl_->dockBackend->setSplitterSizes(area,
                                                 {sideW, centerW, sideW});
          }
          break;
        }
      }
    }
    if (impl_) {
      impl_->startupRefreshScheduled = false;
    }
    update();
  });
#endif
}

void ArtifactMainWindow::keyPressEvent(QKeyEvent *event) {
  if (event && event->modifiers() == Qt::ShiftModifier &&
      event->key() == Qt::Key_Space) {
    QWidget *focusedWidget = QApplication::focusWidget();
    if (focusedWidget) {
      // setDockImmersive() resolves child widgets to their owning dock and
      // preserves the previous visibility/window state for restoration.
      setDockImmersive(focusedWidget, !(impl_ && impl_->immersiveMode_));
      event->accept();
      return;
    }
  }
  if (event && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      event->key() == Qt::Key_F) {
    toggleFocusMode();
    event->accept();
    return;
  }
  if (event && event->modifiers() == Qt::ControlModifier) {
    WorkspaceMode quickMode;
    bool handled = true;
    switch (event->key()) {
    case Qt::Key_1:
      quickMode = WorkspaceMode::Default;
      break;
    case Qt::Key_2:
      quickMode = WorkspaceMode::Animation;
      break;
    case Qt::Key_3:
      quickMode = WorkspaceMode::Compositing;
      break;
    case Qt::Key_4:
      quickMode = WorkspaceMode::Audio;
      break;
    default:
      handled = false;
      break;
    }
    if (handled) {
      setWorkspaceMode(quickMode);
      event->accept();
      return;
    }
  }
#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
  if (event && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
      event->key() == Qt::Key_P) {
    if (auto *palette = sPalette_instance()) {
      if (palette->isVisible()) {
        palette->hide();
      } else {
        palette->setProperty("hostWindow",
                             QVariant::fromValue<QWidget *>(this));
        // Re-collect actions on every show to keep the list current.
        if (auto *cp = qobject_cast<Artifact::ArtifactCommandPaletteWidget *>(
                palette)) {
          cp->setMainWindow(this);
          cp->refreshActionList();
        }
        palette->show();
        palette->raise();
        palette->activateWindow();
      }
      event->accept();
      return;
    }
  }
#endif
  QWidget::keyPressEvent(event);
}

void ArtifactMainWindow::keyReleaseEvent(QKeyEvent *event) {
  QWidget::keyReleaseEvent(event);
}

void ArtifactMainWindow::closeEvent(QCloseEvent *event) {
  const bool unsavedGuardSatisfied =
      property(kUnsavedCloseGuardSatisfiedProperty).toBool();
  setProperty(kUnsavedCloseGuardSatisfiedProperty, false);
  if (!unsavedGuardSatisfied && !confirmUnsavedChangesForClose(this)) {
    event->ignore();
    return;
  }
  if (ArtifactMessageBox::confirmAction(
          this, QStringLiteral("終了"),
          QStringLiteral("Artifact を終了しますか？"))) {
    event->accept();
    // QADS floating containers are independent top-level windows. Relying on
    // QApplication::lastWindowClosed can therefore leave the event loop alive
    // after the main editor disappears. Closing the main editor is an explicit
    // application-exit request, so terminate the event loop regardless of any
    // auxiliary/floating window that is still registered.
    QApplication::quit();
  } else {
    event->ignore();
  }
}

void ArtifactMainWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (!property("artifactStartupScreenshotScheduled").toBool() &&
      startupScreenshotEnabled()) {
    setProperty("artifactStartupScreenshotScheduled", true);
    const int delayMs = std::clamp(
        QSettings().value(QString::fromLatin1(kStartupScreenshotDelayKey), 1500)
            .toInt(),
        250, 10000);
    QPointer<ArtifactMainWindow> guardedWindow(this);
    QTimer::singleShot(delayMs, this, [guardedWindow]() {
      if (guardedWindow) {
        captureStartupScreenshot(guardedWindow);
      }
    });
  }
  if (impl_ && (impl_->startupLayoutFrozen || impl_->startupRefreshScheduled)) {
    return;
  }
#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || !impl_->dockBackend->manager()) {
      return;
    }
    applyDarkNativeTitleBar(this);
    refreshFloatingWidgetTree(this);
    for (auto *dock : impl_->dockWidgets) {
      wireDockWidgetSignals(dock, this, impl_->dockBackend);
      refreshDockWidgetSurface(dock);
    }
    const auto floatingWidgets = impl_->dockBackend->floatingWidgets();
    for (auto *floatingWidget : floatingWidgets) {
      prepareFloatingDockContainer(floatingWidget, this);
    }
    // 初回表示時に左右サイドパネルの幅を整える
    if (!impl_->initialLayoutApplied) {
      impl_->initialLayoutApplied = true;
      for (auto *dock : impl_->dockWidgets) {
        if (!dock)
          continue;
        if (dock->windowTitle() == QStringLiteral("Project") ||
            dock->objectName() == QStringLiteral("Project")) {
          if (auto *area = dock->dockAreaWidget()) {
            const int totalW = this->width();
            const int sideW = qBound(240, totalW / 7, 360);
            const int centerW = qMax(400, totalW - 2 * sideW);
            impl_->dockBackend->setSplitterSizes(area, {sideW, centerW, sideW});
          }
          break;
        }
      }
    }
  });
#endif
}

bool ArtifactMainWindow::eventFilter(QObject *watched, QEvent *event) {
  // 高速パス: フローティングドックに無関係なイベント型は
  // qobject_cast / findFloatingDockContainer の高コスト処理を
  // スキップして即座に返す。
  // ChildAdded / ChildRemoved / LayoutRequest / Polish / PolishRequest は
  // レイアウト処理中に大量発生するためトリガーから除外する。
  if (event) {
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::ActivationChange:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::WindowStateChange:
    case QEvent::ZOrderChange:
    case QEvent::KeyPress:
      break;
    default:
      return QWidget::eventFilter(watched, event);
    }
  }

  if (event && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier) &&
        keyEvent->key() == Qt::Key_F) {
      toggleFocusMode();
      keyEvent->accept();
      return true;
    }
  }

  if (impl_ && impl_->nativeDockSurface) {
    return QWidget::eventFilter(watched, event);
  }

#if defined(ARTIFACT_QADS_COMPAT_BACKEND)
  ads::CFloatingDockContainer *floatingWidget =
      qobject_cast<ads::CFloatingDockContainer *>(watched);
  if (!floatingWidget) {
    if (auto *watchedWidget = qobject_cast<QWidget *>(watched)) {
      floatingWidget = findFloatingDockContainer(watchedWidget);
    }
  }

  if (floatingWidget) {
    const bool isRootFloatingWidget = (watched == floatingWidget);
    switch (event ? event->type() : QEvent::None) {
    case QEvent::Resize:
      // ルートコンテナのリサイズのみ再描画をスケジュールする。
      // 子ウィジェットのリサイズにも反応すると refreshFloatingWidgetTree 内の
      // layout->invalidate()/activate()
      // が子リサイズを発火させ、イベントフィルタに 戻り、再び refresh
      // をスケジュールする無限カスケードが発生していた。
      if (isRootFloatingWidget) {
        refreshFloatingWidgetTree(floatingWidget);
      }
      break;
    case QEvent::Show:
    case QEvent::Hide:
      scheduleFloatingRefresh(floatingWidget);
      break;
    case QEvent::ActivationChange:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::WindowStateChange:
    case QEvent::ZOrderChange:
      if (isRootFloatingWidget) {
        scheduleFloatingRefresh(floatingWidget);
      }
      break;
    default:
      break;
    }
  }

  // Keep welcome widget sized to central host
  if (watched == impl_->centralWidgetHost && event && event->type() == QEvent::Resize) {
      if (impl_->welcomeWidget) {
          impl_->welcomeWidget->setGeometry(static_cast<QWidget*>(watched)->rect());
      }
  }
#endif

  return QWidget::eventFilter(watched, event);
}

ArtifactAICloudWidget *ArtifactMainWindow::aiCloudWidget() const {
  return impl_->aiCloudWidget_;
}

#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
QWidget *ArtifactMainWindow::paletteInstance_ = nullptr;

QWidget *ArtifactMainWindow::sPalette_instance() { return paletteInstance_; }

void ArtifactMainWindow::setPalette_instance(QWidget *palette) {
  paletteInstance_ = palette;
}
#endif

} // namespace Artifact
