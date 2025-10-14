module;
#include <tbb/tbb.h>
#include <QMap>
#include <QDebug>
#include <QThreadPool>
#include <boost/range/size.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Fence.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>


#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>


module Artifact.Render.Offscreen;

import Core.Point2D;
import Image.Raw;
import Color.Float;
import Layer.Blend;

import Graphics.Shader.Compute.HLSL.Blend;

import std;

//#offscreen

namespace Artifact
{
 


 using namespace Diligent;
 using namespace ArtifactCore;

 struct LayerSizeCB
 {

 };

 struct RendererWorker
 {
  RefCntAutoPtr<IDeviceContext> pDefferedContext;
  RefCntAutoPtr<IFence>		   blendFence_;
 };


 class OffscreenRenderer2D::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> renderDevice_;
  RefCntAutoPtr<IDeviceContext> mainDeviceContext_;
  //RefCntAutoPtr<IDeviceContext> dfContext_;

  RefCntAutoPtr<IShader>	   pixelShader_;
  RefCntAutoPtr<ITexture>      compositionBuffer_;
  RefCntAutoPtr<ITexture>	   layerRenderTarget_;

  RefCntAutoPtr<IFence>		   blendFence_;

  QMap<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> blendShaders_;



  QMap<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> layer_blend_pso_map;

  bool shader_compiled_ = false;
  bool pso_created = false;

  void createLayerBlendPSO();
  void createShaders();
  void createConstantBuffers();
 public:
  Impl();
  Impl(const Size_2D& size);
  ~Impl();
  void initialize();
  void blendLayer(LAYER_BLEND_TYPE type);
  void clearCompositionColor(const FloatColor& color);

  void resize(int width, int height);

  void drawSprite(int width, int height);

  void renderFrame(double time);

  void renderStart();

  void drawSolidLayer(const FloatColor& color);
  void drawImage(float x, float y, const QImage& image);
  void drawImage(const Point2DF&,const QImage& image);
  void drawPoint(const Point2DF& point);
  //void drawImageLayer(const FloatImage& image);
 };

 OffscreenRenderer2D::Impl::Impl()
 {

 }

 OffscreenRenderer2D::Impl::Impl(const Size_2D& size)
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


  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &renderDevice_, &mainDeviceContext_);

  TextureDesc TexDesc;
  TexDesc.Type = RESOURCE_DIM_TEX_2D;
  TexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
  TexDesc.Usage = USAGE_DEFAULT;


  FenceDesc fenceDesc;
  fenceDesc.Name = "BlendFence";
  fenceDesc.Type = FENCE_TYPE_GENERAL;
  renderDevice_->CreateFence(fenceDesc, &blendFence_);

  //blendFence_=device_->CreateFence()


  createShaders();
  createLayerBlendPSO();
 }

 void OffscreenRenderer2D::Impl::createLayerBlendPSO()
 {

  for (auto [blendType, pso] : layer_blend_pso_map.asKeyValueRange())
  {
   ComputePipelineStateCreateInfo info;
   info.PSODesc.Name = "";
   auto shaderIt = blendShaders_.find(blendType);
   if (shaderIt == blendShaders_.end())
   {
    //qDebug() << "Blend shader not found for type" << blendType;
    continue;
   }

   info.pCS = shaderIt.value();

   //RefCntAutoPtr<IPipelineState> pso_;
   //renderDevice_->CreateComputePipelineState(info, &pso);

   // マップに保存
   //layer_blend_pso_map[blendType] = pso_;


  }



 }

 void OffscreenRenderer2D::Impl::createShaders()
 {
  ShaderCreateInfo pixelShaderCI;
  pixelShaderCI.EntryPoint = "main";
  pixelShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
  pixelShaderCI.Desc.Name = "";

  ShaderCreateInfo vertexShaderCI;
  vertexShaderCI.EntryPoint = "main";
  vertexShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
  vertexShaderCI.Desc.Name = "";

  QMap<LAYER_BLEND_TYPE, ShaderCreateInfo> createInfoMap;

  for (const auto& [blendType, shaderCode] : BlendShaders) {

   ShaderCreateInfo shaderCI;
   //shaderCI.CompileFlags=
   shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
   shaderCI.Desc.Name = "";
   shaderCI.Source = shaderCode.constData();
   shaderCI.SourceLength = shaderCode.length();
   shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;

   createInfoMap[blendType] = shaderCI;
  }



  //device_->CreateShader(normalBlendShaderCI,&)

 }

 void OffscreenRenderer2D::Impl::clearCompositionColor(const FloatColor& color)
 {
  const float clearColor[] = { color.r(), color.g(), color.b(), color.a() };

  RefCntAutoPtr<ITextureView> pCompositionView;

  pCompositionView = compositionBuffer_->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

  mainDeviceContext_->ClearRenderTarget(pCompositionView, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 }

 void OffscreenRenderer2D::Impl::resize(int width, int height)
 {

 }

 void OffscreenRenderer2D::Impl::drawSprite(int width, int height)
 {
  auto rtv = layerRenderTarget_->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

  mainDeviceContext_->SetRenderTargets(1, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);



 }

 void OffscreenRenderer2D::Impl::blendLayer(LAYER_BLEND_TYPE type)
 {
  mainDeviceContext_->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);


  //imContext_->SetPipelineState(PostFX_PSO);

  //imContext_->DispatchCompute();


 }

 void OffscreenRenderer2D::Impl::renderStart()
 {

 }

 void OffscreenRenderer2D::Impl::renderFrame(double time)
 {

 }

 void OffscreenRenderer2D::Impl::drawSolidLayer(const FloatColor& color)
 {

 }

 void OffscreenRenderer2D::Impl::drawImage(float x, float y, const QImage& image)
 {

 }

 void OffscreenRenderer2D::Impl::drawImage(const Point2DF&, const QImage& image)
 {

 }

 OffscreenRenderer2D::OffscreenRenderer2D()
 {

 }

 OffscreenRenderer2D::OffscreenRenderer2D(const Size_2D& size) :impl_(new Impl(size))
 {

 }

 OffscreenRenderer2D::~OffscreenRenderer2D()
 {
  delete impl_;
 }

 void OffscreenRenderer2D::resize(int width, int height)
 {

 }

 void OffscreenRenderer2D::resize(const Size_2D& size)
 {

 }

 void OffscreenRenderer2D::renderStart()
 {

 }

 Renderer2DFactory::Renderer2DFactory()
 {

 }

 Renderer2DFactory::~Renderer2DFactory()
 {

 }

};
