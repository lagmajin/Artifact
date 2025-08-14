module;

#include <d3d12.h>


#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>

#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/DeviceContextD3D12.h>

#include <boost/signals2.hpp>

export module Artifact.Render.Offscreen;

import std;

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
