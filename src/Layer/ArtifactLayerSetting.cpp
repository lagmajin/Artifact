module;

module Artifact.Layer.Settings;

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



import Utils.String.UniString;
import Utils.Id;


namespace Artifact {

 using namespace ArtifactCore;

  class ArtifactLayerSetting::Impl {
  private:

  public:
   Impl();
   ~Impl();
   UniString layerName_;
   LayerID id_;
   bool visible_ = true;
   bool locked_ = false;
   bool solo_ = false;

  };
  ArtifactLayerSetting::Impl::Impl()
  {

  }
  ArtifactLayerSetting::Impl::~Impl()
  {

  }

  ArtifactLayerSetting::ArtifactLayerSetting() :impl_(new Impl())
  {
  }

  LayerID ArtifactLayerSetting::id() const { return impl_->id_; }

  void ArtifactLayerSetting::setId(const LayerID &id) { impl_->id_ = id; }

  bool ArtifactLayerSetting::visible() const { return impl_->visible_; }

  void ArtifactLayerSetting::setVisible(bool v) { impl_->visible_ = v; }

  bool ArtifactLayerSetting::locked() const { return impl_->locked_; }

  void ArtifactLayerSetting::setLocked(bool l) { impl_->locked_ = l; }

  bool ArtifactLayerSetting::solo() const { return impl_->solo_; }

  void ArtifactLayerSetting::setSolo(bool s) { impl_->solo_ = s; }

  ArtifactLayerSetting::ArtifactLayerSetting(const ArtifactLayerSetting& other)
  {
    if (other.impl_) {
      impl_ = new Impl();
      impl_->layerName_ = other.impl_->layerName_;
      impl_->id_ = other.impl_->id_;
      impl_->visible_ = other.impl_->visible_;
      impl_->locked_ = other.impl_->locked_;
      impl_->solo_ = other.impl_->solo_;
    } else {
      impl_ = new Impl();
    }
  }

  ArtifactLayerSetting::ArtifactLayerSetting(ArtifactLayerSetting&& other) noexcept
  {
    // Steal the impl pointer
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }

  ArtifactLayerSetting::~ArtifactLayerSetting()
  {
   delete impl_;
  }

  ArtifactLayerSetting& ArtifactLayerSetting::operator=(const ArtifactLayerSetting& other)
  {
    if (this == &other) return *this;
    if (!impl_) impl_ = new Impl();
    if (other.impl_) {
        impl_->layerName_ = other.impl_->layerName_;
        impl_->id_ = other.impl_->id_;
        impl_->visible_ = other.impl_->visible_;
        impl_->locked_ = other.impl_->locked_;
        impl_->solo_ = other.impl_->solo_;
    }
    return *this;
  }

  ArtifactLayerSetting& ArtifactLayerSetting::operator=(ArtifactLayerSetting&& other) noexcept
  {
    if (this == &other) return *this;
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
    return *this;
  }

};