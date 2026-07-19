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
#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockOverlay.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>
#include <FloatingDockContainer.h>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QLayout>
#include <QList>
#include <cmath>
#include <QMessageBox>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QShowEvent>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QFileDialog>
#include <wobjectimpl.h>

module Artifact.MainWindow;

import Artifact.Application.Manager;
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
import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Artifact.Workspace.Modes;
import Application.AppSettings;
import Settings.Accessibility;
import Undo.UndoManager;
#ifdef ARTIFACT_FEATURE_COMMAND_PALETTE
import Command.Palette;
#endif

namespace Artifact {

using namespace ads;

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
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties"},


       {"Audio Mixer", "Contents Viewer", "AI Chat", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"}},
      {WorkspaceMode::Import,
       {"Project", "Asset Browser", "Inspector", "Properties"},
       {"Audio Mixer", "Contents Viewer", "AI Chat", "Composition Viewer"}},
      {WorkspaceMode::Layout,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties"},
       {"Audio Mixer", "Contents Viewer", "AI Chat"}},
      {WorkspaceMode::Animation,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control"}},
      {WorkspaceMode::VFX,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties", "Composition View (Software)",
        "Layer Solo View", "Layer View (Software)"},
       {"Audio Mixer", "Contents Viewer", "AI Chat", "Playback Control"}},
      {WorkspaceMode::Compositing,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties", "Layer Solo View"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control", "Composition View (Software)",
        "Layer View (Software)"}},
      {WorkspaceMode::Text,
       {"Composition Viewer", "Project", "Asset Browser", "Inspector",
        "Properties", "Contents Viewer"},
       {"Audio Mixer", "AI Cloud", "AI Chat", "Playback Control"}},
      {WorkspaceMode::Export,
       {"Project", "Asset Browser", "Inspector", "Properties",
        "Composition Viewer"},
       {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
        "Playback Control"}},
      {WorkspaceMode::Debug,
       {"Project", "Asset Browser", "Inspector", "Properties",
        "Contents Viewer", "AI Chat", "Playback Control"},
       {"Audio Mixer"}},
      {WorkspaceMode::Audio,
       {"Contents Viewer", "Audio Mixer", "Project", "Asset Browser",
        "Inspector", "Properties"},
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

QWidget *createLazyDockPlaceholder(QWidget *parent) {
  auto *placeholder = new QWidget(parent);
  placeholder->setObjectName(QStringLiteral("ArtifactLazyDockPlaceholder"));
  placeholder->setAutoFillBackground(true);
  QPalette palette = placeholder->palette();
  palette.setColor(QPalette::Window, QColor(32, 34, 38));
  placeholder->setPalette(palette);
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

void applyWorkspaceVisibility(ArtifactMainWindow *window, WorkspaceMode mode) {
  if (!window) {
    return;
  }

  const QStringList dockTitles = window->dockTitles();
  const auto setVisible = [window](const QString &title, bool visible) {
    if (!title.isEmpty()) {
      window->setDockVisible(title, visible);
    }
  };
  const WorkspaceVisibilityRule *rule = workspaceVisibilityRuleFor(mode);

  for (const QString &title : dockTitles) {
    setVisible(title, false);
  }

  for (const QString &title : rule->visibleTitles) {
    setVisible(title, true);
  }
  for (const QString &title : rule->hiddenTitles) {
    setVisible(title, false);
  }

  for (const QString &title : dockTitles) {
    if (isTimelineDockTitle(title)) {
      setVisible(title, mode == WorkspaceMode::Default ||
                            mode == WorkspaceMode::Animation);
    }
  }
}

void applyWorkspaceMode(ArtifactMainWindow *window, WorkspaceMode mode) {
  if (!window) {
    return;
  }
  applyWorkspaceVisibility(window, mode);
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
    mgr->push(std::make_unique<LayoutSnapshotCommand>(
        label, beforeState, afterState,
        [windowGuard](const QByteArray &state) -> bool {
          return windowGuard ? windowGuard->restoreDockManagerState(state)
                             : false;
        }));
  }
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer *floatingWidget,
                                  QObject *eventFilterOwner);

void materializeDeferredFloatingDock(ads::CDockManager *dockManager,
                                     ads::CDockWidget *dock) {
  if (!dockManager || !dock ||
      !dock->property("artifactDeferredFloatingContainer").toBool() ||
      dock->property("artifactDeferredFloatingMaterialized").toBool() ||
      dock->property("artifactRespectRestoredDockPlacement").toBool()) {
    return;
  }

  if (findFloatingDockContainer(dock)) {
    dock->setProperty("artifactDeferredFloatingMaterialized", true);
    return;
  }

  auto *container = dockManager->addDockWidgetFloating(dock);
  if (!container) {
    return;
  }

  const QRect geometry =
      dock->property("artifactDeferredFloatingGeometry").toRect();
  if (geometry.isValid()) {
    container->setGeometry(geometry);
  }
  dock->setProperty("artifactDeferredFloatingMaterialized", true);
}

void wireDockWidgetSignals(ads::CDockWidget *dock, QObject *owner) {
  if (!dock || !owner ||
      dock->property("artifactFloatingHooksInstalled").toBool()) {
    return;
  }

  dock->setProperty("artifactFloatingHooksInstalled", true);

  QObject::connect(
      dock, &ads::CDockWidget::topLevelChanged, owner, [dock, owner](bool) {
        refreshDockWidgetSurface(dock);
        if (auto *floatingWidget = findFloatingDockContainer(dock)) {
          prepareFloatingDockContainer(floatingWidget, owner);
        }
      });

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, owner, [dock, owner](bool) {
        refreshDockWidgetSurface(dock);
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
} // namespace

W_OBJECT_IMPL(ArtifactMainWindow)

class ArtifactMainWindow::Impl {
public:
  CDockManager *dockManager = nullptr;
  DockStyleManager *dockStyleManager = nullptr;
  ArtifactToolBar *toolBar = nullptr;
  ArtifactToolOptionsBar *toolOptionsBar = nullptr;
  QToolBar *toolOptionsHost = nullptr;
  QToolButton *workspaceButton = nullptr;
  QVBoxLayout *rootLayout = nullptr;
  ArtifactMenuBar *menuBar = nullptr;
  QStatusBar *statusBar = nullptr;
  QWidget *centralWidgetHost = nullptr;
  CDockWidget *primaryCenterDock = nullptr;
  bool primaryCenterDockAssigned = false;
  QVBoxLayout *centralWorkspaceLayout = nullptr;
  QWidget *centralWorkspaceWidget = nullptr;
  QString centralWorkspaceTitle;
  QList<CDockWidget *> dockWidgets;
  WorkspaceMode workspaceMode_ = WorkspaceMode::Default;
  bool immersiveMode_ = false;
  Qt::WindowStates immersivePreviousWindowState_ = Qt::WindowNoState;
  QHash<CDockWidget *, bool> immersiveDockVisibility_;
  QPointer<CDockWidget> immersiveTargetDock_;
  ArtifactWelcomeWidget* welcomeWidget = nullptr;
  bool menuBarInitialized = false;
  bool initialLayoutApplied = false;
  bool startupRefreshScheduled = false;
  bool startupLayoutFrozen = true;
  bool startupLayoutApplying = false;
  bool recordLayoutMutations = true;
  ArtifactAICloudWidget *aiCloudWidget_ = nullptr;
  QLabel *previewResolutionLabel = nullptr;
  QHash<CDockWidget *, std::function<QWidget *()>> lazyDockFactories;
  QMetaObject::Connection currentTextLayerChangedConnection;
  QMetaObject::Connection currentShapeLayerChangedConnection;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  bool createLazyDockWidgetNow(ArtifactMainWindow *owner, CDockWidget *dock) {
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
      return false;
    }

    dock->setProperty("artifactLazyWidgetCreationPending", true);
    auto factory = lazyDockFactories.take(dock);
    QWidget *widget = factory ? factory() : nullptr;
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
    return true;
  }

  void syncTextToolOptions(ArtifactMainWindow *owner) {
    if (!owner || !toolOptionsBar) {
      return;
    }

    if (currentTextLayerChangedConnection) {
      QObject::disconnect(currentTextLayerChangedConnection);
    }

    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(current);
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

    currentTextLayerChangedConnection = QObject::connect(
        current.get(), &ArtifactAbstractLayer::changed, owner, [owner]() {
          if (owner && owner->impl_) {
            owner->impl_->syncTextToolOptions(owner);
          }
        });
  }

  void syncShapeToolOptions(ArtifactMainWindow *owner) {
    if (!owner || !toolOptionsBar) {
      return;
    }

    if (currentShapeLayerChangedConnection) {
      QObject::disconnect(currentShapeLayerChangedConnection);
    }

    auto *app = ArtifactApplicationManager::instance();
    auto *selection = app ? app->layerSelectionManager() : nullptr;
    const auto current =
        selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    const auto shapeLayer =
        std::dynamic_pointer_cast<ArtifactShapeLayer>(current);
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

    currentShapeLayerChangedConnection = QObject::connect(
        current.get(), &ArtifactAbstractLayer::changed, owner, [owner]() {
          if (owner && owner->impl_) {
            owner->impl_->syncShapeToolOptions(owner);
          }
        });
  }
};

ArtifactMainWindow::ArtifactMainWindow(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setUpdatesEnabled(false);
  impl_->rootLayout = new QVBoxLayout(this);
  impl_->rootLayout->setContentsMargins(0, 0, 0, 0);
  impl_->rootLayout->setSpacing(0);
  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  // CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden,
  // true);
  CDockManager::setConfigFlag(CDockManager::FocusHighlighting, false);
  CDockManager::setConfigFlag(CDockManager::TabCloseButtonIsToolButton, true);
  CDockManager::setConfigFlag(CDockManager::AllTabsHaveCloseButton, true);
  CDockManager::setConfigFlag(CDockManager::AlwaysShowTabs, true);
  CDockManager::setConfigFlag(CDockManager::EqualSplitOnInsertion, true);
  CDockManager::setConfigFlag(CDockManager::FloatingContainerHasWidgetTitle,
                              true);
  CDockManager::setConfigFlag(CDockManager::FloatingContainerHasWidgetIcon,
                              true);
  CDockManager::setAutoHideConfigFlags(CDockManager::DefaultAutoHideConfig);
  CDockManager::setAutoHideConfigFlag(CDockManager::AutoHideButtonCheckable,
                                      true);

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
  });

  auto *toolBar = new ArtifactToolBar(this);
  impl_->rootLayout->addWidget(toolBar);
  impl_->toolBar = toolBar;

  auto *workspaceButton = new QToolButton(this);
  impl_->workspaceButton = workspaceButton;
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
  QObject::connect(toolBar, &ArtifactToolBar::workspaceModeChanged, this,
                   [this](WorkspaceMode mode) {
                     if (impl_) {
                       impl_->workspaceMode_ = mode;
                       if (impl_->workspaceButton) {
                         impl_->workspaceButton->setText(Artifact::workspaceModeText(mode));
                       }
                     }
                   });

  // Tool signal routing
  QObject::connect(
      toolBar, &ArtifactToolBar::moveToolRequested, this,
      [this]() { qDebug() << "[MainWindow] Move tool selected (W)"; });
  QObject::connect(
      toolBar, &ArtifactToolBar::rotationToolRequested, this,
      [this]() { qDebug() << "[MainWindow] Rotate tool selected (E)"; });
  QObject::connect(
      toolBar, &ArtifactToolBar::scaleToolRequested, this,
      [this]() { qDebug() << "[MainWindow] Scale tool selected (R)"; });

  QObject::connect(
      impl_->toolOptionsBar, &ArtifactToolOptionsBar::optionChanged, this,
      [this](const QString &toolName, const QString &optionName,
             const QVariant &value) {
        auto *app = ArtifactApplicationManager::instance();
        auto *selection = app ? app->layerSelectionManager() : nullptr;
        const auto current =
            selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};

        if (toolName == QStringLiteral("テキスト")) {
          const auto textLayer =
              std::dynamic_pointer_cast<ArtifactTextLayer>(current);
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
            std::dynamic_pointer_cast<ArtifactShapeLayer>(current);
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
      });

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerSelectionChangedEvent>([this](const LayerSelectionChangedEvent&) {
        if (impl_) {
          impl_->syncTextToolOptions(this);
          impl_->syncShapeToolOptions(this);
        }
      }));
  impl_->syncTextToolOptions(this);
  impl_->syncShapeToolOptions(this);

  impl_->dockManager = new CDockManager(this);
  impl_->rootLayout->addWidget(impl_->dockManager, 1);
  prepareDockDropOverlays(impl_->dockManager);
  QTimer::singleShot(0, this, [this]() {
    if (impl_ && impl_->dockManager) {
      prepareDockDropOverlays(impl_->dockManager);
    }
  });
  impl_->dockStyleManager = new DockStyleManager(impl_->dockManager, this);
  if (qApp) {
    qApp->installEventFilter(this);
  }
  QObject::connect(impl_->dockManager, &CDockManager::floatingWidgetCreated,
                   this, [this](ads::CFloatingDockContainer *floatingWidget) {
                     prepareFloatingDockContainer(floatingWidget, this);
                     if (impl_ && impl_->dockManager) {
                       prepareDockDropOverlays(impl_->dockManager);
                     }
                   });
  impl_->dockStyleManager->setGlowEnabled(true);
  impl_->dockStyleManager->setGlowColor(
      QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->dockStyleManager->setGlowWidth(2);
  impl_->dockStyleManager->setGlowIntensity(0.72f);
  // Dock styling now comes from the global theme and DockStyleManager.
  impl_->centralWidgetHost = new QWidget(this);
  impl_->centralWidgetHost->setObjectName(QStringLiteral("ArtifactCentralWidgetHost"));
  impl_->centralWidgetHost->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
  impl_->centralWorkspaceLayout = new QVBoxLayout(impl_->centralWidgetHost);
  impl_->centralWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
  impl_->centralWorkspaceLayout->setSpacing(0);
  auto *centralDock = new CDockWidget(QStringLiteral("Workspace"), this);
  centralDock->setObjectName(QStringLiteral("ArtifactCentralDock"));
  centralDock->setWidget(impl_->centralWidgetHost);
  centralDock->setFeatures(ads::CDockWidget::NoDockWidgetFeatures);
  impl_->dockManager->setCentralWidget(centralDock);
  impl_->primaryCenterDock = centralDock;
  impl_->dockStyleManager->applyStyle();

  impl_->statusBar = new QStatusBar(this);
  impl_->rootLayout->addWidget(impl_->statusBar);

  // Let Composition Viewer own the startup empty state so its
  // Create Composition action stays actionable.
  impl_->welcomeWidget = new Artifact::ArtifactWelcomeWidget(impl_->centralWidgetHost);
  impl_->welcomeWidget->setGeometry(impl_->centralWidgetHost->rect());
  impl_->welcomeWidget->hide();
  // Sync welcome widget size with central host when it resizes
  impl_->centralWidgetHost->installEventFilter(this);
  QObject::connect(impl_->welcomeWidget, &ArtifactWelcomeWidget::openRecentProject, this,
      [this](const QString& path) {
          if (!path.isEmpty()) {
              ArtifactProjectManager::getInstance().loadFromFile(path);
          }
      });
  QObject::connect(impl_->welcomeWidget, &ArtifactWelcomeWidget::createNewComposition, this,
      [this]() {
          auto* svc = ArtifactProjectService::instance();
          if (!svc) return;
          if (!svc->hasProject()) {
              ArtifactProjectManager::getInstance().createProject();
          }
          svc->createComposition(ArtifactCompositionInitParams::hdPreset());
      });
  QObject::connect(impl_->welcomeWidget, &ArtifactWelcomeWidget::importAsset, this,
      [this]() {
          auto* svc = ArtifactProjectService::instance();
          if (!svc || !svc->hasProject()) {
              ArtifactProjectManager::getInstance().createProject();
              svc = ArtifactProjectService::instance();
          }
          if (svc) {
              const QStringList files = QFileDialog::getOpenFileNames(
                  this, QStringLiteral("Import Assets"));
              if (!files.isEmpty()) {
                  ArtifactImportAssetsDialog dialog(files, this);
                  if (dialog.exec() == QDialog::Accepted) {
                      const QStringList filtered = dialog.selectedPaths();
                      if (!filtered.isEmpty()) {
                          svc->importAssetsFromPaths(filtered);
                      }
                  }
              }
          }
      });
  QObject::connect(impl_->welcomeWidget, &ArtifactWelcomeWidget::openProject, this,
      [this]() {
          const QString path = QFileDialog::getOpenFileName(
              this, QStringLiteral("Open Project"));
          if (!path.isEmpty()) {
              ArtifactProjectManager::getInstance().loadFromFile(path);
          }
      });
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
  if (impl_->primaryCenterDock) {
    impl_->primaryCenterDock->setWindowTitle(title);
    if (!impl_->dockWidgets.contains(impl_->primaryCenterDock)) {
      impl_->dockWidgets.push_back(impl_->primaryCenterDock);
    }
    wireDockWidgetSignals(impl_->primaryCenterDock, this);
  }
  widget->setParent(impl_->centralWidgetHost);
  impl_->centralWorkspaceLayout->addWidget(widget);
  widget->show();
  impl_->primaryCenterDockAssigned = true;
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
  prepareDockDropOverlays(impl_->dockManager);
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
                                         ads::DockWidgetArea area,
                                         QWidget *widget) {
  if (!impl_ || !impl_->dockManager || !widget)
    return;
  if (area == ads::CenterDockWidgetArea && impl_->primaryCenterDock &&
      !impl_->primaryCenterDockAssigned) {
    setCentralWorkspace(title, widget);
    if (title == "AI Cloud") {
      impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
    }
    if (!impl_->startupLayoutFrozen) {
      applyWorkspaceMode(this, impl_->workspaceMode_);
    }
    return;
  }
  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(title);
  dock->setWidget(widget);
  impl_->dockManager->addDockWidget(area, dock);
  impl_->dockWidgets.push_back(dock);
  wireDockWidgetSignals(dock, this);
  prepareDockDropOverlays(impl_->dockManager);
  impl_->dockStyleManager->applyStyle();
  if (title == "AI Cloud") {
    impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
  }
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::addDockedWidgetTabbed(const QString &title,
                                               ads::DockWidgetArea area,
                                               QWidget *widget,
                                               const QString &tabGroupPrefix) {
  addDockedWidgetTabbedWithId(title, title, area, widget, tabGroupPrefix);
}

void ArtifactMainWindow::addDockedWidgetTabbedWithId(
    const QString &title, const QString &dockId, ads::DockWidgetArea area,
    QWidget *widget, const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->dockManager || !widget)
    return;

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setWidget(widget);
  if (auto *aiWidget = qobject_cast<ArtifactAICloudWidget *>(widget)) {
    impl_->aiCloudWidget_ = aiWidget;
  }

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
    impl_->dockManager->addDockWidget(ads::CenterDockWidgetArea, dock,
                                      targetArea);
  } else {
    impl_->dockManager->addDockWidget(area, dock);
  }

  impl_->dockWidgets.push_back(dock);
  if (!impl_->startupLayoutFrozen) {
    dock->toggleView(true);
    dock->setAsCurrentTab();
    dock->raise();
  }
  wireDockWidgetSignals(dock, this);
  prepareDockDropOverlays(impl_->dockManager);
  impl_->dockStyleManager->applyStyle();
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::addLazyDockedWidgetTabbedWithId(
    const QString &title, const QString &dockId, ads::DockWidgetArea area,
    std::function<QWidget *()> factory, const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->dockManager || !factory) {
    return;
  }

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setProperty("artifactLazyDock", true);
  if (dock->objectName().startsWith(QStringLiteral("timeline::"))) {
    dock->setProperty("artifactLazyForceNoScrollArea", true);
  }
  auto *placeholder = createLazyDockPlaceholder(dock);
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
    impl_->dockManager->addDockWidget(ads::CenterDockWidgetArea, dock,
                                      targetArea);
  } else {
    impl_->dockManager->addDockWidget(area, dock);
  }

  impl_->dockWidgets.push_back(dock);
  wireDockWidgetSignals(dock, this);
  dock->toggleView(false);
  prepareDockDropOverlays(impl_->dockManager);

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
            impl_->createLazyDockWidgetNow(this, dock);
          }
        };

        dock->setProperty("artifactLazyWidgetCreationPending", true);
        createNow();
  });

  impl_->dockStyleManager->applyStyle();
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::addDockedWidgetFloating(
    const QString &title, const QString &dockId, QWidget *widget,
    const QRect &floatingGeometry) {
  if (!impl_ || !impl_->dockManager || !widget)
    return;

  qInfo() << "[MainWindow][Floating] addDockedWidgetFloating"
          << "title=" << title << "dockId=" << dockId
          << "widget=" << widget
          << "geometry=" << floatingGeometry;

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setWidget(widget, CDockWidget::ForceNoScrollArea);
  if (auto *aiWidget = qobject_cast<ArtifactAICloudWidget *>(widget)) {
    impl_->aiCloudWidget_ = aiWidget;
  }

  auto *container = impl_->dockManager->addDockWidgetFloating(dock);
  if (container) {
    qInfo() << "[MainWindow][Floating] dock container created"
            << "dock=" << dock << "container=" << container
            << "visible=" << container->isVisible()
            << "geometry=" << container->geometry();
    container->setGeometry(floatingGeometry);
  }

  impl_->dockWidgets.push_back(dock);
  if (!impl_->startupLayoutFrozen) {
    dock->toggleView(true);
  } else {
    dock->toggleView(false);
  }
  wireDockWidgetSignals(dock, this);
  prepareDockDropOverlays(impl_->dockManager);
  impl_->dockStyleManager->applyStyle();
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::addLazyDockedWidgetFloating(
    const QString &title, const QString &dockId,
    std::function<QWidget *()> factory, const QRect &floatingGeometry) {
  if (!impl_ || !impl_->dockManager || !factory) {
    return;
  }

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setProperty("artifactLazyDock", true);
  dock->setProperty("artifactLazyFloatingDock", true);
  dock->setProperty("artifactDeferredFloatingContainer", true);
  dock->setProperty("artifactDeferredFloatingGeometry", floatingGeometry);
  auto *placeholder = createLazyDockPlaceholder(dock);
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
            impl_->createLazyDockWidgetNow(this, dock);
          }
        }();
      });

  // Register a restorable dock shell without creating a native floating
  // window. The floating container is materialized on first explicit use, or
  // supplied by ADS when a saved layout restores this dock as floating.
  impl_->dockManager->addDockWidget(ads::RightDockWidgetArea, dock);

  impl_->dockWidgets.push_back(dock);
  wireDockWidgetSignals(dock, this);
  prepareDockDropOverlays(impl_->dockManager);
  if (!impl_->startupLayoutFrozen) {
    dock->toggleView(true);
  } else {
    dock->toggleView(false);
  }
  impl_->dockStyleManager->applyStyle();
  if (!impl_->startupLayoutFrozen) {
    applyWorkspaceMode(this, impl_->workspaceMode_);
  }
}

void ArtifactMainWindow::moveDockToTabGroup(const QString &title,
                                            const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->dockManager || title.isEmpty() ||
      tabGroupPrefix.isEmpty())
    return;

  const QByteArray beforeState = saveDockManagerState();

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

  impl_->dockManager->addDockWidgetTabToArea(dockToMove, targetArea);
  dockToMove->toggleView(true);
  impl_->dockStyleManager->applyStyle();
  pushDockLayoutSnapshot(this, beforeState,
                         QStringLiteral("Move Dock: %1").arg(title));
}

void ArtifactMainWindow::setDockVisible(const QString &title,
                                        const bool visible) {
  if (!impl_)
    return;
  if (title == impl_->centralWorkspaceTitle &&
      impl_->centralWorkspaceWidget) {
    // The central workspace is structural, not a toggleable tool panel.
    // Keeping it visible prevents workspace-mode changes from leaving the
    // main window with an empty QADS central area.
    impl_->centralWorkspaceWidget->show();
    if (impl_->primaryCenterDock) {
      impl_->primaryCenterDock->toggleView(true);
    }
    return;
  }

  const QByteArray beforeState =
      impl_->recordLayoutMutations ? saveDockManagerState() : QByteArray{};

  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      if (impl_->startupLayoutFrozen) {
        dock->setProperty("artifactStartupVisibilityOverride", visible);
        return;
      }
      if (visible) {
        materializeDeferredFloatingDock(impl_->dockManager, dock);
      }
      const bool isVisible = dock->isVisible() && !dock->isClosed();
      if (isVisible != visible) {
        dock->toggleView(visible);
      }

      const bool needsLazyWidget =
          visible && impl_->lazyDockFactories.contains(dock) &&
          !dock->property("artifactLazyWidgetCreated").toBool() &&
          !dock->property("artifactLazyWidgetCreationPending").toBool();
      if (needsLazyWidget) {
        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
        } else {
          impl_->createLazyDockWidgetNow(this, dock);
        }
      }

      if (visible) {
        dock->setAsCurrentTab();
        dock->raise();
        refreshDockWidgetSurface(dock);
        if (auto *floatingWidget = findFloatingDockContainer(dock)) {
          scheduleFloatingRefresh(floatingWidget);
        }
      }
      if (impl_->recordLayoutMutations) {
        pushDockLayoutSnapshot(
            this, beforeState,
            visible ? QStringLiteral("Show Dock: %1").arg(title)
                    : QStringLiteral("Hide Dock: %1").arg(title));
      }
      return;
    }
  }
}

void ArtifactMainWindow::activateDock(const QString &title) {
  if (!impl_)
    return;
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
      materializeDeferredFloatingDock(impl_->dockManager, dock);
      dock->toggleView(true);
      if (!dock->property("artifactLazyWidgetCreated").toBool() &&
          !dock->property("artifactLazyWidgetCreationPending").toBool()) {
        if (impl_->startupLayoutFrozen || impl_->startupLayoutApplying) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
        } else if (impl_->lazyDockFactories.contains(dock)) {
          impl_->createLazyDockWidgetNow(this, dock);
        }
      }
      dock->setAsCurrentTab();
      dock->raise();
      impl_->dockStyleManager->applyStyle();
      return;
    }
  }
}

bool ArtifactMainWindow::closeDock(const QString &title) {
  if (!impl_ || title.isEmpty())
    return false;
  if (title == impl_->centralWorkspaceTitle) {
    return false;
  }

  const QByteArray beforeState = saveDockManagerState();
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      dock->closeDockWidget();
      impl_->dockStyleManager->applyStyle();
      pushDockLayoutSnapshot(this, beforeState,
                             QStringLiteral("Close Dock: %1").arg(title));
      return true;
    }
  }
  return false;
}

void ArtifactMainWindow::closeAllDocks() {
  if (!impl_)
    return;
  for (auto *dock : impl_->dockWidgets) {
    if (dock && dock != impl_->primaryCenterDock)
      dock->closeDockWidget();
  }
}

void ArtifactMainWindow::setDockImmersive(QWidget *widget, bool immersive) {
  if (!impl_ || !widget) {
    return;
  }

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
}

void ArtifactMainWindow::showStatusMessage(const QString &message,
                                           int timeoutMs) {
  if (impl_ && impl_->statusBar) impl_->statusBar->showMessage(message, timeoutMs);
}

void ArtifactMainWindow::togglePanelsVisible(bool visible) {
  if (!impl_)
    return;
  qDebug() << "[MainWindow] togglePanelsVisible visible=" << visible;
  for (auto *dock : impl_->dockWidgets) {
    if (dock && dock != impl_->primaryCenterDock)
      dock->setVisible(visible);
  }
}

void ArtifactMainWindow::setWorkspaceMode(WorkspaceMode mode) {
  if (!impl_) {
    return;
  }

  qDebug() << "[MainWindow] setWorkspaceMode mode="
           << static_cast<int>(mode);
  impl_->workspaceMode_ = mode;
  applyWorkspaceMode(this, mode);
  if (impl_->toolBar && impl_->toolBar->workspaceMode() != mode) {
    impl_->toolBar->setWorkspaceMode(mode);
  }
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

WorkspaceMode ArtifactMainWindow::workspaceMode() const {
  return impl_ ? impl_->workspaceMode_ : WorkspaceMode::Default;
}

QStringList ArtifactMainWindow::dockTitles() const {
  QStringList titles;
  if (!impl_)
    return titles;
  for (auto *dock : impl_->dockWidgets) {
    if (dock) {
      const QString name = dock->objectName();
      const QString title = dock->windowTitle();
      titles.append(title.isEmpty() ? name : title);
    }
  }
  return titles;
}

bool ArtifactMainWindow::isDockVisible(const QString &title) const {
  if (!impl_)
    return false;
  if (title == impl_->centralWorkspaceTitle &&
      impl_->centralWorkspaceWidget) {
    return impl_->centralWorkspaceWidget->isVisible();
  }
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    const QString name = dock->objectName();
    const QString dockTitle = dock->windowTitle();
    if (name == title || dockTitle == title) {
      return dock->isVisible();
    }
  }
  return false;
}

bool ArtifactMainWindow::hasDock(const QString &title) const {
  if (!impl_)
    return false;
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    const QString name = dock->objectName();
    const QString dockTitle = dock->windowTitle();
    if (name == title || dockTitle == title) {
      return true;
    }
  }
  return false;
}

void ArtifactMainWindow::setStatusZoomLevel(float zoomPercent) {
  if (!impl_ || !impl_->statusBar) return;
  impl_->statusBar->showMessage(
      QStringLiteral("Zoom: %1%").arg(static_cast<int>(zoomPercent)), 1000);
}

void ArtifactMainWindow::setStatusCoordinates(int x, int y) {
  if (!impl_ || !impl_->statusBar) return;
  impl_->statusBar->showMessage(QStringLiteral("X: %1 Y: %2").arg(x).arg(y), 1000);
}

void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB) {
  if (!impl_ || !impl_->statusBar) return;
  impl_->statusBar->showMessage(QStringLiteral("Memory: %1 MB").arg(memoryMB), 1000);
}

void ArtifactMainWindow::setStatusFPS(double fps) {
  if (!impl_ || !impl_->statusBar) return;
  impl_->statusBar->showMessage(
      QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', 1)), 1000);
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
}

void ArtifactMainWindow::setDockSplitterSizes(const QString &dockTitle,
                                              const QList<int> &sizes) {
  if (!impl_ || !impl_->dockManager)
    return;

  const QByteArray beforeState = saveDockManagerState();
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == dockTitle || dock->windowTitle() == dockTitle) {
      if (auto *area = dock->dockAreaWidget()) {
        impl_->dockManager->setSplitterSizes(area, sizes);
        pushDockLayoutSnapshot(
            this, beforeState,
            QStringLiteral("Resize Dock Splitter: %1").arg(dockTitle));
      }
      return;
    }
  }
}

QByteArray ArtifactMainWindow::saveDockManagerState() const {
  if (!impl_ || !impl_->dockManager)
    return {};
  return impl_->dockManager->saveState();
}

bool ArtifactMainWindow::restoreDockManagerState(const QByteArray &state) {
  if (!impl_ || !impl_->dockManager || state.isEmpty())
    return false;
  // ADS は「全ての dock が DockManager に登録された後」でないと restore できない。
  // 呼び出し側（AppMain）がレイアウト構築完了後に呼ぶことを前提とする。
  const bool restored = impl_->dockManager->restoreState(state);
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
      impl_->createLazyDockWidgetNow(this, dock);
    }
  }

  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || !impl_->dockManager) {
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
      impl_->createLazyDockWidgetNow(this, dock);
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
    const auto floatingWidgets = impl_->dockManager->floatingWidgets();
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
            impl_->dockManager->setSplitterSizes(area,
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
}

void ArtifactMainWindow::keyPressEvent(QKeyEvent *event) {
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
  if (ArtifactMessageBox::confirmAction(
          this, QStringLiteral("終了"),
          QStringLiteral("Artifact を終了しますか？"))) {
    event->accept();
  } else {
    event->ignore();
  }
}

void ArtifactMainWindow::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (impl_ && (impl_->startupLayoutFrozen || impl_->startupRefreshScheduled)) {
    return;
  }
  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || !impl_->dockManager) {
      return;
    }
    applyDarkNativeTitleBar(this);
    refreshFloatingWidgetTree(this);
    for (auto *dock : impl_->dockWidgets) {
      wireDockWidgetSignals(dock, this);
      refreshDockWidgetSurface(dock);
    }
    const auto floatingWidgets = impl_->dockManager->floatingWidgets();
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
            impl_->dockManager->setSplitterSizes(area, {sideW, centerW, sideW});
          }
          break;
        }
      }
    }
  });
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
      break;
    default:
      return QWidget::eventFilter(watched, event);
    }
  }

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
