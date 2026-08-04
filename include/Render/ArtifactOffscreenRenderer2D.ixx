module;

#include <d3d12.h>


#include <DeviceContext.h>

#include <DeviceContextD3D12.h>

//#include <boost/signals2.hpp>

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
#include <QImage>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
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
import Color.Float;
import Transform._2D;
import Memory.SharedPtr;

export namespace Artifact
{
 using namespace ArtifactCore;

 class OffscreenRenderer2D
 {
 private:
  class Impl;
  Impl* impl_ = nullptr;

 public:
  OffscreenRenderer2D();
  OffscreenRenderer2D(const Size_2D& size);
  ~OffscreenRenderer2D();

  void resize(const Size_2D& size);
  void resize(int width, int height);

  void setImageWriterPool();

  void addLayer();
  void addLayer(float x, float y, const QImage& image);


  void renderStart();
  void renderFrame(double time);
  void drawSolidRect(const FloatColor& color);
  void drawImage(float x, float y, const QImage& image);
  void drawPoint(const Point2DF& point);
  QImage toImage() const;
 };



 typedef SharedPtr<OffscreenRenderer2D> OffscreenRenderer2DPtr;


 class Renderer2DFactory
 {
 private:

 public:
  Renderer2DFactory();
  ~Renderer2DFactory();
  OffscreenRenderer2DPtr create(const Size_2D& size) const;
 };


};
