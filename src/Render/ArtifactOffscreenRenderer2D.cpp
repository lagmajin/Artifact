module;
#include <QMap>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Fence.h>

#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>

module Artifact.Render.Offscreen;

import Image.Raw;
import Color.Float;
import Layer.Blend;

import std;

namespace ArtifactCore
{

 using namespace Diligent;

 class OffscreenRenderer2D::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> device_;
  RefCntAutoPtr<IDeviceContext> imContext_;
  RefCntAutoPtr<IDeviceContext> dfContext_;

  RefCntAutoPtr<IShader>	   pixelShader_;
  QMap<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> layer_blend_pso_map;
  void createPSO();
  void createShaders();
  void createConstantBuffers();
 public:
  Impl();
  ~Impl();
  void initialize();
  void blendLayer();
  void drawCompositionColor(const FloatColor& color);
 };

 OffscreenRenderer2D::Impl::Impl()
 {

 }

 OffscreenRenderer2D::Impl::~Impl()
 {

 }

 void OffscreenRenderer2D::Impl::initialize()
 {
  auto* pFactory = GetEngineFactoryD3D12();


  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;


  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs,&device_,&imContext_);
 }

 void OffscreenRenderer2D::Impl::createPSO()
 {
  ShaderCreateInfo pixelShaderCI;

  ShaderCreateInfo vertexShaderCI;



 }

 OffscreenRenderer2D::OffscreenRenderer2D()
 {

 }

 OffscreenRenderer2D::~OffscreenRenderer2D()
 {

 }

};
