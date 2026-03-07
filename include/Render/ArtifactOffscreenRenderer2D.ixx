module;

#include <d3d12.h>


#include <DeviceContext.h>

#include <DeviceContextD3D12.h>

#include <boost/signals2.hpp>

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
export module Artifact.Render.Offscreen;





import Size;
import Image.Raw;
import Composition;
import Color.Float;
import Artifact.Layers;
import Transform._2D;

export namespace Artifact
{
 using namespace ArtifactCore;

 class OffscreenRenderer2D
 {
 private:
  class Impl;
  Impl* impl_;

 public:
  OffscreenRenderer2D();
  OffscreenRenderer2D(const Size_2D& size);
  ~OffscreenRenderer2D();

  void resize(const Size_2D& size);
  void resize(int width, int height);

  void setImageWriterPool();

  void addLayer();


  void renderStart();
 };



 typedef std::shared_ptr<OffscreenRenderer2D> OffscreenRenderer2DPtr;


 class Renderer2DFactory
 {
 private:

 public:
  Renderer2DFactory();
  ~Renderer2DFactory();
 };


};
