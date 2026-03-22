module;
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QEvent>
#include <QHideEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <wobjectimpl.h>


module Artifact.Widgets.CompositionEditor;

import Artifact.Widgets.CompositionRenderController;
import Artifact.Widgets.TransformGizmo;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Service.Playback;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)

namespace {
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

class CompositionViewport final : public QWidget {
public:
  explicit CompositionViewport(CompositionRenderController *controller,
                               QWidget *parent = nullptr)
      : QWidget(parent), controller_(controller) {
    setMinimumSize(1, 1);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
  }

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    const bool needsInitialize = controller_ && !controller_->isInitialized();
    if (needsInitialize) {
      controller_->initialize(this);
      controller_->recreateSwapChain(this);
      controller_->setViewportSize((float)width(), (float)height());
    }
    if (controller_) {
      controller_->setComposition(resolvePreferredComposition());
      if (needsInitialize) {
        controller_->zoomFit();
      }
      controller_->start();
    }
  }

  void paintEvent(QPaintEvent *) override {
    // Rendering is driven by QTimer in the controller.
    // With WA_PaintOnScreen the backing store is bypassed.
  }

  void hideEvent(QHideEvent *event) override {
    if (controller_) {
      controller_->stop();
    }
    QWidget::hideEvent(event);
  }

  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (controller_ && controller_->isInitialized()) {
      controller_->recreateSwapChain(this);
      controller_->setViewportSize((float)width(), (float)height());
      controller_->renderOneFrame();
    }
  }

  void wheelEvent(QWheelEvent *event) override {
    if (!controller_) {
      return;
    }

    if (event->angleDelta().y() > 0) {
      controller_->zoomInAt(event->position());
    } else if (event->angleDelta().y() < 0) {
      controller_->zoomOutAt(event->position());
    }
    event->accept();
  }

  void mouseDoubleClickEvent(QMouseEvent *event) override {
    if (controller_) {
      controller_->resetView();
    }
    event->accept();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::MiddleButton) {
      isPanning_ = true;
      lastMousePos_ = event->position();
      setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMousePress(event);
      if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
        const auto cursor = controller_->cursorShapeForViewportPos(event->position());
        setCursor(cursor == Qt::OpenHandCursor ? Qt::ClosedHandCursor : cursor);
        event->accept();
        return;
      }
    }
    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (isPanning_ && controller_) {
      const QPointF delta = event->position() - lastMousePos_;
      lastMousePos_ = event->position();
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
      setCursor(controller_->cursorShapeForViewportPos(event->position()));
    }

    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (event->button() == Qt::MiddleButton && isPanning_) {
      isPanning_ = false;
      unsetCursor();
      event->accept();
      return;
    }

    if (controller_) {
      controller_->handleMouseRelease();
      setCursor(controller_->cursorShapeForViewportPos(event->position()));
    }

    QWidget::mouseReleaseEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    if (!isPanning_) {
      unsetCursor();
    }
    QWidget::leaveEvent(event);
  }

private:
  CompositionRenderController *controller_ = nullptr;
  bool isPanning_ = false;
  QPointF lastMousePos_;
};
} // namespace

class ArtifactCompositionEditor::Impl {
public:
  CompositionViewport *compositionView_ = nullptr;
  CompositionRenderController *renderController_ = nullptr;

  // Top Toolbar (Zoom/View controls)
  QToolBar *topToolbar_ = nullptr;
  QAction *resetAction_ = nullptr;
  QAction *zoomInAction_ = nullptr;
  QAction *zoomOutAction_ = nullptr;
  QAction *zoomFitAction_ = nullptr;
  QAction *zoom100Action_ = nullptr;

  // Bottom Viewer Controls
  QWidget *bottomBar_ = nullptr;
  QComboBox *resolutionCombo_ = nullptr;
  QToolButton *fastPreviewBtn_ = nullptr;
  QToolButton *displayOptionsBtn_ = nullptr;
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  impl_->renderController_ = new CompositionRenderController(this);
  impl_->renderController_->setClearColor({ 0.12f, 0.13f, 0.18f, 1.0f });
  impl_->compositionView_ =
      new CompositionViewport(impl_->renderController_, this);

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
  checkerboardAct->setCheckable(true);
  gridAct->setCheckable(true);
  guidesAct->setCheckable(true);
  safeMarginsAct->setCheckable(true);
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

  // Initialize checked state
  if (impl_->renderController_) {
    checkerboardAct->setChecked(impl_->renderController_->isShowCheckerboard());
    gridAct->setChecked(impl_->renderController_->isShowGrid());
    guidesAct->setChecked(impl_->renderController_->isShowGuides());
    safeMarginsAct->setChecked(impl_->renderController_->isShowSafeMargins());
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

void ArtifactCompositionEditor::setComposition(
    ArtifactCompositionPtr composition) {
  if (impl_->renderController_) {
    impl_->renderController_->setComposition(composition);
  }
}

void ArtifactCompositionEditor::setClearColor(const FloatColor &color) {
  if (impl_->renderController_) {
    impl_->renderController_->setClearColor(color);
  }
}

void ArtifactCompositionEditor::play() {
  if (impl_->renderController_) {
    impl_->renderController_->start();
  }
}

void ArtifactCompositionEditor::stop() {
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
