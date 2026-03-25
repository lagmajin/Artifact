module;
#include <QToolBar>
#include <QVBoxLayout>
#include <QAction>
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
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  impl_->view = new ArtifactLayerEditorWidgetV2(this);
  impl_->toolbar = new QToolBar(this);
  impl_->toolbar->setMovable(false);

  impl_->playAction = impl_->toolbar->addAction("Play");
  impl_->stopAction = impl_->toolbar->addAction("Stop");
  impl_->screenshotAction = impl_->toolbar->addAction("Screenshot");

  layout->addWidget(impl_->view, 1);
  layout->addWidget(impl_->toolbar);

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
  return QSize(800, 600);
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
