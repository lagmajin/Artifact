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
#include <QKeyEvent>
#include <QLayout>
#include <QList>
#include <cmath>
#include <QMessageBox>
#include <QPointer>
#include <QShowEvent>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.MainWindow;

import Artifact.MainWindow;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Event.Bus;
import Text.Style;
import Utils.String.UniString;
import Widgets.ToolOptionsBar;
import Artifact.Widgets.ProjectManagerWidget;
import Menu.MenuBar;
import Artifact.Menu.View;
import Widgets.ToolBar;
import Widgets.Dock.StyleManager;
import Widgets.Utils.CSS;
import Artifact.Widgets.AppDialogs;
import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Application.AppSettings;

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

  QStringList visibleTitles;
  QStringList hiddenTitles;

  switch (mode) {
  case WorkspaceMode::Default:
    visibleTitles = {"Composition Viewer", "Project", "Asset Browser",
                     "Inspector", "Properties"};
    hiddenTitles = {"Audio Mixer", "Contents Viewer", "AI Chat",
                    "Composition Note", "Layer Note",
                    "Composition View (Software)", "Layer View (Diligent)",
                    "Layer View (Software)"};
    break;
  case WorkspaceMode::Animation:
    visibleTitles = {"Composition Viewer", "Project", "Asset Browser",
                     "Inspector", "Composition Note", "Layer Note",
                     "Properties", "Composition View (Software)",
                     "Layer View (Diligent)", "Layer View (Software)"};
    hiddenTitles = {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat"};
    break;
  case WorkspaceMode::VFX:
    visibleTitles = {"Composition Viewer", "Project", "Asset Browser",
                     "Inspector", "Composition Note", "Layer Note",
                     "Properties", "Composition View (Software)",
                     "Layer View (Diligent)", "Layer View (Software)"};
    hiddenTitles = {"Audio Mixer", "Contents Viewer", "AI Chat"};
    break;
  case WorkspaceMode::Compositing:
    visibleTitles = {"Composition Viewer", "Project", "Asset Browser",
                     "Inspector", "Composition Note", "Layer Note",
                     "Properties", "Layer View (Diligent)"};
    hiddenTitles = {"Audio Mixer", "Contents Viewer", "AI Cloud", "AI Chat",
                    "Composition View (Software)", "Layer View (Software)"};
    break;
  case WorkspaceMode::Audio:
    visibleTitles = {"Contents Viewer", "Audio Mixer", "Project",
                     "Asset Browser", "Inspector", "Properties"};
    hiddenTitles = {"AI Cloud", "AI Chat", "Composition Viewer",
                    "Composition View (Software)", "Layer View (Diligent)",
                    "Layer View (Software)"};
    break;
  }

  if (mode == WorkspaceMode::Default) {
    for (const QString &title : dockTitles) {
      setVisible(title, true);
    }
  } else {
    for (const QString &title : dockTitles) {
      setVisible(title, false);
    }
  }

  for (const QString &title : visibleTitles) {
    setVisible(title, true);
  }
  for (const QString &title : hiddenTitles) {
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
    refreshFloatingWidgetTree(content);
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

void scheduleQuitIfNoVisibleDocks(ArtifactMainWindow *window) {
  if (!window) {
    return;
  }

  QTimer::singleShot(0, window, [window]() {
    if (!window) {
      return;
    }

    const auto dockTitles = window->dockTitles();
    for (const auto &title : dockTitles) {
      if (window->isDockVisible(title)) {
        return;
      }
    }

    if (qApp) {
      qApp->quit();
    }
  });
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer *floatingWidget,
                                  QObject *eventFilterOwner);

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
        if (auto *window = qobject_cast<ArtifactMainWindow *>(owner)) {
          scheduleQuitIfNoVisibleDocks(window);
        }
      });
}

void prepareFloatingDockContainer(ads::CFloatingDockContainer *floatingWidget,
                                  QObject *eventFilterOwner) {
  if (!floatingWidget) {
    return;
  }

  if (eventFilterOwner) {
    floatingWidget->removeEventFilter(eventFilterOwner);
    floatingWidget->installEventFilter(eventFilterOwner);
  }

  applyDarkNativeTitleBar(floatingWidget);
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
  QWidget *centralWidgetHost = nullptr;
  CDockWidget *primaryCenterDock = nullptr;
  bool primaryCenterDockAssigned = false;
  QList<CDockWidget *> dockWidgets;
  WorkspaceMode workspaceMode_ = WorkspaceMode::Default;
  bool immersiveMode_ = false;
  Qt::WindowStates immersivePreviousWindowState_ = Qt::WindowNoState;
  QHash<CDockWidget *, bool> immersiveDockVisibility_;
  QPointer<CDockWidget> immersiveTargetDock_;
  bool menuBarInitialized = false;
  bool initialLayoutApplied = false;
  bool startupLayoutFrozen = true;
  ArtifactAICloudWidget *aiCloudWidget_ = nullptr;
  QHash<CDockWidget *, std::function<QWidget *()>> lazyDockFactories;
  QMetaObject::Connection currentTextLayerChangedConnection;
  QMetaObject::Connection currentShapeLayerChangedConnection;

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
        static_cast<int>(std::max(1.0f, textLayer->fontSize())),
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

    toolOptionsBar->setShapeOptions(
        static_cast<int>(shapeLayer->shapeType()),
        std::max(1, shapeLayer->shapeWidth()),
        std::max(1, shapeLayer->shapeHeight()), shapeLayer->fillEnabled(),
        shapeLayer->strokeEnabled(),
        static_cast<int>(std::lround(std::max(0.0f, shapeLayer->strokeWidth()))),
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
    : QMainWindow(parent), impl_(new Impl()) {
  setUpdatesEnabled(false);
  CDockManager::setConfigFlags(CDockManager::DefaultOpaqueConfig);
  // CDockManager::setConfigFlag(CDockManager::RetainTabSizeWhenCloseButtonHidden,
  // true);
  CDockManager::setConfigFlag(CDockManager::FocusHighlighting, false);
  CDockManager::setConfigFlag(CDockManager::TabCloseButtonIsToolButton, true);
  CDockManager::setConfigFlag(CDockManager::AllTabsHaveCloseButton, true);
  CDockManager::setConfigFlag(CDockManager::AlwaysShowTabs, true);

  QTimer::singleShot(0, this, [this]() {
    if (!impl_ || impl_->menuBarInitialized)
      return;
    auto *menuBar = new ArtifactMenuBar(this, this);
    setMenuBar(menuBar);

    // Pass main window reference to view menu for dynamic panel listing
    if (auto *viewMenu = menuBar->findChild<ArtifactViewMenu *>()) {
      viewMenu->setMainWindow(this);
    }

    impl_->menuBarInitialized = true;
  });

  auto *toolBar = new ArtifactToolBar(this);
  addToolBar(toolBar);
  impl_->toolBar = toolBar;

  impl_->toolOptionsBar = new ArtifactToolOptionsBar(this);
  impl_->toolOptionsBar->clearTextOptions();
  impl_->toolOptionsBar->clearShapeOptions();
  impl_->toolOptionsHost = new QToolBar(this);
  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    const QString workspaceModeText =
        settings->projectDefaultWorkspaceModeText();
    WorkspaceMode startupMode = WorkspaceMode::Default;
    if (workspaceModeText.compare(QStringLiteral("Animation"),
                                  Qt::CaseInsensitive) == 0) {
      startupMode = WorkspaceMode::Animation;
    } else if (workspaceModeText.compare(QStringLiteral("VFX"),
                                         Qt::CaseInsensitive) == 0) {
      startupMode = WorkspaceMode::VFX;
    } else if (workspaceModeText.compare(QStringLiteral("Compositing"),
                                         Qt::CaseInsensitive) == 0) {
      startupMode = WorkspaceMode::Compositing;
    } else if (workspaceModeText.compare(QStringLiteral("Audio"),
                                         Qt::CaseInsensitive) == 0) {
      startupMode = WorkspaceMode::Audio;
    }
    impl_->workspaceMode_ = startupMode;
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
  addToolBarBreak();
  addToolBar(impl_->toolOptionsHost);
  toolBar->setToolOptionsBar(impl_->toolOptionsBar);
  toolBar->refreshFromApplicationState();
  QObject::connect(toolBar, &ArtifactToolBar::workspaceModeChanged, this,
                   [this](WorkspaceMode mode) {
                     if (impl_) {
                       impl_->workspaceMode_ = mode;
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
            const float fontSize = std::max(1.0f, value.toFloat());
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

  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *selection = app->layerSelectionManager()) {
      QObject::connect(selection, &ArtifactLayerSelectionManager::selectionChanged,
                       this, [this]() {
                         if (impl_) {
                           impl_->syncTextToolOptions(this);
                           impl_->syncShapeToolOptions(this);
                         }
                       });
    }
  }
  impl_->syncTextToolOptions(this);
  impl_->syncShapeToolOptions(this);

  impl_->dockManager = new CDockManager(this);
  impl_->dockStyleManager = new DockStyleManager(impl_->dockManager, this);
  if (qApp) {
    qApp->installEventFilter(this);
  }
  QObject::connect(impl_->dockManager, &CDockManager::floatingWidgetCreated,
                   this, [this](ads::CFloatingDockContainer *floatingWidget) {
                     prepareFloatingDockContainer(floatingWidget, this);
                   });
  impl_->dockStyleManager->setGlowEnabled(true);
  impl_->dockStyleManager->setGlowColor(
      QColor(ArtifactCore::currentDCCTheme().accentColor));
  impl_->dockStyleManager->setGlowWidth(2);
  impl_->dockStyleManager->setGlowIntensity(0.72f);
  // Dock styling now comes from the global theme and DockStyleManager.
  impl_->centralWidgetHost = new QWidget(this);
  impl_->centralWidgetHost->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
  auto *centralDock = new CDockWidget(QStringLiteral("Workspace"), this);
  centralDock->setObjectName(QStringLiteral("ArtifactCentralDock"));
  centralDock->setWidget(impl_->centralWidgetHost);
  centralDock->setFeatures(
      ads::CDockWidget::AllDockWidgetFeatures); // Enable floating, etc.
  impl_->dockManager->setCentralWidget(centralDock);
  impl_->primaryCenterDock = centralDock;
  impl_->dockStyleManager->applyStyle();

  statusBar();
  resize(2000,
         1200); // Increased initial window size to give central area more space
}

ArtifactMainWindow::~ArtifactMainWindow() {
  if (qApp) {
    qApp->removeEventFilter(this);
  }
  delete impl_;
}

void ArtifactMainWindow::addWidget() {}

void ArtifactMainWindow::applyUiFontSettings() {
  if (!impl_) {
    return;
  }
  if (auto* bar = qobject_cast<ArtifactMenuBar*>(menuBar())) {
    bar->refreshFontFromSettings();
  }
  if (impl_->dockStyleManager) {
    impl_->dockStyleManager->applyStyle();
  }
}

void ArtifactMainWindow::addDockedWidget(const QString &title,
                                         ads::DockWidgetArea area,
                                         QWidget *widget) {
  if (!impl_ || !impl_->dockManager || !widget)
    return;
  if (area == ads::CenterDockWidgetArea && impl_->primaryCenterDock &&
      !impl_->primaryCenterDockAssigned) {
    impl_->primaryCenterDock->setWindowTitle(title);
    impl_->primaryCenterDock->setObjectName(title);
    impl_->primaryCenterDock->setWidget(widget);
    impl_->primaryCenterDock->setFeatures(
        ads::CDockWidget::AllDockWidgetFeatures);
    impl_->primaryCenterDockAssigned = true;
    if (!impl_->dockWidgets.contains(impl_->primaryCenterDock)) {
      impl_->dockWidgets.push_back(impl_->primaryCenterDock);
    }
    wireDockWidgetSignals(impl_->primaryCenterDock, this);
    impl_->dockStyleManager->applyStyle();
    if (title == "AI Cloud") {
      impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
    }
    applyWorkspaceMode(this, impl_->workspaceMode_);
    return;
  }
  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(title);
  dock->setWidget(widget);
  impl_->dockManager->addDockWidget(area, dock);
  impl_->dockWidgets.push_back(dock);
  wireDockWidgetSignals(dock, this);
  impl_->dockStyleManager->applyStyle();
  if (title == "AI Cloud") {
    impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
  }
  applyWorkspaceMode(this, impl_->workspaceMode_);
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
  impl_->dockStyleManager->applyStyle();
  applyWorkspaceMode(this, impl_->workspaceMode_);
}

void ArtifactMainWindow::addLazyDockedWidgetTabbedWithId(
    const QString &title, const QString &dockId, ads::DockWidgetArea area,
    std::function<QWidget *()> factory, const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->dockManager || !factory) {
    return;
  }

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  auto *placeholder = new QWidget(dock);
  dock->setWidget(placeholder);
  impl_->lazyDockFactories.insert(dock, std::move(factory));

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

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, this,
      [this, dock, placeholder](bool visible) mutable {
        if (!visible || !impl_ ||
            dock->property("artifactLazyWidgetCreated").toBool() ||
            dock->property("artifactLazyWidgetCreationPending").toBool()) {
          return;
        }

        if (impl_->startupLayoutFrozen) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
          return;
        }

        auto createNow = [this, dock, placeholder]() {
          if (!impl_ || !dock ||
              dock->property("artifactLazyWidgetCreated").toBool()) {
            if (dock) {
              dock->setProperty("artifactLazyWidgetCreationPending", false);
            }
            return;
          }

          if (!impl_->lazyDockFactories.contains(dock)) {
            dock->setProperty("artifactLazyWidgetCreationPending", false);
            return;
          }

          auto factory = impl_->lazyDockFactories.take(dock);
          QWidget *widget = factory ? factory() : nullptr;
          if (!widget) {
            dock->setProperty("artifactLazyWidgetCreationPending", false);
            return;
          }

          dock->setProperty("artifactLazyWidgetCreated", true);
          dock->setProperty("artifactLazyWidgetCreationPending", false);
          dock->setProperty("artifactLazyWidgetStartupPending", false);
          dock->setWidget(widget);
          if (dock->windowTitle() == QStringLiteral("AI Cloud")) {
            impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
          }
          if (placeholder) {
            placeholder->deleteLater();
          }
          refreshDockWidgetSurface(dock);
        };

        dock->setProperty("artifactLazyWidgetCreationPending", true);
        QTimer::singleShot(0, dock, createNow);
      });

  impl_->dockStyleManager->applyStyle();
}

void ArtifactMainWindow::addDockedWidgetFloating(
    const QString &title, const QString &dockId, QWidget *widget,
    const QRect &floatingGeometry) {
  if (!impl_ || !impl_->dockManager || !widget)
    return;

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  dock->setWidget(widget);

  auto *container = impl_->dockManager->addDockWidgetFloating(dock);
  if (container) {
    container->setGeometry(floatingGeometry);
  }

  impl_->dockWidgets.push_back(dock);
  if (!impl_->startupLayoutFrozen) {
    dock->toggleView(true);
  }
  wireDockWidgetSignals(dock, this);
  impl_->dockStyleManager->applyStyle();
}

void ArtifactMainWindow::addLazyDockedWidgetFloating(
    const QString &title, const QString &dockId,
    std::function<QWidget *()> factory, const QRect &floatingGeometry) {
  if (!impl_ || !impl_->dockManager || !factory) {
    return;
  }

  auto *dock = new CDockWidget(title, this);
  dock->setObjectName(dockId.isEmpty() ? title : dockId);
  auto *placeholder = new QWidget(dock);
  dock->setWidget(placeholder);
  impl_->lazyDockFactories.insert(dock, std::move(factory));

  QObject::connect(
      dock, &ads::CDockWidget::visibilityChanged, this,
      [this, dock, placeholder](bool visible) mutable {
        if (!visible || !impl_ ||
            dock->property("artifactLazyWidgetCreated").toBool() ||
            dock->property("artifactLazyWidgetCreationPending").toBool()) {
          return;
        }

        if (impl_->startupLayoutFrozen) {
          dock->setProperty("artifactLazyWidgetStartupPending", true);
          return;
        }

        dock->setProperty("artifactLazyWidgetCreationPending", true);
        QTimer::singleShot(0, dock, [this, dock, placeholder]() mutable {
          if (!impl_ || !dock ||
              dock->property("artifactLazyWidgetCreated").toBool()) {
            if (dock) {
              dock->setProperty("artifactLazyWidgetCreationPending", false);
            }
            return;
          }

          if (!impl_->lazyDockFactories.contains(dock)) {
            dock->setProperty("artifactLazyWidgetCreationPending", false);
            return;
          }

          auto factory = impl_->lazyDockFactories.take(dock);
          QWidget *widget = factory ? factory() : nullptr;
          if (!widget) {
            dock->setProperty("artifactLazyWidgetCreationPending", false);
            return;
          }

          dock->setProperty("artifactLazyWidgetCreated", true);
          dock->setProperty("artifactLazyWidgetCreationPending", false);
          dock->setProperty("artifactLazyWidgetStartupPending", false);
          dock->setWidget(widget);
          if (dock->windowTitle() == QStringLiteral("AI Cloud")) {
            impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
          }
          if (placeholder) {
            placeholder->deleteLater();
          }
          refreshDockWidgetSurface(dock);
        });
      });

  auto *container = impl_->dockManager->addDockWidgetFloating(dock);
  if (container) {
    container->setGeometry(floatingGeometry);
  }

  impl_->dockWidgets.push_back(dock);
  wireDockWidgetSignals(dock, this);
  dock->toggleView(true);
  impl_->dockStyleManager->applyStyle();
  applyWorkspaceMode(this, impl_->workspaceMode_);
}

void ArtifactMainWindow::moveDockToTabGroup(const QString &title,
                                            const QString &tabGroupPrefix) {
  if (!impl_ || !impl_->dockManager || title.isEmpty() ||
      tabGroupPrefix.isEmpty())
    return;

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
}

void ArtifactMainWindow::setDockVisible(const QString &title,
                                        const bool visible) {
  if (!impl_)
    return;

  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      const bool isOpen = !dock->isClosed();
      if (isOpen == visible) {
        return;
      }
      dock->toggleView(visible);
      return;
    }
  }
}

void ArtifactMainWindow::activateDock(const QString &title) {
  if (!impl_)
    return;
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      dock->toggleView(true);
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
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == title || dock->windowTitle() == title) {
      dock->closeDockWidget();
      impl_->dockStyleManager->applyStyle();
      return true;
    }
  }
  return false;
}

void ArtifactMainWindow::closeAllDocks() {
  if (!impl_)
    return;
  for (auto *dock : impl_->dockWidgets) {
    if (dock)
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
  statusBar()->showMessage(message, timeoutMs);
}

void ArtifactMainWindow::togglePanelsVisible(bool visible) {
  if (!impl_)
    return;
  qDebug() << "[MainWindow] togglePanelsVisible visible=" << visible;
  for (auto *dock : impl_->dockWidgets) {
    if (dock)
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
    case WorkspaceMode::Animation:
      modeText = QStringLiteral("Animation");
      break;
    case WorkspaceMode::VFX:
      modeText = QStringLiteral("VFX");
      break;
    case WorkspaceMode::Compositing:
      modeText = QStringLiteral("Compositing");
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
  statusBar()->showMessage(
      QStringLiteral("Zoom: %1%").arg(static_cast<int>(zoomPercent)), 1000);
}

void ArtifactMainWindow::setStatusCoordinates(int x, int y) {
  statusBar()->showMessage(QStringLiteral("X: %1 Y: %2").arg(x).arg(y), 1000);
}

void ArtifactMainWindow::setStatusMemoryUsage(uint64_t memoryMB) {
  statusBar()->showMessage(QStringLiteral("Memory: %1 MB").arg(memoryMB), 1000);
}

void ArtifactMainWindow::setStatusFPS(double fps) {
  statusBar()->showMessage(
      QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', 1)), 1000);
}

void ArtifactMainWindow::setStatusReady() {
  statusBar()->showMessage(QStringLiteral("Ready"), 1500);
}

void ArtifactMainWindow::setDockSplitterSizes(const QString &dockTitle,
                                              const QList<int> &sizes) {
  if (!impl_ || !impl_->dockManager)
    return;
  for (auto *dock : impl_->dockWidgets) {
    if (!dock)
      continue;
    if (dock->objectName() == dockTitle || dock->windowTitle() == dockTitle) {
      if (auto *area = dock->dockAreaWidget()) {
        impl_->dockManager->setSplitterSizes(area, sizes);
      }
      return;
    }
  }
}

void ArtifactMainWindow::setStartupLayoutFrozen(bool frozen) {
  if (!impl_ || impl_->startupLayoutFrozen == frozen) {
    return;
  }

  impl_->startupLayoutFrozen = frozen;
  setUpdatesEnabled(!frozen);

  if (frozen) {
    return;
  }

  for (auto *dock : impl_->dockWidgets) {
    if (!dock) {
      continue;
    }
    if (!dock->property("artifactLazyWidgetCreated").toBool() &&
        (dock->isVisible() ||
         dock->property("artifactLazyWidgetStartupPending").toBool())) {
      dock->setProperty("artifactLazyWidgetCreationPending", true);
      if (!impl_->lazyDockFactories.contains(dock)) {
        dock->setProperty("artifactLazyWidgetCreationPending", false);
        continue;
      }
      auto factory = impl_->lazyDockFactories.take(dock);
      QWidget *widget = factory ? factory() : nullptr;
      if (!widget) {
        dock->setProperty("artifactLazyWidgetCreationPending", false);
        continue;
      }
      QWidget *placeholder = dock->widget();
      dock->setProperty("artifactLazyWidgetCreated", true);
      dock->setProperty("artifactLazyWidgetCreationPending", false);
      dock->setProperty("artifactLazyWidgetStartupPending", false);
      dock->setWidget(widget);
      if (dock->windowTitle() == QStringLiteral("AI Cloud")) {
        impl_->aiCloudWidget_ = qobject_cast<ArtifactAICloudWidget *>(widget);
      }
      if (placeholder) {
        placeholder->deleteLater();
      }
      refreshDockWidgetSurface(dock);
    }
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
    update();
  });
}

void ArtifactMainWindow::keyPressEvent(QKeyEvent *event) {
  QMainWindow::keyPressEvent(event);
}

void ArtifactMainWindow::keyReleaseEvent(QKeyEvent *event) {
  QMainWindow::keyReleaseEvent(event);
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
  QMainWindow::showEvent(event);
  if (impl_ && impl_->startupLayoutFrozen) {
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
      return QMainWindow::eventFilter(watched, event);
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

  return QMainWindow::eventFilter(watched, event);
}

ArtifactAICloudWidget *ArtifactMainWindow::aiCloudWidget() const {
  return impl_->aiCloudWidget_;
}

} // namespace Artifact
