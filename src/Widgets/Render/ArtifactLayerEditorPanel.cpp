module;
#include <QWidget>
#include <QBoxLayout>


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
module Artifact.Widgets.LayerEditorPanel;




import Artifact.Widgets.CompositionFooter;
import Artifact.Widgets.RenderLayerWidgetv2;
import Artifact.Service.Playback;

namespace Artifact {

 class ArtifactLayerEditorPanel::Impl {
 private:

 public:
  ArtifactLayerEditorWidgetV2* editor_ = nullptr;
  ArtifactCompositionViewerFooter* footer_ = nullptr;

  Impl();
  ~Impl();
 };

 ArtifactLayerEditorPanel::Impl::Impl()
 {
  editor_ = new ArtifactLayerEditorWidgetV2();
  
  footer_ = new ArtifactCompositionViewerFooter();

  QObject::connect(footer_, &ArtifactCompositionViewerFooter::takeSnapShotRequested, editor_, &ArtifactLayerEditorWidgetV2::takeScreenShot);
  if (auto* playback = ArtifactPlaybackService::instance()) {
    QObject::connect(playback, &ArtifactPlaybackService::ramPreviewStatsChanged,
                     footer_, &ArtifactCompositionViewerFooter::setRamPreviewStats);
    footer_->setRamPreviewStats(playback->ramPreviewHitRate(), playback->ramPreviewCachedFrameCount());
  }
 }

 ArtifactLayerEditorPanel::Impl::~Impl()
 {

 }

 ArtifactLayerEditorPanel::ArtifactLayerEditorPanel(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  

  auto vBoxLayout = new QVBoxLayout(this);
  vBoxLayout->setContentsMargins(0, 0, 0, 0);
  vBoxLayout->setSpacing(1);
  vBoxLayout->addWidget(impl_->editor_);
  vBoxLayout->addWidget(impl_->footer_);

  setLayout(vBoxLayout);
 }

 ArtifactLayerEditorPanel::~ArtifactLayerEditorPanel()
 {
  delete impl_;
 }

 void ArtifactLayerEditorPanel::closeEvent(QCloseEvent* event)
 {
  this->deleteLater();
 }

 QSize ArtifactLayerEditorPanel::sizeHint() const
 {
  return QSize(400, 600);
 }

};
