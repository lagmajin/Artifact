module;

export module Artifact.Layer.Solid2D;

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



import Color.Float;
import Artifact.Layer.Abstract;

export namespace Artifact
{
 using namespace ArtifactCore;
	
 class ArtifactSolid2DLayer:public ArtifactAbstractLayer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolid2DLayer();
  ~ArtifactSolid2DLayer();
  FloatColor color() const;
  void setColor(const FloatColor& color);
  void setSize(int width, int height);

  void draw(ArtifactIRenderer* renderer) override;

 };




}