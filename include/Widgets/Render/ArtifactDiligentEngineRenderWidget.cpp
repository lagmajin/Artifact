module;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RIGHT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <QWidget>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <opencv2/opencv.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentTools/RenderStateNotation/interface/RenderStateNotationParser.h>
#include <DiligentTools/TextureLoader/interface/Image.h>
#include <DiligentTools/Imgui/interface/ImGuiDiligentRenderer.hpp>

#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <QTimer>
#include <QBoxLayout>

#include <wobjectimpl.h>
#include <QClipboard>
#include <vulkan/vulkan_core.h>
#include <roapi.h>
#include "qevent.h"
#ifdef Q_OS_WIN
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/base.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
#endif
#include <wrl/client.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>



//#include <algorithm>

module Widgets.Render.Composition;

import Graphics;
import Color.Float;
import Graphics.Func;
import Graphics.CBuffer.Constants.Helper;
import Graphics.Shader.Compute.HLSL.Blend;
import Size;

import std;

import Core.Scale.Zoom;

import Layer.Blend;

import Graphics.Shader.HLSL.Basics.Vertex;

namespace Artifact {

 using namespace Diligent;
 using namespace ArtifactCore;
 using namespace winrt;
#ifdef Q_OS_WIN
 //using namespace Windows::Graphics::Capture;
 //using namespace Windows::Foundation;
 using namespace winrt;
 using Microsoft::WRL::ComPtr;
#endif
 struct BlendResources
 {
  RefCntAutoPtr<IPipelineState> pPSO;
  RefCntAutoPtr<IShaderResourceBinding> pSRB;
 };

 W_OBJECT_IMPL(ArtifactDiligentEngineComposition2DWindow)

  class ArtifactDiligentEngineComposition2DWindow::Impl {
  private:
   FloatColor canvasColor_;
   float scale_ = 1.0f;
   ZoomScale2D zoomScale_;


   RefCntAutoPtr<IRenderDevice> pDevice;
   RefCntAutoPtr<IDeviceContext> pImmediateContext;
   RefCntAutoPtr<ISwapChain> pSwapChain_;

   RefCntAutoPtr<IPipelineState> pTEST_2D_PSO_;
   RefCntAutoPtr<IPipelineState> pLine_PSO_;
   RefCntAutoPtr<IPipelineState> pSprite_PSO_;

   RefCntAutoPtr<IShader> p2D_pixel;
   RefCntAutoPtr<IShader> p2D_vertex_shader;

   RenderShaderPair m_draw_test_shaders;
   RenderShaderPair m_draw_line_shaders;
   RenderShaderPair m_draw_solid_shaders;
   RenderShaderPair m_draw_sprite_shaders;




   //RefCntAutoPtr<IShader> p
   ComPtr<ID3D11Device> d3d11Device_;
   ComPtr<ID3D11DeviceContext> d3d11Context_;
   ComPtr<ID3D11On12Device> d3d11On12Device_;

   RefCntAutoPtr<IBuffer>       pConstantsBuffer;
   RefCntAutoPtr<IBuffer>		pBuffer;


   RefCntAutoPtr<IBuffer>        p2D_VBuffer_;
   RefCntAutoPtr<IBuffer>        p2D_VFrastumBuffer_;
   RefCntAutoPtr<IBuffer>		 p2D_VLineBuffer_;
   RefCntAutoPtr<IBuffer>		 p2D_draw_solid_color_constants;

   RefCntAutoPtr<IBuffer>		 m_pDrawSpriteVertexBuffer;

   RefCntAutoPtr<IShaderResourceBinding> p2D_SRB_;
   RefCntAutoPtr<IShaderResourceBinding> p2D_LINE_SRB_;
   RefCntAutoPtr<IShaderResourceBinding> p2D_draw_sprite_srb;


   RefCntAutoPtr<ITextureView> p2D_TextureView;
   RefCntAutoPtr<ITexture> p2D_Texture;

   RefCntAutoPtr<ITexture> compositeRenderTarget;
   //float4x4 projectionMatrix_;
   std::vector<ShaderResourceVariableDesc> m_ResourceVars;
   std::vector<ImmutableSamplerDesc> m_sampler_;

   std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> m_blendShaderMap;
   //std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> m_blendPSOMap;

   std::map<LAYER_BLEND_TYPE, BlendResources> m_BlendMap;

   QWidget* widget_ = nullptr;
   bool m_initialized = false;
   int m_CurrentPhysicalWidth;
   int m_CurrentPhysicalHeight;

   glm::mat4 glm_projection_;
   glm::mat4 view_;
   qreal m_CurrentDevicePixelRatio;



   glm::mat4 calculateModelMatrixGLM(const glm::vec2& position,      // 画面上の最終的なピクセル位置
	const glm::vec2& size,          // レイヤーの元のピクセルサイズ
	const glm::vec2& anchorPoint,   // ローカル正規化座標 (0.0-1.0) でのアンカーポイント
	float rotationDegrees,          // 回転角度 (度数法)
	const glm::vec2& scale);
   std::mutex g_eventMutex;
   std::queue<std::function<void()>> g_renderEvents;
   bool takeScreenshot = false;

   bool panning_ = false;
   const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;
   void initializeResources();
   void createShaders();
   void createBlendShaders();
   void createConstantBuffers();
   void createPSO();
   void createBlendShaderAndPSOs();
   void calcProjection(int width, int height);

  public:
   Impl();
   ~Impl();
   void initialize(QWidget* window);
   void initializeImGui(QWidget* window);
   void destroy();
   void recreateSwapChain(QWidget* window);
   void resizeComposition(const Size_2D& size);
   void clearCanvas(const Diligent::float4& clearColor);
   void clearComposition(const FloatColor& color);
   void present();
   void renderOneFrame();
   void drawSprite(float x, float y, float w, float h, const QImage& sprite);
   void drawSolidQuadToCompositionOld(float x, float y, float w, float h);
   void drawSolidQuadToComposition(float x, float y, float w, float h);
   void drawQuadLine(float x, float y, float w, float h, const FloatColor& lineColor, float thikness = 1.0f);
   void drawLineCanvas(float x_1, float y_1, float x_2, float y_2, const FloatColor& color, float tick = 1.0f);
   void drawViewCameraFrustum();
   void drawTextInCanvas(const QString& string);

   void zoomIn();
   void zoomOut();
   void setCanvasColor(const FloatColor& color);
   void postScreenShotEvent();
   void saveScreenShotToClipboard();
   void saveScreenShotToClipboardByQt();
   void saveScreenShotToClipboardByWinRT();

   void hit();
 };


 ArtifactDiligentEngineComposition2DWindow::Impl::~Impl()
 {
  // d3d11Context_->Release();
   //d3d11Device_->Release();

  RoUninitialize();
 }

 ArtifactDiligentEngineComposition2DWindow::Impl::Impl()
 {
  Diligent::SamplerDesc SamplerStateDesc;
  // サンプラーのプロパティを具体的に設定
  SamplerStateDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
  SamplerStateDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
  SamplerStateDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
  SamplerStateDesc.AddressU = Diligent::TEXTURE_ADDRESS_WRAP;
  SamplerStateDesc.AddressV = Diligent::TEXTURE_ADDRESS_WRAP;
  SamplerStateDesc.AddressW = Diligent::TEXTURE_ADDRESS_WRAP;
  SamplerStateDesc.ComparisonFunc = Diligent::COMPARISON_FUNC_ALWAYS;
  SamplerStateDesc.MaxAnisotropy = 1;
  SamplerStateDesc.MipLODBias = 0.0f;
  SamplerStateDesc.MinLOD = 0.0f;
  SamplerStateDesc.MaxLOD = FLT_MAX;

  // ImmutableSamplerDesc を作成し、ベクターに追加
  // 第一引数がシェーダータイプ、第二引数が変数名、第三引数が SamplerDesc
  m_sampler_.push_back(
   Diligent::ImmutableSamplerDesc{
	   Diligent::SHADER_TYPE_PIXEL,
	   "g_sampler",
	   SamplerStateDesc
   }
  );

  RoInitialize(RO_INIT_MULTITHREADED);

 }
 void ArtifactDiligentEngineComposition2DWindow::Impl::initialize(QWidget* window)
 {
  widget_ = window;
  view_ = CreateInitialViewMatrix();

  auto* pFactory = GetEngineFactoryD3D12();


  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;

  // ウィンドウハンドルを設定
  Win32NativeWindow hWindow;
  hWindow.hWnd = reinterpret_cast<HWND>(window->winId());
  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice, &pImmediateContext);


  if (!pDevice)
  {
   // エラーログ出力、アプリケーション終了などの処理
   qWarning() << "Failed to create Diligent Engine device and contexts.";
   return;
  }
  m_CurrentPhysicalWidth = static_cast<int>(window->width() * window->devicePixelRatio());
  m_CurrentPhysicalHeight = static_cast<int>(window->height() * window->devicePixelRatio());
  m_CurrentDevicePixelRatio = window->devicePixelRatio();
  // スワップチェインを作成
  SwapChainDesc SCDesc;
  SCDesc.Width = m_CurrentPhysicalWidth;  // QWindowの現在の幅
  SCDesc.Height = m_CurrentPhysicalHeight; // QWindowの現在の高さ
  SCDesc.ColorBufferFormat = MAIN_RTV_FORMAT;
  SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;

  SCDesc.BufferCount = 2;
  SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

  FullScreenModeDesc desc;

  desc.Fullscreen = false;

  pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, hWindow, &pSwapChain_);

  Diligent::Viewport VP;
  VP.Width = static_cast<float>(m_CurrentPhysicalWidth);
  VP.Height = static_cast<float>(m_CurrentPhysicalHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  // SetViewportsの最後の2引数は、レンダーターゲットの物理ピクセルサイズを渡すのが安全
  pImmediateContext->SetViewports(1, &VP, m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);




  // GLMの行列をDiligent Engineのfloat4x4に変換
  //projectionMatrix_ = GLMMat4ToDiligentFloat4x4(glm_projection_);

  //m_ResourceVars.push_back({ Diligent::SHADER_TYPE_PIXEL, "g_texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE });
  m_ResourceVars.push_back({ Diligent::SHADER_TYPE_VERTEX, "Constants", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE });
  // g_sampler は静的変数、またはイミュータブルサンプラーとして扱う。
  // ここではイミュータブルサンプラーとして定義するのが一般的かつ効率的です。
  // イミュータブルサンプラーはResourceLayout.ImmutableSamplersに設定します。
  // そのため、ResourceVarsには含めません。
  // もしイミュータブルサンプラーを使わない場合は、以下のようにSTATICでResourceVarsに含めます:
  // m_ResourceVars.push_back({Diligent::SHADER_TYPE_PIXEL, "g_sampler", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC});
  auto hwnd = reinterpret_cast<HWND>(widget_->winId());


  /*
  ID3D12Device* d3d12Device;

  UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  D3D_FEATURE_LEVEL featureLevels[] = {
D3D_FEATURE_LEVEL_11_1,
D3D_FEATURE_LEVEL_11_0,
  };

  auto renderDevice12 = static_cast<IRenderDeviceD3D12*>(pDevice.RawPtr());


  d3d12Device = renderDevice12->GetD3D12Device();
  */

  /*
HRESULT hr = D3D11On12CreateDevice(
 d3d12Device,
 d3d11DeviceFlags,
 featureLevels,
 _countof(featureLevels),
 nullptr,              // 共有する D3D12 コマンドキュー配列
 0,    // キュー数
 0,                     // ノードマスク（通常は0）
 &d3d11Device_,
 &d3d11Context_,
 nullptr                // 実際に使われた feature level を受け取るならここにポインタ
);


d3d11Device_.As(&d3d11On12Device_);


if (FAILED(hr)) {
 qDebug() << "D3D11 Error";
}

*/

  TextureDesc compositionDesc;

  compositionDesc.Width = 1;
  compositionDesc.Height = 1;
  //compositionDesc.Format=


	//pDevice->CreateTexture(compositionDesc,nullptr,)

  createShaders();
  //createBlendShaders();
  createPSO();
  createBlendShaderAndPSOs();
  initializeResources();
  createConstantBuffers();

  m_initialized = true;
 }
 void ArtifactDiligentEngineComposition2DWindow::Impl::createConstantBuffers()
 {
  BufferDesc CBDesc;
  CBDesc.Name = "My Constant Buffer";
  //CBDesc.Size = sizeof(MyConstantBufferData);
  CBDesc.Usage = USAGE_DYNAMIC;
  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;  // ← これが重要
  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

  BufferData CBData;
  CBData.pData = nullptr;
  CBData.DataSize = 0;

  //RefCntAutoPtr<IBuffer> pConstantBuffer;
  //pDevice->CreateBuffer(CBDesc, &CBData, &);


 // BufferDesc CBClearColorDesc;
  //CBClearColorDesc.Name = "ClearColorDesc";
  //CBClearColorDesc.Usage = USAGE_DYNAMIC;
  //CBClearColorDesc.BindFlags = BIND_UNIFORM_BUFFER;  // ← これが重要
  //CBClearColorDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

  //pDevice->CreateBuffer(CBClearColorDesc, nullptr, &p2D_canvas_clear_color_buffer);

  CBSolidColor colorCBInitialData{ {0.0f, 0.0f, 0.0f, 1.0f} };



  p2D_draw_solid_color_constants = CreateConstantBuffer(pDevice, sizeof(CBSolidColor), nullptr, "ClearColorBuffer");

  BufferDesc CBDesc2;
  CBDesc2.Name = "SpriteCB";
  CBDesc2.Size = sizeof(Vertex);
  CBDesc2.Usage = USAGE_DYNAMIC;
  CBDesc2.BindFlags = BIND_VERTEX_BUFFER;
  CBDesc2.CPUAccessFlags = CPU_ACCESS_WRITE;

  BufferData CBData2;
  CBData2.pData = nullptr;
  CBData2.DataSize = 0;

  pDevice->CreateBuffer(CBDesc2, &CBData2, &m_pDrawSpriteVertexBuffer);



  qDebug() << "Buffer created";
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::initializeResources()
 {
  Vertex vertices[] =
  {
   // 左上 → 左下 → 右上 → 右下（TRIANGLE_STRIP）
   {{0.0f, 0.0f}, {0.0f, 0.0f}},  // 左上
   {{0.0f, 1.0f}, {0.0f, 1.0f}},  // 左下
   {{1.0f, 0.0f}, {1.0f, 0.0f}},  // 右上
   {{1.0f, 1.0f}, {1.0f, 1.0f}},  // 右下
  };
  Diligent::BufferDesc VertBuffDesc;
  VertBuffDesc.Name = "2D Quad Vertex Buffer";
  VertBuffDesc.Usage = Diligent::USAGE::USAGE_IMMUTABLE;       // 頻繁に更新するならDYNAMIC
  // または USAGE_DEFAULT (一度設定したらあまり変更しないが、UpdateBuffer()で後から変更する可能性)
  VertBuffDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
  VertBuffDesc.Size = sizeof(Vertex) * 4;            // 4頂点分のメモリを確保 (Vertex構造体は事前に定義)
  //VertBuffDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
  Diligent::BufferData InitialData;
  InitialData.pData = vertices;
  InitialData.DataSize = sizeof(vertices);
  pDevice->CreateBuffer(VertBuffDesc, &InitialData, &p2D_VBuffer_);

  if (!p2D_VBuffer_)
  {
   // エラー処理
   std::cerr << "Error: Failed to create vertex buffer!" << std::endl;
   return;
  }

  Diligent::BufferDesc lineVertBuffDesc;
  lineVertBuffDesc.Name = "2D Quad Vertex Buffer";




 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::initializeImGui(QWidget* window)
 {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  (void)io;


 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::recreateSwapChain(QWidget* window)
 {
  if (!window || !pDevice)
  {

   return;
  }


  //pSwapChain->Release();
  const int newWidth = static_cast<int>(window->width() * window->devicePixelRatio());
  const int newHeight = static_cast<int>(window->height() * window->devicePixelRatio());
  const float newDevicePixelRatio = window->devicePixelRatio();
  qDebug() << "Impl::recreateSwapChain - Logical:" << window->width() << "x" << window->height()
   << ", DPI:" << newDevicePixelRatio
   << ", Physical:" << newWidth << "x" << newHeight;
  qDebug() << "Before Resize - SwapChain Desc:" << pSwapChain_->GetDesc().Width << "x" << pSwapChain_->GetDesc().Height;
  pSwapChain_->Resize(newWidth, newHeight);


  qDebug() << "After Resize - SwapChain Desc:" << pSwapChain_->GetDesc().Width << "x" << pSwapChain_->GetDesc().Height;

  Diligent::Viewport VP;
  VP.Width = static_cast<float>(newWidth);  // newWidth, newHeightは物理ピクセル
  VP.Height = static_cast<float>(newHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  pImmediateContext->SetViewports(1, &VP, newWidth, newHeight);

  qDebug() << "After SetViewports - Viewport WxH: " << VP.Width << "x" << VP.Height;
  qDebug() << "After SetViewports - Viewport TopLeftXY: " << VP.TopLeftX << ", " << VP.TopLeftY;

  calcProjection(m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::createShaders()
 {
  ShaderCreateInfo testShaderCI;
  testShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  testShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  testShaderCI.Desc.Name = "TestPixelShader";
  testShaderCI.EntryPoint = "main";
  testShaderCI.Source = g_qsSolidColorPS2.constData();
  testShaderCI.SourceLength = g_qsSolidColorPS2.length();


  pDevice->CreateShader(testShaderCI, &m_draw_test_shaders.PS);

  ShaderCreateInfo drawSolidShaderCI;
  drawSolidShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  drawSolidShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  drawSolidShaderCI.EntryPoint = "main";
  drawSolidShaderCI.Desc.Name = "BasicPixelShader";
  drawSolidShaderCI.Source = g_qsSolidColorPSSource.constData();
  drawSolidShaderCI.SourceLength = g_qsSolidColorPSSource.length();

  pDevice->CreateShader(drawSolidShaderCI, &p2D_pixel);


  ShaderCreateInfo BasicVertexShaderCI;
  BasicVertexShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  BasicVertexShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  BasicVertexShaderCI.EntryPoint = "main";
  BasicVertexShaderCI.Desc.Name = "CompositionVertexShader";
  BasicVertexShaderCI.Source = g_qsBasic2DVS.constData();
  BasicVertexShaderCI.SourceLength = g_qsBasic2DVS.length();

  pDevice->CreateShader(BasicVertexShaderCI, &p2D_vertex_shader);

  ShaderCreateInfo lineVsInfo;

  lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  lineVsInfo.EntryPoint = "main";
  lineVsInfo.Desc.Name = "CompositionVertexShader";
  lineVsInfo.Source = lineShaderVSText.constData();
  lineVsInfo.SourceLength = lineShaderVSText.length();

  pDevice->CreateShader(lineVsInfo, &m_draw_line_shaders.VS);

  ShaderCreateInfo linePsInfo;

  linePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  linePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  linePsInfo.Desc.Name = "MyPixelShader";
  linePsInfo.Source = g_qsSolidColorPS2.constData();
  lineVsInfo.SourceLength = g_qsSolidColorPS2.length();

  pDevice->CreateShader(linePsInfo, &m_draw_line_shaders.PS);//#fix

  ShaderCreateInfo psSolidInfo;

  ShaderCreateInfo vsSoildShaderInfo;




  ShaderCreateInfo sprite2DVsInfo;
  sprite2DVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  sprite2DVsInfo.Desc.Name = "SpriteShader";

  sprite2DVsInfo.Source = g_qsBasic2DVS.constData();
  sprite2DVsInfo.SourceLength = g_qsBasic2DVS.length();

  pDevice->CreateShader(sprite2DVsInfo, &m_draw_sprite_shaders.VS);


  ShaderCreateInfo sprite2DPsInfo;
  sprite2DPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  sprite2DPsInfo.Desc.Name = "SpriteShader";

  sprite2DPsInfo.Source = g_qsBasicSprite2DImagePS.constData();
  sprite2DPsInfo.SourceLength = g_qsBasicSprite2DImagePS.length();

  pDevice->CreateShader(sprite2DPsInfo, &m_draw_sprite_shaders.PS);

  Diligent::BufferDesc CBDesc;
  CBDesc.Name = "Constants CB";              // バッファの名前（デバッグ用）
  CBDesc.Usage = Diligent::USAGE_DYNAMIC;     // CPUから頻繁に更新されるためDYNAMIC
  CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER; // HLSLの cbuffer に対応 (DirectX系ではUNIFORM_BUFFER)
  CBDesc.Size = sizeof(Constants);          // Constants 構造体のサイズ


  CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;  // 
  pDevice->CreateBuffer(CBDesc, nullptr, &pConstantsBuffer);
  if (!pConstantsBuffer)
  {
   // エラー処理: 定数バッファの作成に失敗しました
   // 例: qCritical() << "Failed to create constants buffer!";
  }

  if (!p2D_SRB_) { /* エラー処理 */ return; }

  // p2D_SRB_ が有効であることを確認した上で、GetVariableByName を呼び出す
  IShaderResourceVariable* pVSConstantsVar = p2D_SRB_->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants");
  if (pVSConstantsVar) // ここで nullptr チェック
  {
   pVSConstantsVar->Set(pConstantsBuffer);
   pImmediateContext->CommitShaderResources(p2D_SRB_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
  else
  {
   // ここに到達する場合、"Constants" 変数が見つかっていない
   // デバッグ出力やブレークポイントで確認
   std::cerr << "Error: Vertex Shader variable 'Constants' not found in SRB!" << std::endl;
  }

  //Shader



  calcProjection(m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::createPSO()
 {
  std::vector<Uint8> TexData(4); // RGBA で4バイト
  TexData[0] = 255; // R
  TexData[1] = 0;   // G
  TexData[2] = 0;   // B
  TexData[3] = 255; // A (不透明)

  // --- 2. Diligent Engine の TextureDesc を定義 ---
  Diligent::TextureDesc TexDesc;
  TexDesc.Name = "Red 1x1 Texture"; // デバッグ用の名前
  TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D; // 2Dテクスチャ
  TexDesc.Width = 1; // 幅
  TexDesc.Height = 1; // 高さ
  TexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM; // RGBA 8ビット符号なし正規化形式
  TexDesc.Usage = Diligent::USAGE_IMMUTABLE; // 作成後にデータは変更しない
  TexDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE; // シェーダーから読み取り可能にする

  // --- 3. 初期テクスチャデータ (TextureData) を準備 ---
  TextureData InitialTexData;
  TextureSubResData subResData;
  subResData.pData = TexData.data(); // ピクセルデータへのポインタ
  subResData.Stride = TexDesc.Width * 4; // 1行あたりのバイト数 (1ピクセル4バイト)
  InitialTexData.pSubResources = &subResData;
  InitialTexData.NumSubresources = 1; // ミップレベルは1つ（ミップマップなし）

  // --- 4. IDevice::CreateTexture() でテクスチャを作成 ---
  // この p2D_Texture は Impl クラスのメンバ変数として宣言してください
  // Diligent::RefCntAutoPtr<Diligent::ITexture> p2D_Texture;
  pDevice->CreateTexture(TexDesc, &InitialTexData, &p2D_Texture);
  if (!p2D_Texture)
  {
   std::cerr << "Error: Failed to create texture!" << std::endl;
   return;
  }

  // --- 5. テクスチャビュー (ITextureView) の作成 ---
  // この p2D_TextureView も Impl クラスのメンバ変数として宣言してください
  // Diligent::RefCntAutoPtr<Diligent::ITextureView> p2D_TextureView;
  p2D_TextureView = p2D_Texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  if (!p2D_TextureView)
  {
   std::cerr << "Error: Failed to get default texture view!" << std::endl;
   return;
  }



  auto testPsoInfo = create2DPSOHelper();
  {
   // Define vertex shader input layout
   LayoutElement LayoutElems[] =
   {
   Diligent::LayoutElement{0, 0, 2, VT_FLOAT32, False, 0},
	Diligent::LayoutElement{1, 0, 2, VT_FLOAT32, False, sizeof(float2)}
   };
   testPsoInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
   testPsoInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;

   testPsoInfo.pVS = p2D_vertex_shader;
   testPsoInfo.pPS = m_draw_test_shaders.PS;
   testPsoInfo.PSODesc.ResourceLayout.Variables = m_ResourceVars.data();
   testPsoInfo.PSODesc.ResourceLayout.NumVariables = static_cast<Uint32>(m_ResourceVars.size());

   testPsoInfo.PSODesc.ResourceLayout.ImmutableSamplers = m_sampler_.data();
   testPsoInfo.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Uint32>(m_sampler_.size());
   pDevice->CreateGraphicsPipelineState(testPsoInfo, &pTEST_2D_PSO_);

   pTEST_2D_PSO_->CreateShaderResourceBinding(&p2D_SRB_, false);
   if (!p2D_SRB_)
   {
	// エラー処理（例: qCritical() << "Failed to create 2D Shader Resource Binding!";）
	return;
   }
  }

  auto linePSOInfo = createDrawLinePSOHelper();
  linePSOInfo.pVS = m_draw_line_shaders.VS;
  linePSOInfo.pPS = m_draw_line_shaders.PS;






  if (pLine_PSO_)
  {

  }

  //IPipelineStateCache

  auto solidPSOInfo = create2DPSOHelper();

  auto drawSpritePSOInfo = create2DPSOHelper();
  drawSpritePSOInfo.PSODesc.Name = "DrawSpritePSO";

  drawSpritePSOInfo.pPS = m_draw_sprite_shaders.PS;
  drawSpritePSOInfo.pVS = m_draw_sprite_shaders.VS;
  drawSpritePSOInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
  drawSpritePSOInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
  LayoutElement VertexLayout[] =
  {
	  {0, 0, 2, VT_FLOAT32, False}, // position
	  {1, 0, 2, VT_FLOAT32, False}  // texCoord
  };

  drawSpritePSOInfo.GraphicsPipeline.InputLayout.LayoutElements = VertexLayout;
  drawSpritePSOInfo.GraphicsPipeline.InputLayout.NumElements = _countof(VertexLayout);

  drawSpritePSOInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

  drawSpritePSOInfo.GraphicsPipeline.NumRenderTargets = 1;
  drawSpritePSOInfo.GraphicsPipeline.RTVFormats[0] = MAIN_RTV_FORMAT;
  drawSpritePSOInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;

  pDevice->CreatePipelineState(drawSpritePSOInfo, &pSprite_PSO_);
  if (!pSprite_PSO_)
  {
   // ここでログ出力
   std::cerr << "PSO creation failed! HRESULT=" << std::hex << std::endl;
  }

  pSprite_PSO_->CreateShaderResourceBinding(&p2D_draw_sprite_srb, false);

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::createBlendShaders()
 {
  //std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> BlendShaderObjects;



 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::createBlendShaderAndPSOs()
 {

  for (const auto& [type, shaderText] : ArtifactCore::BlendShaders)
  {
   BlendResources res;

   // 1. Compute Shader作成
   ShaderCreateInfo ShaderCI;
   ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
   ShaderCI.EntryPoint = "main";
   ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
   ShaderCI.Desc.Name = "BlendShader";
   ShaderCI.Source = shaderText.constData();

   RefCntAutoPtr<IShader> pShader;
   pDevice->CreateShader(ShaderCI, &pShader);
   if (!pShader)
   {
	std::cerr << "Failed to create shader for blend type: " << static_cast<int>(type) << std::endl;
	continue;
   }

   // 2. Compute PSO作成
   ComputePipelineStateCreateInfo PSOCreateInfo;
   PSOCreateInfo.pCS = pShader;
   RefCntAutoPtr<IPipelineState> pComputePSO;
   pDevice->CreateComputePipelineState(PSOCreateInfo, &pComputePSO);
   if (!pComputePSO)
   {
	std::cerr << "Failed to create Compute PSO for blend type: " << static_cast<int>(type) << std::endl;
	continue;
   }

   res.pPSO = pComputePSO;

   // 3. SRB作成
   pComputePSO->CreateShaderResourceBinding(&res.pSRB, false);
   if (!res.pSRB)
   {
	std::cerr << "Failed to create SRB for blend type: " << static_cast<int>(type) << std::endl;
	continue;
   }

   // 4. Mapに格納
   m_BlendMap[type] = res;
  }


 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::present()
 {
  if (pSwapChain_)
  {

   pSwapChain_->Present(1);
  }
 }
 //#RenderLoop
 void ArtifactDiligentEngineComposition2DWindow::Impl::renderOneFrame()
 {
  if (!pSwapChain_) return;

  const Diligent::float4& clearColor = { 0.5f,0.5f,0.5f,1.0f };
  clearCanvas(clearColor);

  //QImage img(600,400,QImage::)

  //drawSprite(0, 0, 0, 0,);

  {
   std::lock_guard<std::mutex> lock(g_eventMutex);
   while (!g_renderEvents.empty()) {
	auto ev = std::move(g_renderEvents.front());
	g_renderEvents.pop();

	lock.~lock_guard();  // 明示的にロックを外す
	ev();                // イベント実行（スクショ保存）

	new (&lock) std::lock_guard<std::mutex>(g_eventMutex);  // 再ロック
   }
  }

  present();


 }

 //#DrawQuad
 void ArtifactDiligentEngineComposition2DWindow::Impl::drawSolidQuadToCompositionOld(float x, float y, float w, float h)
 {

  glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
  glm::mat4 translate = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
  glm::mat4 model = translate * scale;

  // View行列は単位行列（使わない）
  view_ = glm::mat4(1.0f);

  Constants constantsData;

  memcpy(&constantsData.ModelMatrix, glm::value_ptr(model), sizeof(glm::mat4));
  memcpy(&constantsData.ViewMatrix, glm::value_ptr(view_), sizeof(glm::mat4));
  memcpy(&constantsData.ProjectionMatrix, glm::value_ptr(glm_projection_), sizeof(glm::mat4));




  void* pMappedData = nullptr;
  pImmediateContext->MapBuffer(pConstantsBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pMappedData);
  if (!pMappedData)
  {
   // マップに失敗した場合の処理
   qCritical() << "Failed to map constants buffer!";
   return;
  }

  memcpy(pMappedData, &constantsData, sizeof(Constants));
  pImmediateContext->UnmapBuffer(pConstantsBuffer, Diligent::MAP_WRITE);

  pImmediateContext->SetPipelineState(pTEST_2D_PSO_);

  if (p2D_SRB_)
  {
   auto p = p2D_SRB_->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants");

   p->Set(pConstantsBuffer);

   //pImmediateContext->CommitShaderResources(p2D_SRB_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }

  /*
  if (p2D_SRB_)
  {
   IShaderResourceVariable* pPSTextureVar = p2D_SRB_->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_texture");
   if (pPSTextureVar)
   {
	// ここで作成したテクスチャビューをSetします
	// 例えば、m_pTextureView が有効なテクスチャビューだと仮定
	//pPSTextureVar->Set(this->p2D_TextureView); // ★ここにテクスチャビューをバインド！
   }
   else
   {
	std::cerr << "ERROR: Pixel Shader variable 'g_texture' not found in SRB!" << std::endl;
   }

  }
  */
  RefCntAutoPtr<IFence> pFence;

  Uint64 fenceValue = 0;
  ++fenceValue;

  FenceDesc fenceDesc;
  fenceDesc.Name = "ReadbackSyncFence";
  fenceDesc.Type = FENCE_TYPE_GENERAL;
  //pDevice->CreateFence(fenceDesc, &pFence);


  Diligent::Uint64 offset = 0;
  Diligent::IBuffer* pBuffers[] = { p2D_VBuffer_.RawPtr() };

  pImmediateContext->SetVertexBuffers(0,    // 開始スロット (PSOのInputLayoutと一致させる)
   1,    // バインドするバッファの数
   pBuffers, // バッファの配列
   &offset, // オフセットの配列 (各バッファの開始オフセット)
   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION // リソースの状態遷移
  );
  pImmediateContext->CommitShaderResources(p2D_SRB_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::DrawAttribs DrawAttrs;
  DrawAttrs.NumVertices = 4;
  DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

  pImmediateContext->Draw(DrawAttrs);
  //pImmediateContext->EnqueueSignal(pFence, fenceValue);

  //pImmediateContext->Flush();
  //pImmediateContext->DeviceWaitForFence(pFence, fenceValue);
  ++fenceValue;
  //pImmediateContext->Flush();
 }



 void ArtifactDiligentEngineComposition2DWindow::Impl::zoomIn()
 {

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::zoomOut()
 {

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::calcProjection(int width, int height)
 {
  glm_projection_ = glm::orthoRH_ZO(
   0.0f,          // left (X軸の開始)
   (float)width,   // right (X軸の終了)
   (float)height,  // bottom (Y軸の開始 - DirectXはY軸下向きが正なので、大きい値が下)
   0.0f,          // top (Y軸の終了 - DirectXはY軸下向きが正なので、小さい値が上)
   0.0f,          // zNear (ニアクリップ面 - カメラからの近距離)
   100.0f         // zFar (ファークリップ面 - カメラからの遠距離)
  );
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::setCanvasColor(const FloatColor& color)
 {
  canvasColor_ = color;
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawViewCameraFrustum()
 {

  // 1. クリップ空間の8頂点（NDCの立方体のコーナー）を定義
  float3 ndcCorners[8] = {
	  {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0},    // near plane (z=0)
	  {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}     // far plane  (z=1)
  };




 }
 //#screenshot
 void ArtifactDiligentEngineComposition2DWindow::Impl::saveScreenShotToClipboard()
 {


  RefCntAutoPtr<ITextureView> pRTV;
  pRTV = pSwapChain_->GetCurrentBackBufferRTV();

  RefCntAutoPtr<ITexture> pBackBuffer;
  pBackBuffer = pRTV->GetTexture();

  const auto& desc = this->pSwapChain_->GetDesc();
  //qDebug() << "Texture format:" << desc.Format;
  qDebug() << "Current State:" << pBackBuffer->GetState();
  TextureDesc ReadableDesc;
  ReadableDesc.Width = desc.Width;
  ReadableDesc.Height = desc.Height;
  ReadableDesc.Name = "ScreenCapture staging";
  ReadableDesc.Type = RESOURCE_DIM_TEX_2D;
  ReadableDesc.BindFlags = BIND_NONE;
  ReadableDesc.Usage = USAGE_STAGING;
  //ReadableDesc.
  ReadableDesc.CPUAccessFlags = CPU_ACCESS_READ;
  ReadableDesc.Format = desc.ColorBufferFormat;

  RefCntAutoPtr<ITexture> pReadableTex;
  pDevice->CreateTexture(ReadableDesc, nullptr, &pReadableTex);

  //pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  RefCntAutoPtr<IFence> pFence;

  int fenceValue = 0;
  int currentFenceValue = fenceValue++;
  FenceDesc fenceDesc;
  fenceDesc.Name = "ReadbackSyncFence";
  fenceDesc.Type = FENCE_TYPE_GENERAL;
  pDevice->CreateFence(fenceDesc, &pFence);





  CopyTextureAttribs CopyAttribs(pBackBuffer, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, pReadableTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pImmediateContext->CopyTexture(CopyAttribs);
  //std::this_thread::sleep_for(std::chrono::milliseconds(50));

  //pImmediateContext->WaitForIdle();


  ++currentFenceValue;
  pImmediateContext->EnqueueSignal(pFence, currentFenceValue);
  pImmediateContext->Flush();                // ここ大事
  pImmediateContext->DeviceWaitForFence(pFence, currentFenceValue);

  pFence->Wait(currentFenceValue);

  qDebug() << "Test:" << pFence->GetCompletedValue();

  MappedTextureSubresource MappedData{};
  pImmediateContext->MapTextureSubresource(
   pReadableTex,
   0,              // mip level
   0,              // array slice
   MAP_READ,
   MAP_FLAG_NONE,
   nullptr,        // 全面をマップ
   MappedData      // 参照で渡す
  );
  if (MappedData.pData == nullptr)
  {
   qWarning() << "MapTexture Subresource returned null data pointer";
   return;
  }




  // 画像サイズなど
  const auto& desc2 = pReadableTex->GetDesc();
  int width = desc2.Width;
  int height = desc2.Height;
  int bytesPerPixel = 4; // RGBA8_UNORM想定
  int rowStride = MappedData.Stride;

  // QtのQImageを用意（RGBA8888）
  QImage image(width, height, QImage::Format_RGBA8888);

  // DirectX系テクスチャは上下反転していることが多いので上下反転コピー
  for (int y = 0; y < height; ++y)
  {
   const uint8_t* srcRow = reinterpret_cast<const uint8_t*>(MappedData.pData) + rowStride * y;
   uint8_t* dstRow = image.scanLine(y);
   memcpy(dstRow, srcRow, width * bytesPerPixel);
  }

  cv::Mat image2(height, width, CV_8UC4); // RGBA8相当 (4バイト/ピクセル)

  for (int y = 0; y < height; ++y)
  {
   const uint8_t* srcRow = reinterpret_cast<const uint8_t*>(MappedData.pData) + rowStride * y;
   uint8_t* dstRow = image.scanLine(y); // ← 上下反転コピー
   memcpy(dstRow, srcRow, width * bytesPerPixel);
  }



  pImmediateContext->UnmapTextureSubresource(pReadableTex, 0, 0);

  // クリップボードに転送
  QClipboard* clipboard = QGuiApplication::clipboard();
  QPixmap pixmap = QPixmap::fromImage(image);
  clipboard->setPixmap(pixmap, QClipboard::Clipboard);

  qDebug() << "Screenshot copied to clipboard";



 }
 void ArtifactDiligentEngineComposition2DWindow::Impl::saveScreenShotToClipboardByQt()
 {
  QPixmap pixmap(widget_->size());
  widget_->render(&pixmap);
  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setImage(pixmap.toImage());


  //Uint32 offset = 0;

  //pImmediateContext->SetPipelineState(pLine_PSO_);

 }



 void ArtifactDiligentEngineComposition2DWindow::Impl::saveScreenShotToClipboardByWinRT()
 {
  auto pRTV = pSwapChain_->GetCurrentBackBufferRTV();

  auto pBackBuffer = pRTV->GetTexture();

  Diligent::ITextureD3D12* pD3D12Texture;

  pBackBuffer->QueryInterface(IID_TextureD3D12, reinterpret_cast<IObject**>(&pD3D12Texture));





 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::postScreenShotEvent()
 {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  g_renderEvents.push([this]() {
   saveScreenShotToClipboard();

   });
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawLineCanvas(float x_1, float y_1, float x_2, float y_2, const FloatColor& color, float tick/*=1.0f*/)
 {
  LineVertex vertices[2] = {
	{ {x_1, y_1}, float4()},
	{ {x_2, y_2}, float4()} };

  void* mappedData = nullptr;

  Uint32 offset = 0;
 };



 void ArtifactDiligentEngineComposition2DWindow::Impl::drawQuadLine(float x, float y, float w, float h, const FloatColor& lineColor, float thikness/*=1.0f*/)
 {

 }
 //#createbuffer


 //#ClearCanvas
 void ArtifactDiligentEngineComposition2DWindow::Impl::clearCanvas(const Diligent::float4& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

  auto pRTV = pSwapChain_->GetCurrentBackBufferRTV();
  auto pDSV = pSwapChain_->GetDepthBufferDSV(); // 2Dならnullptrの場合が多い

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = { clearColor.r, clearColor.g,clearColor.b,clearColor.a };
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);






  if (pDSV) // pDSVがnullptrでないことを確認
  {
   pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }


 }

 //#ClearComposition
 void ArtifactDiligentEngineComposition2DWindow::Impl::clearComposition(const FloatColor& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

  if (compositeRenderTarget == nullptr)
  {

   return;
  }


  //auto desc = compositeRenderTarget->GetDesc();


  auto pRtv = compositeRenderTarget->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  pImmediateContext->SetRenderTargets(1, &pRtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const float ClearColor[] = { clearColor.r(), clearColor.g(),clearColor.b(),clearColor.a() };

  pImmediateContext->ClearRenderTarget(pRtv, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::resizeComposition(const ArtifactCore::Size_2D& size)
 {
  TextureDesc desc;

  desc.Width = size.width;
  desc.Height = size.height;
  //desc.Format = pSwapChain_->GetDesc().ColorBufferFormat();
  desc.Type = RESOURCE_DIM_TEX_2D;

  pDevice->CreateTexture(desc, nullptr, &compositeRenderTarget);

 }
 glm::mat4 ArtifactDiligentEngineComposition2DWindow::Impl::calculateModelMatrixGLM(const glm::vec2& position, /* 画面上の最終的なピクセル位置 */ const glm::vec2& size, /* レイヤーの元のピクセルサイズ */ const glm::vec2& anchorPoint, /* ローカル正規化座標 (0.0-1.0) でのアンカーポイント */ float rotationDegrees, /* 回転角度 (度数法) */ const glm::vec2& scale)
 {
  float anchorX_px = anchorPoint.x * size.x;
  float anchorY_px = anchorPoint.y * size.y;

  // 2. 変換行列の構築
  glm::mat4 model = glm::mat4(1.0f); // 単位行列で初期化

  // 2a. アンカーポイントを原点に移動
  // GLM はデフォルトで左から右へ乗算 (行ベクトル * 行列)
  model = glm::translate(model, glm::vec3(-anchorX_px, -anchorY_px, 0.0f));

  // 2b. スケーリング
  model = glm::scale(model, glm::vec3(scale.x, scale.y, 1.0f));

  // 2c. 回転 (Z軸周り)
  float rotationRadians = glm::radians(rotationDegrees); // 度数法からラジアンに変換
  model = glm::rotate(model, rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f));

  // 2d. アンカーポイントを元の位置に戻す + レイヤーの最終位置に移動
  model = glm::translate(model, glm::vec3(position.x + anchorX_px, position.y + anchorY_px, 0.0f));

  return model;
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawSolidQuadToComposition(float x, float y, float w, float h)
 {

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawTextInCanvas(const QString& string)
 {

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::hit()
 {

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawSprite(float x, float y, float w, float h, const QImage& sprite)
 {




  RefCntAutoPtr<ITexture> pTempRT;
  {
   TextureDesc TexDesc;
   TexDesc.Name = "TemporaryRenderTarget";
   TexDesc.Type = RESOURCE_DIM_TEX_2D;
   TexDesc.Width = 512;   // 小さめでもOK、100x100でも動く
   TexDesc.Height = 512;
   TexDesc.Format = TEX_FORMAT_RGBA8_UNORM;
   TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

   pDevice->CreateTexture(TexDesc, nullptr, &pTempRT);
  }

  auto pTempRTV = pTempRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

  auto* pBackBufferRTV = pSwapChain_->GetCurrentBackBufferRTV();

  pImmediateContext->SetRenderTargets(1, &pTempRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = { 0, 0, 0, 0 };
  pImmediateContext->ClearRenderTarget(pTempRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Vertex vertices[] = {
  { { x,     y },     { 0.0f, 0.0f } },
  { { x + w, y },     { 1.0f, 0.0f } },
  { { x,     y + h }, { 0.0f, 1.0f } },
  { { x + w, y + h }, { 1.0f, 1.0f } },
  };

  void* pVBData = nullptr;
  pImmediateContext->MapBuffer(m_pDrawSpriteVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD, pVBData);
  memcpy(pVBData, vertices, sizeof(vertices));
  pImmediateContext->UnmapBuffer(m_pDrawSpriteVertexBuffer, MAP_WRITE);

  Uint64 offset = 0;
  IBuffer* pVBs[] = { m_pDrawSpriteVertexBuffer };
  pImmediateContext->SetVertexBuffers(0, 1, pVBs, &offset,
   RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_NONE);


  pImmediateContext->SetPipelineState(pSprite_PSO_);


  Diligent::DrawAttribs DrawAttrs;
  DrawAttrs.NumVertices = 4;
  DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

  pImmediateContext->Draw(DrawAttrs);

  pImmediateContext->SetRenderTargets(1, &pBackBufferRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);




 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::destroy()
 {
  if (pSwapChain_)
  {
   // GPUリソースを安全に破棄
   pSwapChain_.Release();
   //pSwapChain_ = nullptr;
  }

 }

 QSize ArtifactDiligentEngineComposition2DWindow::sizeHint() const
 {
  // デフォルトのヒントサイズを返す。これがフローティングウィンドウの初期サイズになります。
  return QSize(800, 600); // 例: 適切なデフォルトサイズ
 }

 void ArtifactDiligentEngineComposition2DWindow::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  impl_->recreateSwapChain(this);

  update();
 }

 void ArtifactDiligentEngineComposition2DWindow::showEvent(QShowEvent* event)
 {
  //impl_->renderOneFrame();

  update();



 }

 void ArtifactDiligentEngineComposition2DWindow::wheelEvent(QWheelEvent* event)
 {
  QPoint numDegrees = event->angleDelta() / 8;

  if (!numDegrees.isNull()) {
   if (numDegrees.y() > 0) {
	impl_->zoomIn();
   }
   else {
	// Wheel scrolled down (backward) - Zoom Out
	//zoomOut();
   }
  }

  // Accept the event to prevent it from being propagated to parent widgets.
  event->accept();
 }

 ArtifactDiligentEngineComposition2DWindow::ArtifactDiligentEngineComposition2DWindow(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->initialize(this);

  QTimer* renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
   impl_->renderOneFrame();

   });
  renderTimer->start(16);
  connect(this, &QWidget::destroyed, renderTimer, &QTimer::stop);

  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
 }

 ArtifactDiligentEngineComposition2DWindow::~ArtifactDiligentEngineComposition2DWindow()
 {
  impl_->destroy();
  delete impl_;
 }
 bool ArtifactDiligentEngineComposition2DWindow::clear(const Diligent::float4& clearColor)
 {

  return true;
 }

 void ArtifactDiligentEngineComposition2DWindow::saveScreenShotToClipboard()
 {
  //impl_->saveScreenShotToClipboard();
 }
 void ArtifactDiligentEngineComposition2DWindow::paintEvent(QPaintEvent* event)
 {
  impl_->renderOneFrame();

 }

 void ArtifactDiligentEngineComposition2DWindow::setCanvasColor(const FloatColor& color)
 {
  impl_->setCanvasColor(color);
 }

 void ArtifactDiligentEngineComposition2DWindow::keyPressEvent(QKeyEvent* event)
 {
  if (event->key() == Qt::Key_S) {
   // ここでスクショ保存関数呼ぶ
   impl_->postScreenShotEvent();
   event->accept();
   return;
  }
  QWidget::keyPressEvent(event);
 }

 void ArtifactDiligentEngineComposition2DWindow::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::MiddleButton) {
   //panning = true;
   //panStart = event->pos();
   setCursor(Qt::ClosedHandCursor); // 手のアイコン
   event->accept();
  }
  else {
   QWidget::mousePressEvent(event);
  }
 }

 void ArtifactDiligentEngineComposition2DWindow::mouseReleaseEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::MiddleButton) {
   //panning = false;
   setCursor(Qt::ArrowCursor);
   event->accept();
  }
  else {
   QWidget::mouseMoveEvent(event);
  }
 }

 void ArtifactDiligentEngineComposition2DWindow::mouseMoveEvent(QMouseEvent* event)
 {


 }

 void ArtifactDiligentEngineComposition2DWindow::saveScreenShotToFile()
 {

 }

 class  ArtifactDiligentEngineComposition2DWidget::Impl {
 private:

 public:
  ArtifactDiligentEngineComposition2DWindow* window_ = nullptr;
  Impl(QWidget* widget);

 };

 W_OBJECT_IMPL(ArtifactDiligentEngineComposition2DWidget)

  void ArtifactDiligentEngineComposition2DWindow::saveScreenShotToClipboardByQt()
 {
  QPixmap pixmap = this->grab(); // widget全体をキャプチャ
  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setImage(pixmap.toImage());
 }

 void ArtifactDiligentEngineComposition2DWindow::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();

  QWidget::closeEvent(event);
 }

 ArtifactDiligentEngineComposition2DWidget::Impl::Impl(QWidget* widget)
 {
  //window_ = new ArtifactDiligentEngineComposition2DWindow();


  //QWidget* container = QWidget::createWindowContainer(window_,widget);
 // container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  //QVBoxLayout* layout = new QVBoxLayout();
  //layout->addWidget(container,1);
  //layout->setContentsMargins(0, 0, 0, 0); // マージンを0に設定
  //layout->setSpacing(0);
  //widget->setLayout(layout);
  //container->setMinimumSize(0, 0);
  //container->setAutoFillBackground(false);
  //container->setStyleSheet("background-color:red;");
  //container->setAttribute(Qt::WA_TranslucentBackground);
  //container->setAttribute(Qt::WA_NoSystemBackground);
 }

 void ArtifactDiligentEngineComposition2DWidget::paintEvent(QPaintEvent* event)
 {
  Q_UNUSED(event);
 }

 ArtifactDiligentEngineComposition2DWidget::ArtifactDiligentEngineComposition2DWidget(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl(this))
 {
  setAttribute(Qt::WA_NoSystemBackground); // システム背景の描画を無効化
  setAttribute(Qt::WA_TranslucentBackground);
  //QVBoxLayout* mainLayout = new QVBoxLayout(this);


  //mainLayout->addWidget(m_compositionWidget, 1);
 }

 ArtifactDiligentEngineComposition2DWidget::~ArtifactDiligentEngineComposition2DWidget()
 {
  delete impl_;
 }

 void ArtifactDiligentEngineComposition2DWidget::setCanvasColor(const FloatColor& color)
 {

 }




}
