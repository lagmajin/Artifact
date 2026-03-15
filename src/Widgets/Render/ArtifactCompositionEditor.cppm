module;
#include <QAction>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionEditor;

import Artifact.Widgets.CompositionRenderController;
import Color.Float;
import Artifact.Composition.Abstract;
import Artifact.Service.Project;

namespace Artifact {

W_OBJECT_IMPL(ArtifactCompositionEditor)

namespace {
 class CompositionViewport final : public QWidget {
 public:
  explicit CompositionViewport(CompositionRenderController* controller, QWidget* parent = nullptr)
   : QWidget(parent), controller_(controller)
  {
   setMinimumSize(1, 1);
   setAttribute(Qt::WA_NativeWindow);
   setAttribute(Qt::WA_PaintOnScreen);
   setAttribute(Qt::WA_NoSystemBackground);
   setMouseTracking(true);
   setFocusPolicy(Qt::StrongFocus);
  }

 protected:
  void showEvent(QShowEvent* event) override
  {
   QWidget::showEvent(event);
   if (controller_ && !controller_->isInitialized()) {
    controller_->initialize(this);
    controller_->setViewportSize((float)width(), (float)height());
    controller_->start();
   }
  }

  void closeEvent(QCloseEvent* event) override
  {
   if (controller_) {
    controller_->destroy();
   }
   QWidget::closeEvent(event);
  }

  void resizeEvent(QResizeEvent* event) override
  {
   QWidget::resizeEvent(event);
   if (controller_ && controller_->isInitialized()) {
    controller_->recreateSwapChain(this);
    controller_->setViewportSize((float)width(), (float)height());
   }
  }

  void paintEvent(QPaintEvent*) override {}

  void wheelEvent(QWheelEvent* event) override
  {
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

  void mouseDoubleClickEvent(QMouseEvent* event) override
  {
   if (controller_) {
    controller_->resetView();
   }
   event->accept();
  }

  void mousePressEvent(QMouseEvent* event) override
  {
   if (event->button() == Qt::MiddleButton) {
    isPanning_ = true;
    lastMousePos_ = event->position();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
   }
   
   if (controller_) {
    controller_->handleMousePress(event->position());
    if (controller_->gizmo() && controller_->gizmo()->isDragging()) {
     event->accept();
     return;
    }
   }

   QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent* event) override
  {
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
   }

   QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent* event) override
  {
   if (event->button() == Qt::MiddleButton && isPanning_) {
    isPanning_ = false;
    unsetCursor();
    event->accept();
    return;
   }

   if (controller_) {
    controller_->handleMouseRelease();
   }

   QWidget::mouseReleaseEvent(event);
  }

 private:
  CompositionRenderController* controller_ = nullptr;
  bool isPanning_ = false;
  QPointF lastMousePos_;
 };
} // namespace

class ArtifactCompositionEditor::Impl {
public:
 CompositionViewport* compositionView_ = nullptr;
 CompositionRenderController* renderController_ = nullptr;
 QToolBar* toolbar_ = nullptr;
 QAction* playAction_ = nullptr;
 QAction* stopAction_ = nullptr;
 QAction* resetAction_ = nullptr;
 QAction* zoomInAction_ = nullptr;
 QAction* zoomOutAction_ = nullptr;
 QAction* zoomFitAction_ = nullptr;
 QAction* zoom100Action_ = nullptr;
};

ArtifactCompositionEditor::ArtifactCompositionEditor(QWidget* parent)
 : QWidget(parent), impl_(new Impl())
{
 auto* layout = new QVBoxLayout(this);
 layout->setContentsMargins(0, 0, 0, 0);
 layout->setSpacing(0);

 impl_->renderController_ = new CompositionRenderController(this);
 impl_->compositionView_ = new CompositionViewport(impl_->renderController_, this);
 impl_->toolbar_ = new QToolBar(this);
 impl_->toolbar_->setMovable(false);

 impl_->playAction_ = impl_->toolbar_->addAction("Play");
 impl_->stopAction_ = impl_->toolbar_->addAction("Stop");
 impl_->resetAction_ = impl_->toolbar_->addAction("Reset");
 impl_->toolbar_->addSeparator();
 impl_->zoomInAction_ = impl_->toolbar_->addAction("Zoom+");
 impl_->zoomOutAction_ = impl_->toolbar_->addAction("Zoom-");
 impl_->zoomFitAction_ = impl_->toolbar_->addAction("Fit");
 impl_->zoom100Action_ = impl_->toolbar_->addAction("100%");

 layout->addWidget(impl_->compositionView_, 1);
 layout->addWidget(impl_->toolbar_);

 QObject::connect(impl_->playAction_, &QAction::triggered, this, &ArtifactCompositionEditor::play);
 QObject::connect(impl_->stopAction_, &QAction::triggered, this, &ArtifactCompositionEditor::stop);
 QObject::connect(impl_->resetAction_, &QAction::triggered, this, &ArtifactCompositionEditor::resetView);
 QObject::connect(impl_->zoomInAction_, &QAction::triggered, this, &ArtifactCompositionEditor::zoomIn);
 QObject::connect(impl_->zoomOutAction_, &QAction::triggered, this, &ArtifactCompositionEditor::zoomOut);
 QObject::connect(impl_->zoomFitAction_, &QAction::triggered, this, &ArtifactCompositionEditor::zoomFit);
 QObject::connect(impl_->zoom100Action_, &QAction::triggered, this, &ArtifactCompositionEditor::zoom100);

 if (auto* service = ArtifactProjectService::instance()) {
  QObject::connect(service, &ArtifactProjectService::currentCompositionChanged, this, [this](const ArtifactCore::CompositionID& id) {
   if (id.isNil()) {
    setComposition(nullptr);
   } else {
    auto compResult = ArtifactProjectService::instance()->findComposition(id);
    if (compResult.success) {
     setComposition(compResult.ptr.lock());
    } else {
     setComposition(nullptr);
    }
   }
  });
  QObject::connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
   setComposition(nullptr);
  });
 }
}

ArtifactCompositionEditor::~ArtifactCompositionEditor()
{
 delete impl_;
}

void ArtifactCompositionEditor::setComposition(ArtifactCompositionPtr composition)
{
 if (impl_->renderController_) {
  impl_->renderController_->setComposition(composition);
 }
}

void ArtifactCompositionEditor::setClearColor(const FloatColor& color)
{
 if (impl_->renderController_) {
  impl_->renderController_->setClearColor(color);
 }
}

void ArtifactCompositionEditor::play()
{
 if (impl_->renderController_) {
  impl_->renderController_->start();
 }
}

void ArtifactCompositionEditor::stop()
{
 if (impl_->renderController_) {
  impl_->renderController_->stop();
 }
}

void ArtifactCompositionEditor::resetView()
{
 if (impl_->renderController_) {
  impl_->renderController_->resetView();
 }
}

void ArtifactCompositionEditor::zoomIn()
{
 if (impl_->renderController_ && impl_->compositionView_) {
  impl_->renderController_->zoomInAt(QPointF(impl_->compositionView_->width() * 0.5, impl_->compositionView_->height() * 0.5));
 }
}

void ArtifactCompositionEditor::zoomOut()
{
 if (impl_->renderController_ && impl_->compositionView_) {
  impl_->renderController_->zoomOutAt(QPointF(impl_->compositionView_->width() * 0.5, impl_->compositionView_->height() * 0.5));
 }
}

void ArtifactCompositionEditor::zoomFit()
{
 if (impl_->renderController_) {
  impl_->renderController_->zoomFit();
 }
}

void ArtifactCompositionEditor::zoom100()
{
 if (impl_->renderController_) {
  impl_->renderController_->zoom100();
 }
}

}
