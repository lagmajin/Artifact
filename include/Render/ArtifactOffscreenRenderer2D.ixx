module;

#include <d3d12.h>


#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>

#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/DeviceContextD3D12.h>

#include <boost/signals2.hpp>

export module Artifact.Render.Offscreen;
import Image.Raw;
import Composition;

import Color.Float;

export namespace ArtifactCore
{

 class OffscreenRenderer2D
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  OffscreenRenderer2D();
  ~OffscreenRenderer2D();
 };



};
