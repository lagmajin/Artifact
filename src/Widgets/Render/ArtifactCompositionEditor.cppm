module;
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDebug>
#include <QIcon>
#include <QHBoxLayout>
#include <QEvent>
#include <QContextMenuEvent>
#include <QHideEvent>
#include <QHash>
#include <QFocusEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QResizeEvent>
#include <QShowEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QSignalBlocker>
#include <QSet>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QVector>
#include <wobjectimpl.h>
#include "../../../../out/build/x64-Debug/vcpkg_installed/x64-windows/include/Qt6/QtCore/QTimer"


module Artifact.Widgets.CompositionEditor;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import Artifact.Widgets.PieMenu;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Layer.Video;
import Artifact.Tool.Manager;
import Utils.Path;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)

namespace {
QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  QIcon icon(resourcePath);
  if (!icon.isNull()) {
    return icon;
  }
  return QIcon(ArtifactCore::resolveIconPath(fileName));
}

ArtifactCompositionPtr resolvePreferredComposition() {
  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      if (auto comp = active->activeComposition()) {
        return comp;
      }
    }
  }

  if (auto *playback = ArtifactPlaybackService::instance()) {
    if (auto comp = playback->currentComposition()) {
      return comp;
    }
  }

  if (auto *service = ArtifactProjectService::instance()) {
    return service->currentComposition().lock();
  }

  return {};
}

bool editTextLayerInline(QWidget* parent, const ArtifactAbstractLayerPtr& layer)
{
  const auto textLayer = std::dynamic_pointer_cast<ArtifactTextLayer>(layer);
  if (!textLayer) {
    return false;
  }

  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Edit Text Layer"));
  dialog.setModal(true);
  dialog.resize(640, 360);

  auto *layout = new QVBoxLayout(&dialog);
  auto *editor = new QPlainTextEdit(&dialog);
  editor->setPlainText(textLayer->text().toQString());
  editor->setPlaceholderText(QStringLiteral("Enter text..."));
  editor->selectAll();
  editor->setFocus();
  layout->addWidget(editor, 1);

  auto* commitShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), &dialog);
  QObject::connect(commitShortcut, &QShortcut::activated, &dialog, &QDialog::accept);
  auto* commitShortcutAlt = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), &dialog);
  QObject::connect(commitShortcutAlt, &QShortcut::activated, &dialog, &QDialog::accept);
  auto* cancelShortcut = new QShortcut(QKeySequence::Cancel, &dialog);
  QObject::connect(cancelShortcut, &QShortcut::activated, &dialog, &QDialog::reject);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  textLayer->setText(ArtifactCore::UniString::fromQString(editor->toPlainText()));
  Q_EMIT textLayer->changed();
  return true;
}

class CompositionViewport final : public QWidget {
public:
  explicit CompositionViewport(CompositionRenderController *controller,
                               QWidget *parent = nullptr)
      : QWidget(parent), controller_(controller) {
    setMinimumSize(1, 1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    resizeDebounceTimer_ = new QTimer(this);
    resizeDebounceTimer_->setSingleShot(true);
    QObject::connect(resizeDebounceTimer_, &QTimer::timeout, this, [this]() {
      if (!controller_ || !controller_->isInitialized()) {
        resizePending_ = false;
        return;
      }
      const QSize pendingSize = pendingResizeSize_.isValid() ? pendingResizeSize_ : size();
      controller_->recreateSwapChain(this);
      controller_->setViewportSize(static_cast<float>(pendingSize.width()),
                                   static_cast<float>(pendingSize.height()));
      controller_->renderOneFrame();
      resizePending_ = false;
    });
  }

  void requestInitialFit() {
    pendingInitialFit_ = true;
    scheduleInitialFit();
  }

  void setPieMenu(ArtifactPieMenuWidget* pieMenu) { pieMenu_ = pieMenu; }

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    if (controller_) {
      const bool needsInitialize = !controller_->isInitialized();
      if (needsInitialize) {
        QTimer::singleShot(0, this, [this]() {
          if (!controller_ || !isVisible() || controller_->isInitialized()) {
            return;
          }
          QElapsedTimer timer;
          timer.start();
          controller_->initialize(this);
          qInfo() << "[CompositionEditor][Startup] controller initialize ms=" << timer.elapsed();
          timer.restart();
          controller_->recreateSwapChain(this);
          qInfo() << "[CompositionEditor][Startup] recreateSwapChain ms=" << timer.elapsed();
          controller_->setViewportSize((float)width(), (float)height());
          controller_->setComposition(resolvePreferredComposition());
          pendingInitialFit_ = true;
          scheduleInitialFit();
          controller_->start();
        });
      } else {
        controller_->setComposition(resolvePreferredComposition());
        scheduleInitialFit();
        controller_->start();
      }
    }
  }

  void paintEvent(QPaintEvent *) override {
    // Rendering is driven by QTimer in the controller.
    // With WA_PaintOnScreen the backing store is bypassed.
  }

  void hideEvent(QHideEvent *event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    if (controller_) {
      controller_->stop();
    }
    QWidget::hideEvent(event);
  }

  void focusOutEvent(QFocusEvent* event) override {
    restoreTemporarySolo();
    restoreTemporaryPlayback();
    QWidget::focusOutEvent(event);
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (controller_ && controller_->isInitialized()) {
      controller_->setViewportSize(static_cast<float>(event->size().width()),
                                   static_cast<float>(event->size().height()));
      scheduleInitialFit();
      pendingResizeSize_ = event->size();
      resizePending_ = true;
      if (resizeDebounceTimer_) {
        resizeDebounceTimer_->stop();
        resizeDebounceTimer_->start(80);
      }
    }
  }

  void wheelEvent(QWheelEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible()) return; // Block while menu open

    if (!controller_) {
      return;
    }

    controller_->notifyViewportInteractionActivity();

    const auto modifiers = event->modifiers();
    const QPointF angleDelta = event->angleDelta();

    if (modifiers.testFlag(Qt::AltModifier) || modifiers.testFlag(Qt::ControlModifier)) {
      // AE Style: Alt/Ctrl + Wheel = Zoom
      if (angleDelta.y() > 0) {
        controller_->zoomInAt(event->position());
      } else if (angleDelta.y() < 0) {
        controller_->zoomOutAt(event->position());
      }
    } else if (modifiers.testFlag(Qt::ShiftModifier)) {
      // AE Style: Shift + Wheel = Horizontal Pan
      float deltaX = angleDelta.y(); // Vertical wheel converted to horizontal
      controller_->panBy(QPointF(deltaX, 0));
    } else {
      // AE Style: Wheel = Vertical Pan
      float deltaY = angleDelta.y();
      controller_->panBy(QPointF(0, deltaY));
    }
    
    event->accept();
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (controller_) {
      const auto layerId = controller_->layerAtViewportPos(event->position());
      if (!layerId.isNil()) {
        if (const auto comp = currentComposition()) {
          if (auto layer = comp->layerById(layerId)) {
            if (editTextLayerInline(this, layer)) {
              controller_->renderOneFrame();
              event->accept();
              return;
            }
          }
        }
      } else {
        controller_->resetView();
        event->accept();
        return;
      }
    }
    event->accept();
  }

  void contextMenuEvent(QContextMenuEvent* event) override {
    const auto layerId = controller_ ? controller_->layerAtViewportPos(event->pos()) : LayerID::Nil();
    if (layerId.isNil()) {
      QWidget::contextMenuEvent(event);
      return;
    }

    const auto comp = currentComposition();
    const auto layer = comp ? comp->layerById(layerId) : ArtifactAbstractLayerPtr{};
    if (!layer || !std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
      QWidget::contextMenuEvent(event);
      return;
    }

    QMenu menu(this);
    auto* editAction = menu.addAction(QStringLiteral("Edit Text"));
    editAction->setEnabled(true);
    connect(editAction, &QAction::triggered, this, [this, layer]() {
      if (editTextLayerInline(this, layer) && controller_) {
        controller_->renderOneFrame();
      }
    });

    menu.addSeparator();
    menu.addAction(QStringLiteral("Reset View"), this, [this]() {
      if (controller_) {
        controller_->resetView();
      }
    });
    menu.exec(event->globalPos());
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible()) return;

    if (event->button() == Qt::MiddleButton || 
        (event->button() == Qt::LeftButton && spacePressed_)) {
      isPanning_ = true;
      lastMousePos_ = event->position();
      if (controller_) {
        controller_->notifyViewportInteractionActivity();
      }
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }

    if (controller_ && !spacePressed_) {
      controller_->handleMousePress(event);
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
        grabMouse();
        const auto cursor = controller_->cursorShapeForViewportPos(event->position());
        setCursor(cursor == Qt::OpenHandCursor ? Qt::ClosedHandCursor : cursor);
        event->accept();
        return;
      }
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible()) {
        pieMenu_->updateMousePos(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    if (isPanning_ && controller_) {
      const QPointF delta = event->position() - lastMousePos_;
      lastMousePos_ = event->position();
      controller_->notifyViewportInteractionActivity();
      controller_->panBy(delta);
      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMouseMove(event->position());
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
        event->accept();
        return;
      }
      if (spacePressed_) {
          setCursor(Qt::OpenHandCursor);
      } else {
          setCursor(controller_->cursorShapeForViewportPos(event->position()));
      }
    }

    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (pieMenu_ && pieMenu_->isVisible()) {
        // Confirmation happens on KeyRelease (Tab), but we can also allow click to confirm
        if (event->button() == Qt::LeftButton) {
            pieMenu_->confirmSelection();
        }
        event->accept();
        return;
    }

    if ((event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) && isPanning_) {
      isPanning_ = false;
      if (controller_) {
        controller_->finishViewportInteraction();
      }
      if (spacePressed_) {
          setCursor(Qt::OpenHandCursor);
      } else {
          unsetCursor();
      }
      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMouseRelease();
      releaseMouse();
      if (!spacePressed_) {
          setCursor(controller_->cursorShapeForViewportPos(event->position()));
      }
    }

    QWidget::mouseReleaseEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    if (controller_) {
      const bool gizmoDragging = controller_->gizmo() && controller_->gizmo()->isDragging();
      const bool maskEditing = controller_->hasPendingMaskEdit();
      if (gizmoDragging || maskEditing) {
        controller_->handleMouseRelease();
        if (gizmoDragging) {
          releaseMouse();
        }
      }
    }
    if (!isPanning_) {
      unsetCursor();
    }
    QWidget::leaveEvent(event);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
        showPieMenu();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
      spacePressed_ = true;
      setCursor(Qt::OpenHandCursor);
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F && !event->isAutoRepeat()) {
      if (controller_) {
        controller_->focusSelectedLayer();
      }
      event->accept();
      return;
    }
    if (event->key() == Qt::Key_F12) {
      if (controller_) {
          saveCurrentFrame(controller_);
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_P) {
      beginTemporaryPlayback();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_M) {
      if (auto* toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr) {
        toolManager->setActiveTool(ToolType::Pen);
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_QuoteLeft || event->key() == Qt::Key_AsciiTilde)) {
      beginTemporarySolo();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_H) {
      if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        soloCurrentLayer();
      } else {
        toggleCurrentLayerVisibility();
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        event->key() == Qt::Key_S &&
        event->modifiers().testFlag(Qt::ShiftModifier)) {
      soloCurrentLayer();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        event->key() == Qt::Key_C &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        event->modifiers().testFlag(Qt::AltModifier)) {
      centerCurrentLayer();
      event->accept();
      return;
    }
    if ((event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) && !event->isAutoRepeat()) {
      auto* app = ArtifactApplicationManager::instance();
      auto* svc = ArtifactProjectService::instance();
      auto* active = app ? app->activeContextService() : nullptr;
      auto* selection = app ? app->layerSelectionManager() : nullptr;
      const auto selectedLayers = selection ? selection->selectedLayers() : QSet<ArtifactAbstractLayerPtr>{};
      const auto currentComp = active ? active->activeComposition() : ArtifactCompositionPtr{};
      if (svc && currentComp && !selectedLayers.isEmpty()) {
        if (selectedLayers.size() > 1) {
          for (const auto& layer : selectedLayers) {
            if (layer) {
              svc->removeLayerFromComposition(currentComp->id(), layer->id());
            }
          }
        } else if (const auto currentLayer = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{}; currentLayer) {
          svc->removeLayerFromComposition(currentComp->id(), currentLayer->id());
        }
        event->accept();
        return;
      }
    }
    QWidget::keyPressEvent(event);
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Tab && !event->isAutoRepeat()) {
        if (pieMenu_ && pieMenu_->isVisible()) {
            pieMenu_->confirmSelection();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
      spacePressed_ = false;
      isPanning_ = false;
      if (controller_) {
        controller_->finishViewportInteraction();
      }
      unsetCursor();
      if (controller_) {
          setCursor(controller_->cursorShapeForViewportPos(mapFromGlobal(QCursor::pos())));
      }
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() &&
        (event->key() == Qt::Key_QuoteLeft || event->key() == Qt::Key_AsciiTilde)) {
      restoreTemporarySolo();
      event->accept();
      return;
    }
    if (!event->isAutoRepeat() && event->key() == Qt::Key_P) {
      restoreTemporaryPlayback();
      event->accept();
      return;
    }
    QWidget::keyReleaseEvent(event);
  }

private:
  struct TemporarySoloState {
    LayerID layerId;
    bool solo = false;
  };

  ArtifactAbstractLayerPtr currentLayer() const {
    auto* app = ArtifactApplicationManager::instance();
    auto* selection = app ? app->layerSelectionManager() : nullptr;
    return selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
  }

  ArtifactCompositionPtr currentComposition() const {
    auto* active = ArtifactApplicationManager::instance()
                       ? ArtifactApplicationManager::instance()->activeContextService()
                       : nullptr;
    return active ? active->activeComposition() : ArtifactCompositionPtr{};
  }

  void beginTemporarySolo() {
    if (temporarySoloActive_) {
      return;
    }
    auto* svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    const auto layer = currentLayer();
    if (!svc || !comp || !layer) {
      return;
    }

    temporarySoloStates_.clear();
    const auto layers = comp->allLayer();
    temporarySoloStates_.reserve(layers.size());
    for (const auto& candidate : layers) {
      if (!candidate) {
        continue;
      }
      temporarySoloStates_.push_back({candidate->id(), candidate->isSolo()});
    }

    temporarySoloActive_ = true;
    svc->soloOnlyLayerInCurrentComposition(layer->id());
  }

  void beginTemporaryPlayback() {
    if (temporaryPlaybackActive_) {
      return;
    }
    auto* playback = ArtifactPlaybackService::instance();
    if (!playback || playback->isPlaying()) {
      return;
    }
    temporaryPlaybackActive_ = true;
    playback->play();
    if (controller_) {
      controller_->start();
    }
  }

  void restoreTemporarySolo() {
    if (!temporarySoloActive_) {
      return;
    }
    auto* svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    if (!svc || !comp) {
      temporarySoloActive_ = false;
      temporarySoloStates_.clear();
      return;
    }

    for (const auto& state : temporarySoloStates_) {
      if (state.layerId.isNil()) {
        continue;
      }
      svc->setLayerSoloInCurrentComposition(state.layerId, state.solo);
    }
    temporarySoloActive_ = false;
    temporarySoloStates_.clear();
  }

  void restoreTemporaryPlayback() {
    if (!temporaryPlaybackActive_) {
      return;
    }
    auto* playback = ArtifactPlaybackService::instance();
    if (playback && playback->isPlaying()) {
      playback->stop();
    }
    if (controller_) {
      controller_->stop();
    }
    temporaryPlaybackActive_ = false;
  }

  void toggleCurrentLayerVisibility() {
    auto* svc = ArtifactProjectService::instance();
    const auto layer = currentLayer();
    if (!svc || !layer) {
      return;
    }
    const bool current = svc->isLayerVisibleInCurrentComposition(layer->id());
    svc->setLayerVisibleInCurrentComposition(layer->id(), !current);
  }

  void soloCurrentLayer() {
    auto* svc = ArtifactProjectService::instance();
    const auto layer = currentLayer();
    if (!svc || !layer) {
      return;
    }
    svc->soloOnlyLayerInCurrentComposition(layer->id());
  }

  void centerCurrentLayer() {
    auto* svc = ArtifactProjectService::instance();
    const auto comp = currentComposition();
    const auto layer = currentLayer();
    if (!svc || !comp || !layer) {
      return;
    }

    const QSize compSize = comp->settings().compositionSize();
    const float compCenterX = static_cast<float>(compSize.width() > 0 ? compSize.width() : 1920) * 0.5f;
    const float compCenterY = static_cast<float>(compSize.height() > 0 ? compSize.height() : 1080) * 0.5f;

    if (layer->is3D()) {
      const QVector3D current = layer->position3D();
      const QVector3D centeredPos(compCenterX, compCenterY, current.z());
      layer->setPosition3D(centeredPos);
    } else {
      const QVector3D current = layer->position3D();
      const QVector3D centeredPos(compCenterX, compCenterY, current.z());
      layer->setPosition3D(centeredPos);
    }
    layer->changed();
  }

  QVector<TemporarySoloState> temporarySoloStates_;
  bool temporarySoloActive_ = false;
  bool temporaryPlaybackActive_ = false;
  void scheduleInitialFit() {
    if (!pendingInitialFit_) {
      return;
    }
    QTimer::singleShot(0, this, [this]() {
      if (!pendingInitialFit_ || !controller_ || !isVisible() || !controller_->isInitialized()) {
        if (pendingInitialFit_) {
          QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
        }
        return;
      }
      if (width() <= 64 || height() <= 64) {
        QTimer::singleShot(50, this, [this]() { scheduleInitialFit(); });
        return;
      }
      controller_->zoomFit();
      pendingInitialFit_ = false;
    });
  }

  ArtifactPieMenuWidget *pieMenu_ = nullptr;
  void showPieMenu() {
      if (!pieMenu_ || !controller_) return;

      PieMenuModel model;
      model.title = "View Controls";

      auto* toolManager = ArtifactApplicationManager::instance()
                              ? ArtifactApplicationManager::instance()->toolManager()
                              : nullptr;

      // Selection Tool
      model.items.push_back({
          "Select", 
          loadIconWithFallback("MaterialVS/neutral/select.svg"), 
          "tool.select", true, false, 
          [toolManager]() { if(toolManager) toolManager->setActiveTool(ToolType::Selection); }
      });

      // Hand Tool
      model.items.push_back({
          "Hand", 
          loadIconWithFallback("MaterialVS/neutral/hand.svg"), 
          "tool.hand", true, false, 
          [toolManager]() { if(toolManager) toolManager->setActiveTool(ToolType::Hand); }
      });

      // Mask Tool
      model.items.push_back({
          "Mask",
          loadIconWithFallback("MaterialVS/neutral/draw.svg"),
          "tool.mask", true, false,
          [toolManager]() { if (toolManager) toolManager->setActiveTool(ToolType::Pen); }
      });

      // Zoom Fit
      model.items.push_back({
          "Fit", 
          loadIconWithFallback("MaterialVS/neutral/fit.svg"), 
          "view.fit", true, false, 
          [this]() { controller_->zoomFit(); }
      });

      // Zoom 100%
      model.items.push_back({
          "100%", 
          loadIconWithFallback("MaterialVS/neutral/zoom_100.svg"), 
          "view.100", true, false, 
          [this]() { controller_->zoom100(); }
      });

      // Reset View
      model.items.push_back({
          "Reset", 
          loadIconWithFallback("MaterialVS/neutral/reset.svg"), 
          "view.reset", true, false, 
          [this]() { controller_->resetView(); }
      });

      if (auto* gizmo3D = controller_->gizmo3D()) {
          model.items.push_back({
              "3D Move",
              QIcon(),
              "gizmo3d.move", true, gizmo3D->mode() == GizmoMode::Move,
              [this, gizmo3D]() { gizmo3D->setMode(GizmoMode::Move); controller_->renderOneFrame(); }
          });
          model.items.push_back({
              "3D Rotate",
              QIcon(),
              "gizmo3d.rotate", true, gizmo3D->mode() == GizmoMode::Rotate,
              [this, gizmo3D]() { gizmo3D->setMode(GizmoMode::Rotate); controller_->renderOneFrame(); }
          });
          model.items.push_back({
              "3D Scale",
              QIcon(),
              "gizmo3d.scale", true, gizmo3D->mode() == GizmoMode::Scale,
              [this, gizmo3D]() { gizmo3D->setMode(GizmoMode::Scale); controller_->renderOneFrame(); }
          });
      }

      // Grid Toggle
      model.items.push_back({
          "Grid", 
          loadIconWithFallback("MaterialVS/neutral/grid.svg"), 
          "display.grid", true, controller_->isShowGrid(), 
          [this]() { controller_->setShowGrid(!controller_->isShowGrid()); }
      });

      // Safe Area Toggle
      model.items.push_back({
          "Safe Area", 
          loadIconWithFallback("MaterialVS/neutral/safe_area.svg"), 
          "display.safeArea", true, controller_->isShowSafeMargins(), 
          [this]() { controller_->setShowSafeMargins(!controller_->isShowSafeMargins()); }
      });

      pieMenu_->setModel(model);
      pieMenu_->showAt(QCursor::pos());
  }

  void saveCurrentFrame(CompositionRenderController* controller) {
      auto comp = controller->composition();
      if (!comp) return;

      auto* svc = ArtifactProjectService::instance();
      if (!svc) return;

      // 選択されているレイヤーを取得（簡易的にコントローラーから）
      // 実際には SelectionManager 経由が良いが、controller にもキャッシュがある
      // ここでは CompositionRenderController の内部実装にアクセスできないため、
      // 外部から見える情報を元にするか、controller にメソッドを追加する必要がある。
      // 一旦、コンポジション全体のレンダリング結果（もし取れれば）か、
      // ログ出力で生存確認する。

      qDebug() << "Debug: F12 pressed. Attempting to save current frame...";
      
      // フォルダ作成
      QDir dir(".");
      if (!dir.exists("test")) {
          dir.mkdir("test");
      }

      // TODO: 本来は controller->captureSelectedLayer() のようなものが必要
      // ここではコンポジション内の「最初の動画レイヤー」を探して保存するデバッグコードを試みる
      for (auto& layer : comp->allLayer()) {
          if (auto video = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
              QImage img = video->currentFrameToQImage();
              if (!img.isNull()) {
                  QString path = QString("test/frame_%1_%2.png")
                      .arg(layer->id().toString())
                      .arg(comp->framePosition().framePosition());
                  if (img.save(path)) {
                      qDebug() << "Successfully saved debug frame to:" << path;
                  } else {
                      qWarning() << "Failed to save image to:" << path;
                  }
              } else {
                  qWarning() << "Layer" << layer->id().toString() << "returned null image.";
              }
          }
      }
  }

  CompositionRenderController *controller_ = nullptr;
  bool isPanning_ = false;
  bool spacePressed_ = false;
  bool pendingInitialFit_ = true;
  QTimer *resizeDebounceTimer_ = nullptr;
  QSize pendingResizeSize_;
  bool resizePending_ = false;
  QPointF lastMousePos_;
};
} // namespace

class ArtifactCompositionEditor::Impl {
public:
  CompositionViewport *compositionView_ = nullptr;
  CompositionRenderController *renderController_ = nullptr;
  ArtifactPieMenuWidget *pieMenu_ = nullptr;

  // Top Toolbar (Zoom/View controls)
  QToolBar *topToolbar_ = nullptr;
  QAction *resetAction_ = nullptr;
  QAction *zoomInAction_ = nullptr;
  QAction *zoomOutAction_ = nullptr;
  QAction *zoomFitAction_ = nullptr;
  QAction *zoom100Action_ = nullptr;
  QAction *editTextAction_ = nullptr;
  QAction *motionPathAction_ = nullptr;
  QToolButton *toolModeButton_ = nullptr;
  QToolButton *gizmoModeButton_ = nullptr;

  // Bottom Viewer Controls
  QWidget *bottomBar_ = nullptr;
  QComboBox *resolutionCombo_ = nullptr;
  QToolButton *fastPreviewBtn_ = nullptr;
  QToolButton *displayOptionsBtn_ = nullptr;
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumSize(960, 640);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->renderController_ = new CompositionRenderController(this);
  impl_->renderController_->setClearColor({ 0.12f, 0.13f, 0.18f, 1.0f });

  QObject::connect(impl_->renderController_, &CompositionRenderController::videoDebugMessage,
                   this, &ArtifactCompositionEditor::videoDebugMessage);
  impl_->compositionView_ =
      new CompositionViewport(impl_->renderController_, this);

  impl_->pieMenu_ = new ArtifactPieMenuWidget(this);
  impl_->compositionView_->setPieMenu(impl_->pieMenu_);

  // Top Toolbar
  impl_->topToolbar_ = new QToolBar(this);
  impl_->topToolbar_->setMovable(false);
  impl_->topToolbar_->setIconSize(QSize(18, 18));
  impl_->topToolbar_->setStyleSheet(
      "QToolBar { background: #252526; border-bottom: 1px solid #1e1e1e; "
      "spacing: 2px; }");

  impl_->resetAction_ = impl_->topToolbar_->addAction("Reset");
  impl_->topToolbar_->addSeparator();
  impl_->zoomInAction_ = impl_->topToolbar_->addAction("Zoom+");
  impl_->zoomOutAction_ = impl_->topToolbar_->addAction("Zoom-");
  impl_->zoomFitAction_ = impl_->topToolbar_->addAction("Fit");
  impl_->zoom100Action_ = impl_->topToolbar_->addAction("100%");
  impl_->editTextAction_ = impl_->topToolbar_->addAction("Edit Text");
  impl_->editTextAction_->setToolTip(QStringLiteral("Edit current text layer"));
  impl_->editTextAction_->setShortcut(QKeySequence(Qt::Key_F2));
  impl_->motionPathAction_ = impl_->topToolbar_->addAction("Motion Path");
  impl_->motionPathAction_->setCheckable(true);
  impl_->motionPathAction_->setChecked(true);
  impl_->motionPathAction_->setToolTip(QStringLiteral("Show motion path overlay for the selected layer"));

  auto* toolMenu = new QMenu(impl_->topToolbar_);
  auto* toolGroup = new QActionGroup(this);
  toolGroup->setExclusive(true);
  const auto addToolAction = [&](const QString& text, const QString& iconName, ToolType toolType, bool checked) {
    QAction* action = toolMenu->addAction(loadIconWithFallback(iconName), text);
    action->setCheckable(true);
    action->setChecked(checked);
    toolGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, toolType, text]() {
      if (auto* toolManager = ArtifactApplicationManager::instance()
                                  ? ArtifactApplicationManager::instance()->toolManager()
                                  : nullptr) {
        toolManager->setActiveTool(toolType);
      }
      if (impl_->toolModeButton_) {
        impl_->toolModeButton_->setText(text);
      }
    });
  };
  addToolAction(QStringLiteral("Select"), QStringLiteral("MaterialVS/neutral/select.svg"), ToolType::Selection, true);
  addToolAction(QStringLiteral("Hand"), QStringLiteral("MaterialVS/neutral/hand.svg"), ToolType::Hand, false);
  addToolAction(QStringLiteral("Mask"), QStringLiteral("MaterialVS/neutral/draw.svg"), ToolType::Pen, false);
  impl_->toolModeButton_ = new QToolButton(this);
  impl_->toolModeButton_->setText(QStringLiteral("Select"));
  impl_->toolModeButton_->setMenu(toolMenu);
  impl_->toolModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->toolModeButton_);

  auto* gizmoMenu = new QMenu(impl_->topToolbar_);
  auto* gizmoGroup = new QActionGroup(this);
  gizmoGroup->setExclusive(true);
  const auto addGizmoAction = [&](const QString& text, TransformGizmo::Mode mode, bool checked) {
    QAction* action = gizmoMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    gizmoGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, mode]() {
      if (auto* gizmo = impl_->renderController_ ? impl_->renderController_->gizmo() : nullptr) {
        gizmo->setMode(mode);
        impl_->renderController_->renderOneFrame();
      }
    });
  };
  addGizmoAction(QStringLiteral("Gizmo: All"), TransformGizmo::Mode::All, true);
  addGizmoAction(QStringLiteral("Gizmo: Move"), TransformGizmo::Mode::Move, false);
  addGizmoAction(QStringLiteral("Gizmo: Rotate"), TransformGizmo::Mode::Rotate, false);
  addGizmoAction(QStringLiteral("Gizmo: Scale"), TransformGizmo::Mode::Scale, false);
  impl_->gizmoModeButton_ = new QToolButton(this);
  impl_->gizmoModeButton_->setText(QStringLiteral("Gizmo"));
  impl_->gizmoModeButton_->setMenu(gizmoMenu);
  impl_->gizmoModeButton_->setPopupMode(QToolButton::InstantPopup);
  impl_->topToolbar_->addWidget(impl_->gizmoModeButton_);

  // Bottom Bar (Viewer Controls)
  impl_->bottomBar_ = new QWidget(this);
  impl_->bottomBar_->setFixedHeight(28);
  impl_->bottomBar_->setStyleSheet(
      "background: #252526; border-top: 1px solid #1e1e1e;");

  auto *bottomLayout = new QHBoxLayout(impl_->bottomBar_);
  bottomLayout->setContentsMargins(6, 0, 6, 0);
  bottomLayout->setSpacing(8);

  // Resolution Dropdown — wired to PreviewQualityPreset
  impl_->resolutionCombo_ = new QComboBox(impl_->bottomBar_);
  impl_->resolutionCombo_->addItem("Full", QVariant::fromValue(static_cast<int>(PreviewQualityPreset::Final)));
  impl_->resolutionCombo_->addItem("Half", QVariant::fromValue(static_cast<int>(PreviewQualityPreset::Preview)));
  impl_->resolutionCombo_->addItem("Quarter", QVariant::fromValue(static_cast<int>(PreviewQualityPreset::Draft)));
  impl_->resolutionCombo_->setFixedWidth(70);
  impl_->resolutionCombo_->setStyleSheet(
      "QComboBox { background: #333; color: #ccc; border: 1px solid #444; "
      "font-size: 11px; }");

  // Fast Preview Button (Lightning)
  impl_->fastPreviewBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->fastPreviewBtn_->setText("⚡"); // Lightning icon
  impl_->fastPreviewBtn_->setToolTip("Fast Preview (Lightning)");
  impl_->fastPreviewBtn_->setPopupMode(QToolButton::InstantPopup);
  impl_->fastPreviewBtn_->setStyleSheet(
      "QToolButton { color: #ffeb3b; font-weight: bold; background: "
      "transparent; border: none; } QToolButton:hover { background: #444; }");

  auto *fastPreviewMenu = new QMenu(impl_->fastPreviewBtn_);
  QAction *fpOff = fastPreviewMenu->addAction("Off");
  QAction *fpAdaptive = fastPreviewMenu->addAction("Adaptive Resolution");
  QAction *fpDraft = fastPreviewMenu->addAction("Fast Draft");
  fpOff->setCheckable(true);
  fpAdaptive->setCheckable(true);
  fpDraft->setCheckable(true);
  fpOff->setChecked(true);
  auto *fpGroup = new QActionGroup(fastPreviewMenu);
  fpGroup->setExclusive(true);
  fpGroup->addAction(fpOff);
  fpGroup->addAction(fpAdaptive);
  fpGroup->addAction(fpDraft);

  QObject::connect(fpOff, &QAction::triggered, this, [this]() {
    if (auto* svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Final);
  });
  QObject::connect(fpAdaptive, &QAction::triggered, this, [this]() {
    if (auto* svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Preview);
  });
  QObject::connect(fpDraft, &QAction::triggered, this, [this]() {
    if (auto* svc = ArtifactProjectService::instance())
      svc->setPreviewQualityPreset(PreviewQualityPreset::Draft);
  });

  impl_->fastPreviewBtn_->setMenu(fastPreviewMenu);

  // Display Options Button (Grid/Guides)
  impl_->displayOptionsBtn_ = new QToolButton(impl_->bottomBar_);
  impl_->displayOptionsBtn_->setText("👁"); // View options icon
  impl_->displayOptionsBtn_->setToolTip("Choose transparency, grid, and guide options");
  impl_->displayOptionsBtn_->setPopupMode(QToolButton::InstantPopup);
  impl_->displayOptionsBtn_->setStyleSheet(
      "QToolButton { color: #ccc; background: transparent; border: none; } "
      "QToolButton:hover { background: #444; }");

  auto *displayMenu = new QMenu(impl_->displayOptionsBtn_);
  QAction *checkerboardAct = displayMenu->addAction("Checkerboard");
  QAction *gridAct = displayMenu->addAction("Grid");
  QAction *guidesAct = displayMenu->addAction("Guides");
  QAction *safeMarginsAct = displayMenu->addAction("Safe Area");
  displayMenu->addSeparator();
  QAction *gpuBlendAct = displayMenu->addAction("GPU Blend (CS)");
  checkerboardAct->setCheckable(true);
  gridAct->setCheckable(true);
  guidesAct->setCheckable(true);
  safeMarginsAct->setCheckable(true);
  gpuBlendAct->setCheckable(true);
  impl_->displayOptionsBtn_->setMenu(displayMenu);

  // Connect actions
  QObject::connect(checkerboardAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_)
      impl_->renderController_->setShowCheckerboard(checked);
  });
  QObject::connect(gridAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_)
      impl_->renderController_->setShowGrid(checked);
  });
  QObject::connect(guidesAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_)
      impl_->renderController_->setShowGuides(checked);
  });
  QObject::connect(safeMarginsAct, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_)
                       impl_->renderController_->setShowSafeMargins(checked);
                   });
  QObject::connect(gpuBlendAct, &QAction::toggled, this, [this](bool checked) {
    if (impl_->renderController_) {
      impl_->renderController_->setGpuBlendEnabled(checked);
    }
  });

  // Initialize checked state
  if (impl_->renderController_) {
    checkerboardAct->setChecked(impl_->renderController_->isShowCheckerboard());
    gridAct->setChecked(impl_->renderController_->isShowGrid());
    guidesAct->setChecked(impl_->renderController_->isShowGuides());
    safeMarginsAct->setChecked(impl_->renderController_->isShowSafeMargins());
    gpuBlendAct->setChecked(impl_->renderController_->isGpuBlendEnabled());
  }

  bottomLayout->addWidget(impl_->resolutionCombo_);
  bottomLayout->addWidget(impl_->fastPreviewBtn_);
  bottomLayout->addWidget(impl_->displayOptionsBtn_);
  bottomLayout->addStretch();

  // Assembly
  mainLayout->addWidget(impl_->topToolbar_);
  mainLayout->addWidget(impl_->compositionView_, 1);
  mainLayout->addWidget(impl_->bottomBar_);

  // Connections
  QObject::connect(impl_->resetAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::resetView);
  QObject::connect(impl_->zoomInAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomIn);
  QObject::connect(impl_->zoomOutAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomOut);
  QObject::connect(impl_->zoomFitAction_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoomFit);
  QObject::connect(impl_->zoom100Action_, &QAction::triggered, this,
                   &ArtifactCompositionEditor::zoom100);
  QObject::connect(impl_->editTextAction_, &QAction::triggered, this,
                   [this]() {
                     auto* app = ArtifactApplicationManager::instance();
                     auto* selection = app ? app->layerSelectionManager() : nullptr;
                     const auto layer = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
                     if (!layer || !std::dynamic_pointer_cast<ArtifactTextLayer>(layer)) {
                       return;
                     }
                     if (editTextLayerInline(impl_->compositionView_, layer) &&
                         impl_->renderController_) {
                       impl_->renderController_->renderOneFrame();
                     }
                   });
  QObject::connect(impl_->motionPathAction_, &QAction::toggled, this,
                   [this](bool checked) {
                     if (impl_->renderController_) {
                       impl_->renderController_->setShowMotionPathOverlay(checked);
                     }
                   });

  // Resolution dropdown connection
  QObject::connect(impl_->resolutionCombo_,
                   QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                   [this](int index) {
                     auto preset = static_cast<PreviewQualityPreset>(
                         impl_->resolutionCombo_->itemData(index).toInt());
                     if (auto* svc = ArtifactProjectService::instance()) {
                       svc->setPreviewQualityPreset(preset);
                     }
                   });

  if (auto *service = ArtifactProjectService::instance()) {
    const auto syncResolutionCombo = [this](PreviewQualityPreset preset) {
      if (!impl_ || !impl_->resolutionCombo_) {
        return;
      }
      const int targetIndex = std::clamp(
          impl_->resolutionCombo_->findData(QVariant::fromValue(static_cast<int>(preset))),
          0, std::max(0, impl_->resolutionCombo_->count() - 1));
      QSignalBlocker blocker(impl_->resolutionCombo_);
      impl_->resolutionCombo_->setCurrentIndex(targetIndex);
    };
    syncResolutionCombo(service->previewQualityPreset());
    QObject::connect(service, &ArtifactProjectService::previewQualityPresetChanged,
                     this, syncResolutionCombo);
    QObject::connect(
        service, &ArtifactProjectService::currentCompositionChanged, this,
        [this](const ArtifactCore::CompositionID &id) {
          if (id.isNil()) {
            setComposition(nullptr);
          } else {
            auto compResult =
                ArtifactProjectService::instance()->findComposition(id);
            if (compResult.success) {
              setComposition(compResult.ptr.lock());
            } else {
              setComposition(nullptr);
            }
          }
        });
    QObject::connect(
        service, &ArtifactProjectService::projectChanged, this,
        [this]() { setComposition(resolvePreferredComposition()); });
  }

  if (auto *app = ArtifactApplicationManager::instance()) {
    if (auto *active = app->activeContextService()) {
      QObject::connect(active,
                       &ArtifactActiveContextService::activeCompositionChanged,
                       this, [this](ArtifactCompositionPtr composition) {
                         setComposition(composition);
                       });
    }
    if (auto *toolManager = app->toolManager()) {
      QObject::connect(toolManager, &ArtifactToolManager::toolChanged, this,
                       [this](ToolType type) {
                         if (!impl_ || !impl_->toolModeButton_) {
                           return;
                         }
                         switch (type) {
                           case ToolType::Selection:
                             impl_->toolModeButton_->setText(QStringLiteral("Select"));
                             break;
                           case ToolType::Hand:
                             impl_->toolModeButton_->setText(QStringLiteral("Hand"));
                             break;
                           case ToolType::Pen:
                             impl_->toolModeButton_->setText(QStringLiteral("Mask"));
                             break;
                           default:
                             impl_->toolModeButton_->setText(QStringLiteral("Tool"));
                             break;
                         }
                       });
    }
    if (auto *selection = app->layerSelectionManager()) {
      QObject::connect(selection, &ArtifactLayerSelectionManager::selectionChanged,
                       this, [this, selection]() {
                         if (!impl_ || !impl_->renderController_) {
                           return;
                         }
                         const auto current = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
                         const int selectedCount = selection ? selection->selectedLayers().size() : 0;
                         impl_->renderController_->setSelectedLayerId(
                             current ? current->id() : ArtifactCore::LayerID::Nil());
                         if (impl_->editTextAction_) {
                            impl_->editTextAction_->setEnabled(
                               selectedCount == 1 &&
                               current && std::dynamic_pointer_cast<ArtifactTextLayer>(current));
                         }
                       });
      if (impl_->editTextAction_) {
        const auto current = selection->currentLayer();
        impl_->editTextAction_->setEnabled(
            selection->selectedLayers().size() == 1 &&
            current && std::dynamic_pointer_cast<ArtifactTextLayer>(current));
      }
    }
  }

  if (auto *playback = ArtifactPlaybackService::instance()) {
    QObject::connect(playback,
                     &ArtifactPlaybackService::currentCompositionChanged, this,
                     [this](ArtifactCompositionPtr composition) {
                       setComposition(composition);
                     });
  }
}

ArtifactCompositionEditor::~ArtifactCompositionEditor() { delete impl_; }

QSize ArtifactCompositionEditor::sizeHint() const { return QSize(1280, 820); }

void ArtifactCompositionEditor::setComposition(ArtifactCompositionPtr composition) {
  if (impl_->renderController_) {
    impl_->renderController_->setComposition(composition);
  }
  if (auto* playback = ArtifactPlaybackService::instance()) {
    playback->setCurrentComposition(composition);
  }
  if (impl_->compositionView_) {
    impl_->compositionView_->requestInitialFit();
  }
}

void ArtifactCompositionEditor::setClearColor(const FloatColor &color) {
  if (impl_->renderController_) {
    impl_->renderController_->setClearColor(color);
  }
}

void ArtifactCompositionEditor::play() {
  if (auto* playback = ArtifactPlaybackService::instance()) {
    playback->play();
  }
  if (impl_->renderController_) {
    impl_->renderController_->start();
  }
}

void ArtifactCompositionEditor::stop() {
  if (auto* playback = ArtifactPlaybackService::instance()) {
    playback->stop();
  }
  if (impl_->renderController_) {
    impl_->renderController_->stop();
  }
}

void ArtifactCompositionEditor::resetView() {
  if (impl_->renderController_) {
    impl_->renderController_->resetView();
  }
}

void ArtifactCompositionEditor::zoomIn() {
  if (impl_->renderController_ && impl_->compositionView_) {
    impl_->renderController_->zoomInAt(
        QPointF(impl_->compositionView_->width() * 0.5,
                impl_->compositionView_->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomOut() {
  if (impl_->renderController_ && impl_->compositionView_) {
    impl_->renderController_->zoomOutAt(
        QPointF(impl_->compositionView_->width() * 0.5,
                impl_->compositionView_->height() * 0.5));
  }
}

void ArtifactCompositionEditor::zoomFit() {
  if (impl_->renderController_) {
    impl_->renderController_->zoomFit();
  }
}

void ArtifactCompositionEditor::zoom100() {
  if (impl_->renderController_) {
    impl_->renderController_->zoom100();
  }
}

} // namespace Artifact
