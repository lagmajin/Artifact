module;

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
module Artifact.Layer.Composition;




import Artifact.Composition.Abstract;


namespace Artifact {

 class ArtifactCompositionLayer::Impl
 {
 private:
  
 public:
  Impl();
  ~Impl();
  CompositionID id_;
 };

 ArtifactCompositionLayer::ArtifactCompositionLayer()
 {

 }

 ArtifactCompositionLayer::~ArtifactCompositionLayer()
 {

 }

 CompositionID ArtifactCompositionLayer::sourceCompositionId() const
 {
  return impl_->id_;
 }

 void ArtifactCompositionLayer::setCompositionId(const CompositionID& id)
 {
  impl_->id_ = id;
 }



};