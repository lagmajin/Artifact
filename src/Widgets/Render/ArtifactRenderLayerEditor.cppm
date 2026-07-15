module;
#include <QToolBar>
#include <QVBoxLayout>
#include <QAction>
#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QSize>
#include <wobjectimpl.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Widgets.RenderLayerEditor;

import Artifact.Widgets.RenderLayerWidgetv2;
import Color.Float;
import Utils.Id;
import Utils.Path;

namespace Artifact {

 W_OBJECT_IMPL(ArtifactRenderLayerEditor)

 class ArtifactRenderLayerEditor::Impl {
  public:
  ArtifactLayerEditorWidgetV2* view = nullptr;
  QToolBar* toolbar = nullptr;
  QAction* playAction = nullptr;
  QAction* stopAction = nullptr;
  QAction* screenshotAction = nullptr;
 };

 ArtifactRenderLayerEditor::ArtifactRenderLayerEditor(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setWindowTitle(QStringLiteral("Layer Solo View"));
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  impl_->view = new ArtifactLayerEditorWidgetV2(this);
  impl_->view->setWindowTitle(QStringLiteral("Layer Solo View"));
  impl_->toolbar = new QToolBar(this);
  impl_->toolbar->setMovable(false);
  impl_->toolbar->setFloatable(false);
  impl_->toolbar->setIconSize(QSize(18, 18));
  impl_->toolbar->setFixedHeight(42);
  QPalette toolbarPalette = impl_->toolbar->palette();
  toolbarPalette.setColor(QPalette::Window, QColor(20, 23, 27));
  toolbarPalette.setColor(QPalette::WindowText, QColor(229, 233, 238));
  toolbarPalette.setColor(QPalette::Button, QColor(31, 36, 42));
  toolbarPalette.setColor(QPalette::ButtonText, QColor(229, 233, 238));
  toolbarPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  toolbarPalette.setColor(QPalette::HighlightedText, Qt::white);
  impl_->toolbar->setPalette(toolbarPalette);
  impl_->toolbar->setAutoFillBackground(true);

  impl_->playAction = impl_->toolbar->addAction("Play");
  impl_->stopAction = impl_->toolbar->addAction("Stop");
  impl_->screenshotAction = impl_->toolbar->addAction("Capture");
  impl_->playAction->setIcon(QIcon(ArtifactCore::resolveIconPath(
      QStringLiteral("Studio/figma_media_play.svg"))));
  impl_->stopAction->setIcon(QIcon(ArtifactCore::resolveIconPath(
      QStringLiteral("Studio/figma_media_stop.svg"))));
  impl_->screenshotAction->setIcon(QIcon(ArtifactCore::resolveIconPath(
      QStringLiteral("Studio/figma_tool_camera.svg"))));
  layout->addWidget(impl_->toolbar);
  layout->addWidget(impl_->view, 1);

  connect(impl_->playAction, &QAction::triggered, this, &ArtifactRenderLayerEditor::play);
  connect(impl_->stopAction, &QAction::triggered, this, &ArtifactRenderLayerEditor::stop);
  connect(impl_->screenshotAction, &QAction::triggered, this, &ArtifactRenderLayerEditor::takeScreenShot);
 }

 ArtifactRenderLayerEditor::~ArtifactRenderLayerEditor()
 {
  delete impl_;
 }

 QSize ArtifactRenderLayerEditor::sizeHint() const
 {
  return QSize(1280, 820);
 }

 ArtifactLayerEditorWidgetV2* ArtifactRenderLayerEditor::view() const
 {
  return impl_->view;
 }

 void ArtifactRenderLayerEditor::setClearColor(const FloatColor& color)
 {
  impl_->view->setClearColor(color);
 }

 void ArtifactRenderLayerEditor::setTargetLayer(const LayerID& id)
 {
  impl_->view->setTargetLayer(id);
 }

 void ArtifactRenderLayerEditor::setEditMode(EditMode mode)
 {
  impl_->view->setEditMode(mode);
 }

 void ArtifactRenderLayerEditor::setDisplayMode(DisplayMode mode)
 {
  impl_->view->setDisplayMode(mode);
 }

 void ArtifactRenderLayerEditor::resetView()
 {
  impl_->view->resetView();
 }

 void ArtifactRenderLayerEditor::play()
 {
  impl_->view->play();
 }

 void ArtifactRenderLayerEditor::stop()
 {
  impl_->view->stop();
 }

 void ArtifactRenderLayerEditor::takeScreenShot()
 {
  impl_->view->takeScreenShot();
 }

}
