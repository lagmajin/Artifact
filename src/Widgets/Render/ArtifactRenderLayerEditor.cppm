module;
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QActionGroup>
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
  QToolButton* editModeButton = nullptr;
  QToolButton* displayModeButton = nullptr;
 };

 ArtifactRenderLayerEditor::ArtifactRenderLayerEditor(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  impl_->view = new ArtifactLayerEditorWidgetV2(this);
  impl_->toolbar = new QToolBar(this);
  impl_->toolbar->setMovable(false);

  impl_->playAction = impl_->toolbar->addAction("Play");
  impl_->stopAction = impl_->toolbar->addAction("Stop");
  impl_->screenshotAction = impl_->toolbar->addAction("Screenshot");

  auto* editMenu = new QMenu(impl_->toolbar);
  auto* editGroup = new QActionGroup(this);
  editGroup->setExclusive(true);
  const auto addEditAction = [&](const QString& text, EditMode mode, bool checked) {
   QAction* action = editMenu->addAction(text);
   action->setCheckable(true);
   action->setChecked(checked);
   editGroup->addAction(action);
   connect(action, &QAction::triggered, this, [this, mode]() {
    if (impl_ && impl_->view) {
     impl_->view->setEditMode(mode);
    }
   });
  };
  addEditAction(QStringLiteral("View"), EditMode::View, true);
  addEditAction(QStringLiteral("Transform"), EditMode::Transform, false);
  addEditAction(QStringLiteral("Path"), EditMode::Paint, false);
  addEditAction(QStringLiteral("Mask"), EditMode::Mask, false);
  impl_->editModeButton = new QToolButton(this);
  impl_->editModeButton->setText(QStringLiteral("Edit Mode"));
  impl_->editModeButton->setMenu(editMenu);
  impl_->editModeButton->setPopupMode(QToolButton::InstantPopup);
  impl_->toolbar->addWidget(impl_->editModeButton);

  auto* displayMenu = new QMenu(impl_->toolbar);
  auto* displayGroup = new QActionGroup(this);
  displayGroup->setExclusive(true);
  const auto addDisplayAction = [&](const QString& text, DisplayMode mode, bool checked) {
   QAction* action = displayMenu->addAction(text);
   action->setCheckable(true);
   action->setChecked(checked);
   displayGroup->addAction(action);
   connect(action, &QAction::triggered, this, [this, mode]() {
    if (impl_ && impl_->view) {
     impl_->view->setDisplayMode(mode);
    }
   });
  };
  addDisplayAction(QStringLiteral("Color"), DisplayMode::Color, true);
  addDisplayAction(QStringLiteral("Alpha"), DisplayMode::Alpha, false);
  addDisplayAction(QStringLiteral("Mask"), DisplayMode::Mask, false);
  addDisplayAction(QStringLiteral("Wireframe"), DisplayMode::Wireframe, false);
  impl_->displayModeButton = new QToolButton(this);
  impl_->displayModeButton->setText(QStringLiteral("Display"));
  impl_->displayModeButton->setMenu(displayMenu);
  impl_->displayModeButton->setPopupMode(QToolButton::InstantPopup);
  impl_->toolbar->addWidget(impl_->displayModeButton);

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
