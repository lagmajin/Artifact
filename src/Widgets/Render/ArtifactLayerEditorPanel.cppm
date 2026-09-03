module;
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
import Artifact.Widgets.LayerEditorWidget;
import Artifact.Service.Playback;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

 class ArtifactLayerEditorPanel::Impl {
 private:

 public:
  ArtifactLayerEditorWidget* editor_ = nullptr;
  ArtifactCompositionViewerFooter* footer_ = nullptr;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  Impl();
  ~Impl();
 };

 ArtifactLayerEditorPanel::Impl::Impl()
 {
  editor_ = new ArtifactLayerEditorWidget();
  
  footer_ = new ArtifactCompositionViewerFooter();

  QObject::connect(footer_, &ArtifactCompositionViewerFooter::takeSnapShotRequested, editor_, &ArtifactLayerEditorWidget::takeScreenShot);
  if (auto* playback = ArtifactPlaybackService::instance()) {
    eventBusSubscriptions_.push_back(
        eventBus_.subscribe<PlaybackRamPreviewStatsChangedEvent>(
            [this](const PlaybackRamPreviewStatsChangedEvent& event) {
              if (footer_) {
                footer_->setRamPreviewStats(event.hitRate,
                                             event.cachedFrameCount);
              }
            }));
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
