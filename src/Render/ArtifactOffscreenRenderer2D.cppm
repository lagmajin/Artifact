module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tbb/tbb.h>
#include <QMap>
#include <QDebug>
#include <QThreadPool>
#include <boost/range/size.hpp>
#include <RefCntAutoPtr.hpp>
#include <Fence.h>
#include <DeviceContext.h>
#include <RenderDevice.h>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPointF>


#include <EngineFactoryD3D12.h>


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
module Artifact.Render.Offscreen;



import Core.Point2D;
import Image.Raw;
import Color.Float;
import Layer.Blend;

import Graphics.Shader.Compute.HLSL.Blend;
import Graphics.Resource.PSOAndSRB;



//#offscreen

namespace Artifact
{
 namespace {
  Diligent::IEngineFactoryD3D12* resolveD3D12Factory()
  {
#if D3D12_SUPPORTED
#if DILIGENT_D3D12_SHARED
   return Diligent::LoadAndGetEngineFactoryD3D12();
#else
   return Diligent::GetEngineFactoryD3D12();
#endif
#else
   return nullptr;
#endif
  }
 }
 


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

 namespace
 {
  QColor toQColor(const FloatColor& color)
  {
   return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
  }
 }


 class OffscreenRenderer2D::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> renderDevice_;
  RefCntAutoPtr<IDeviceContext> mainDeviceContext_;
  //RefCntAutoPtr<IDeviceContext> dfContext_;

  
  RefCntAutoPtr<ITexture>      compositionBuffer_;
  RefCntAutoPtr<ITexture>	   layerRenderTarget_;

  RefCntAutoPtr<IFence>		   blendFence_;
  RefCntAutoPtr<IShader>	   pixelShader_;
  RefCntAutoPtr<IShader>       m_shader_;
  QSize canvasSize_;
  QImage canvas_;
  double lastFrameTime_ = 0.0;

  //PSOAndSRB                    psor
  QMap<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> blendShaders_;
  QMap<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> layer_blend_pso_map;

  bool shader_compiled_ = false;
  bool pso_created = false;

  void createLayerBlendPSO();
  void createShaders();
  void createConstantBuffers();
  void ensureCanvas(int width, int height);
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

  void drawSolidRect(const FloatColor& color);
  void drawImage(float x, float y, const QImage& image);
  void drawImage(const Point2DF&,const QImage& image);
  void drawPoint(const Point2DF& point);
  //void drawImageLayer(const FloatImage& image);
 };

 OffscreenRenderer2D::Impl::Impl()
 {
  ensureCanvas(0, 0);
 }

 OffscreenRenderer2D::Impl::Impl(const Size_2D& size)
 {
  ensureCanvas(size.width, size.height);
 }

 OffscreenRenderer2D::Impl::~Impl()
 {

 }

 void OffscreenRenderer2D::Impl::initialize()
 {
  auto* pFactory = resolveD3D12Factory();
  if (!pFactory) {
   ensureCanvas(canvasSize_.width(), canvasSize_.height());
   return;
  }


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
  if (renderDevice_) {
   renderDevice_->CreateFence(fenceDesc, &blendFence_);
  }

  //blendFence_=device_->CreateFence()


  createShaders();
  createLayerBlendPSO();
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
 }

 void OffscreenRenderer2D::Impl::ensureCanvas(int width, int height)
 {
  const int safeWidth = std::max(0, width);
  const int safeHeight = std::max(0, height);
  if (safeWidth <= 0 || safeHeight <= 0) {
   canvas_ = QImage();
   canvasSize_ = QSize();
   return;
  }

  const QSize desiredSize(safeWidth, safeHeight);
  if (canvas_.isNull() || canvas_.size() != desiredSize || canvas_.format() != QImage::Format_ARGB32_Premultiplied) {
   canvas_ = QImage(desiredSize, QImage::Format_ARGB32_Premultiplied);
   canvas_.fill(Qt::transparent);
  }
  canvasSize_ = desiredSize;
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

   createInfoMap[toLegacyBlendType(blendType)] = shaderCI;
  }



  //device_->CreateShader(normalBlendShaderCI,&)

 }

 void OffscreenRenderer2D::Impl::clearCompositionColor(const FloatColor& color)
 {
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
  if (canvas_.isNull()) {
   return;
  }
  QPainter painter(&canvas_);
  painter.fillRect(canvas_.rect(), toQColor(color));
  painter.end();

  const float clearColor[] = { color.r(), color.g(), color.b(), color.a() };

  RefCntAutoPtr<ITextureView> pCompositionView;

  if (!compositionBuffer_) {
   return;
  }
  pCompositionView = compositionBuffer_->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  if (!pCompositionView) {
   return;
  }

  if (!mainDeviceContext_) {
   return;
  }
  mainDeviceContext_->ClearRenderTarget(pCompositionView, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 }

 void OffscreenRenderer2D::Impl::resize(int width, int height)
 {
  ensureCanvas(width, height);
  if (!renderDevice_ || width <= 0 || height <= 0) {
   return;
  }

  TextureDesc texDesc;
  texDesc.Type = RESOURCE_DIM_TEX_2D;
  texDesc.Width = static_cast<Uint32>(width);
  texDesc.Height = static_cast<Uint32>(height);
  texDesc.Format = TEX_FORMAT_RGBA8_UNORM;
  texDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
  texDesc.Usage = USAGE_DEFAULT;

  renderDevice_->CreateTexture(texDesc, nullptr, &compositionBuffer_);
  renderDevice_->CreateTexture(texDesc, nullptr, &layerRenderTarget_);
 }

 void OffscreenRenderer2D::Impl::drawSprite(int width, int height)
 {
  ensureCanvas(width, height);
  if (canvas_.isNull()) {
   return;
  }

  if (!mainDeviceContext_ || !layerRenderTarget_) {
   return;
  }

  auto rtv = layerRenderTarget_->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

  if (!rtv) {
    return;
  }
  mainDeviceContext_->SetRenderTargets(1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);



 }

 void OffscreenRenderer2D::Impl::blendLayer(LAYER_BLEND_TYPE type)
 {
  (void)type;
  if (!mainDeviceContext_) {
   return;
  }
  mainDeviceContext_->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);


  //imContext_->SetPipelineState(PostFX_PSO);

  //imContext_->DispatchCompute();


 }

 void OffscreenRenderer2D::Impl::renderStart()
 {
  if (!renderDevice_ || !mainDeviceContext_) {
   initialize();
  }
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
 }

 void OffscreenRenderer2D::Impl::renderFrame(double time)
 {
  lastFrameTime_ = time;
  if (canvasSize_.isEmpty()) {
   return;
  }
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
 }

 void OffscreenRenderer2D::Impl::drawSolidRect(const FloatColor& color)
 {
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
  if (canvas_.isNull()) {
   return;
  }
  QPainter painter(&canvas_);
  painter.fillRect(canvas_.rect(), toQColor(color));
  painter.end();
 }

 void OffscreenRenderer2D::Impl::drawImage(float x, float y, const QImage& image)
 {
  if (image.isNull()) {
   return;
  }
  if (canvasSize_.isEmpty()) {
   ensureCanvas(image.width(), image.height());
  } else {
   ensureCanvas(canvasSize_.width(), canvasSize_.height());
  }
  if (canvas_.isNull()) {
   return;
  }
  QPainter painter(&canvas_);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawImage(QPointF(x, y), image);
  painter.end();
 }

 void OffscreenRenderer2D::Impl::drawImage(const Point2DF& point, const QImage& image)
 {
  drawImage(point.getX(), point.getY(), image);
 }

 void OffscreenRenderer2D::Impl::drawPoint(const Point2DF& point)
 {
  ensureCanvas(canvasSize_.width(), canvasSize_.height());
  if (canvas_.isNull()) {
   return;
  }

  QPainter painter(&canvas_);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::white);
  painter.drawRect(QRectF(point.getX(), point.getY(), 1.0, 1.0));
  painter.end();
 }

 OffscreenRenderer2D::OffscreenRenderer2D()
  : impl_(new Impl())
 {
 }

 OffscreenRenderer2D::OffscreenRenderer2D(const Size_2D& size) :impl_(new Impl(size))
 {
  if (impl_) {
   impl_->resize(size.width, size.height);
  }
 }

 OffscreenRenderer2D::~OffscreenRenderer2D()
 {
  delete impl_;
  impl_ = nullptr;
 }

 void OffscreenRenderer2D::resize(int width, int height)
 {
  if (!impl_) {
   impl_ = new Impl();
  }
  impl_->resize(width, height);
 }

 void OffscreenRenderer2D::resize(const Size_2D& size)
 {
  resize(size.width, size.height);
 }

 void OffscreenRenderer2D::renderStart()
 {
  if (impl_) {
   impl_->renderStart();
  }
 }

 void OffscreenRenderer2D::setImageWriterPool()
 {
  // Placeholder hook: future writer-pool integration can be wired here.
 }

 void OffscreenRenderer2D::addLayer()
 {
  if (!impl_) {
   impl_ = new Impl();
  }
  // Placeholder hook for future layer registration.
 }

 Renderer2DFactory::Renderer2DFactory()
 {

 }

 Renderer2DFactory::~Renderer2DFactory()
 {

 }

};
