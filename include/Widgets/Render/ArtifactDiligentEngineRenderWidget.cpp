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


import std;

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

 W_OBJECT_IMPL(ArtifactDiligentEngineComposition2DWindow)

  class ArtifactDiligentEngineComposition2DWindow::Impl {
  private:
   FloatColor canvasColor_;
   float scale_ = 1.0f;

   RefCntAutoPtr<IRenderDevice> pDevice;
   RefCntAutoPtr<IDeviceContext> pImmediateContext;

   RefCntAutoPtr<ISwapChain> pSwapChain_;
   RefCntAutoPtr<IPipelineState> p2D_PSO_;
   RefCntAutoPtr<IPipelineState> pLine_PSO_;
   RefCntAutoPtr<IShader> p2D_vertex_shader;
   RefCntAutoPtr<IShader> p2D_pixel_;
   RefCntAutoPtr<IShader>p2d_line_vertex_shader_;
   RefCntAutoPtr<IShader>p2d_line_pixel_shader_;
   //RefCntAutoPtr<IShader> p
   ComPtr<ID3D11Device> d3d11Device_;
   ComPtr<ID3D11DeviceContext> d3d11Context_;
   ComPtr<ID3D11On12Device> d3d11On12Device_;

   RefCntAutoPtr<IBuffer>        pConstantsBuffer;
   RefCntAutoPtr<IBuffer>		pBuffer;


   RefCntAutoPtr<IBuffer>        p2D_VBuffer_;
   RefCntAutoPtr<IBuffer>        p2D_VFrastumBuffer_;
   RefCntAutoPtr<IBuffer>		p2D_VLineBuffer_;
   RefCntAutoPtr<IShaderResourceBinding> p2D_SRB_;
   RefCntAutoPtr<IShaderResourceBinding> p2D_LINE_SRB_;
   //float4x4 projectionMatrix_;
   std::vector<ShaderResourceVariableDesc> m_ResourceVars;
   std::vector<ImmutableSamplerDesc> m_sampler_;
   QWidget* widget_ = nullptr;
   bool m_initialized = false;
   int m_CurrentPhysicalWidth;
   int m_CurrentPhysicalHeight;

   glm::mat4 glm_projection_;
   glm::mat4 view_;
   qreal m_CurrentDevicePixelRatio;
   RefCntAutoPtr<ITextureView> p2D_TextureView;
   RefCntAutoPtr<ITexture> p2D_Texture;
   glm::mat4 calculateModelMatrixGLM(const glm::vec2& position,      // 画面上の最終的なピクセル位置
	const glm::vec2& size,          // レイヤーの元のピクセルサイズ
	const glm::vec2& anchorPoint,   // ローカル正規化座標 (0.0-1.0) でのアンカーポイント
	float rotationDegrees,          // 回転角度 (度数法)
	const glm::vec2& scale);
   std::mutex g_eventMutex;
   std::queue<std::function<void()>> g_renderEvents;
   bool takeScreenshot = false;

   void initializeResources();
   void createShaders();
   void createBuffers();
   void createPSO();
   void calcProjection(int width, int height);

  public:
   Impl();
   ~Impl();
   void initialize(QWidget* window);
   void initializeImGui(QWidget* window);
   void recreateSwapChain(QWidget* window);
   void clear(const Diligent::float4& clearColor);
   void present();
   void renderOneFrame();
   void drawSprite(float x, float y, float w, float h);
   void drawSolidQuad(float x, float y, float w, float h);
   void drawQuadLine(float x, float y, float w, float h, const FloatColor& lineColor, float thikness = 1.0f);
   void drawLine(float x_1, float y_1, float x_2, float y_2, const FloatColor& color);
   void drawViewCameraFrustum();

   void zoomIn();
   void zoomOut();
   void setCanvasColor(const FloatColor& color);
   void postScreenShotEvent();
   void saveScreenShotToClipboard();
   void saveScreenShotToClipboardByQt();
   void saveScreenShotToClipboardByWinRT();
 };

 void ArtifactDiligentEngineComposition2DWindow::saveScreenShotToClipboardByQt()
 {
  QPixmap pixmap = this->grab(); // widget全体をキャプチャ
  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setImage(pixmap.toImage());
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::clear(const Diligent::float4& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

  auto pRTV = pSwapChain_->GetCurrentBackBufferRTV();
  auto pDSV = pSwapChain_->GetDepthBufferDSV(); // 2Dならnullptrの場合が多い

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = { clearColor.r, clearColor.g,clearColor.b,clearColor.a };
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (pDSV) // pDSVがnullptrでないことを確認
  {
   pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }


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
  testShaderCI.EntryPoint = "main";
  testShaderCI.Desc.Name = "TestPixelShader";
  //ShaderCI.Source = g_qsBasic2DImagePS.constData();
  //ShaderCI.SourceLength = g_qsBasic2DImagePS.length();
  testShaderCI.Source = g_qsSolidColorPS2.constData();
  testShaderCI.SourceLength = g_qsSolidColorPS2.length();


  pDevice->CreateShader(testShaderCI, &p2D_pixel_);

  ShaderCreateInfo basicShaderCI;
  basicShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  basicShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  basicShaderCI.EntryPoint = "main";
  basicShaderCI.Desc.Name = "BasicPixelShader";
  basicShaderCI.Source = g_qsSolidColorPS2.constData();
  //ShaderCI.SourceLength = g_qsBasic2DImagePS.length();




  ShaderCreateInfo BasicVertexShaderCI;
  BasicVertexShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  BasicVertexShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  BasicVertexShaderCI.EntryPoint = "main";
  BasicVertexShaderCI.Desc.Name = "MyVertexShader";
  BasicVertexShaderCI.Source = g_qsBasic2DVS.constData();
  BasicVertexShaderCI.SourceLength = g_qsBasic2DVS.length();

  pDevice->CreateShader(BasicVertexShaderCI, &p2D_vertex_shader);

  ShaderCreateInfo lineVsInfo;

  lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;



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

  calcProjection(m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);

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

 void ArtifactDiligentEngineComposition2DWindow::Impl::present()
 {
  if (pSwapChain_)
  {

   pSwapChain_->Present(1);
  }
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::initializeImGui(QWidget* window)
 {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  (void)io;


 }

 //#RenderLoop
 void ArtifactDiligentEngineComposition2DWindow::Impl::renderOneFrame()
 {
  const Diligent::float4& clearColor = { 0.5f,0.5f,0.5f,1.0f };
  clear(clearColor);

  drawSolidQuad(0, 0, 400, 400);

  drawSolidQuad(500, 0, 400, 400);


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



  auto psoInfo = create2DPSOHelper();

  // Define vertex shader input layout
  LayoutElement LayoutElems[] =
  {
  Diligent::LayoutElement{0, 0, 2, VT_FLOAT32, False, 0},
   Diligent::LayoutElement{1, 0, 2, VT_FLOAT32, False, sizeof(float2)}
  };
  psoInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
  psoInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;

  psoInfo.pVS = p2D_vertex_shader;
  psoInfo.pPS = p2D_pixel_;
  psoInfo.PSODesc.ResourceLayout.Variables = m_ResourceVars.data();
  psoInfo.PSODesc.ResourceLayout.NumVariables = static_cast<Uint32>(m_ResourceVars.size());

  psoInfo.PSODesc.ResourceLayout.ImmutableSamplers = m_sampler_.data();
  psoInfo.PSODesc.ResourceLayout.NumImmutableSamplers = static_cast<Uint32>(m_sampler_.size());
  pDevice->CreateGraphicsPipelineState(psoInfo, &p2D_PSO_);

  p2D_PSO_->CreateShaderResourceBinding(&p2D_SRB_, false);
  if (!p2D_SRB_)
  {
   // エラー処理（例: qCritical() << "Failed to create 2D Shader Resource Binding!";）
   return;
  }

  auto linePSOInfo = createLinePSOHelper();

  //linePSOInfo.pVS = ;

  //pDevice->CreateGraphicsPipelineState(linePSOInfo, &p);



 }
 //#DrawQuad
 void ArtifactDiligentEngineComposition2DWindow::Impl::drawSolidQuad(float x, float y, float w, float h)
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

  pImmediateContext->SetPipelineState(p2D_PSO_);

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
  //impl_->setCanvasColor(color);
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

  qDebug()<<"Test:"<<pFence->GetCompletedValue();

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

 void ArtifactDiligentEngineComposition2DWindow::Impl::postScreenShotEvent()
 {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  g_renderEvents.push([this]() {
   saveScreenShotToClipboard();

   });
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawLine(float x_1, float y_1, float x_2, float y_2, const FloatColor& color)
 {
  //LineVertex vertices[2] = {
	//{ {x_1, y_1}, color },
	//{ {x_2, y_2}, color }
 };

 void ArtifactDiligentEngineComposition2DWindow::Impl::saveScreenShotToClipboardByQt()
 {
  QPixmap pixmap(widget_->size());
  widget_->render(&pixmap);
  QClipboard* clipboard = QGuiApplication::clipboard();
  clipboard->setImage(pixmap.toImage());


  //Uint32 offset = 0;

  //pImmediateContext->SetPipelineState(pLine_PSO_);

 }

 ArtifactDiligentEngineComposition2DWindow::Impl::~Impl()
 {
  // d3d11Context_->Release();
   //d3d11Device_->Release();

  RoUninitialize();
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::saveScreenShotToClipboardByWinRT()
 {
  auto pRTV = pSwapChain_->GetCurrentBackBufferRTV();

  auto pBackBuffer = pRTV->GetTexture();

  Diligent::ITextureD3D12* pD3D12Texture;

  pBackBuffer->QueryInterface(IID_TextureD3D12, reinterpret_cast<IObject**>(&pD3D12Texture));





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
  SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
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



  ID3D12Device* d3d12Device;

  UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  D3D_FEATURE_LEVEL featureLevels[] = {
D3D_FEATURE_LEVEL_11_1,
D3D_FEATURE_LEVEL_11_0,
  };

  auto renderDevice12 = static_cast<IRenderDeviceD3D12*>(pDevice.RawPtr());


  d3d12Device = renderDevice12->GetD3D12Device();


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



  createShaders();
  createPSO();
  initializeResources();

  m_initialized = true;
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::drawQuadLine(float x, float y, float w, float h, const FloatColor& lineColor, float thikness/*=1.0f*/)
 {

 }
 //#createbuffer
 void ArtifactDiligentEngineComposition2DWindow::Impl::createBuffers()
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
 }

 ArtifactDiligentEngineComposition2DWindow::ArtifactDiligentEngineComposition2DWindow(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->initialize(this);

  QTimer* renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
   impl_->renderOneFrame();

   });
  renderTimer->start(16);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
 }

 ArtifactDiligentEngineComposition2DWindow::~ArtifactDiligentEngineComposition2DWindow()
 {
  delete impl_;
 }
 void ArtifactDiligentEngineComposition2DWindow::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  impl_->recreateSwapChain(this);

  update();
 }

 bool ArtifactDiligentEngineComposition2DWindow::clear(const Diligent::float4& clearColor)
 {

  return true;
 }

 QSize ArtifactDiligentEngineComposition2DWindow::sizeHint() const
 {
  // デフォルトのヒントサイズを返す。これがフローティングウィンドウの初期サイズになります。
  return QSize(800, 600); // 例: 適切なデフォルトサイズ
 }

 void ArtifactDiligentEngineComposition2DWindow::showEvent(QShowEvent* event)
 {
  //impl_->renderOneFrame();

  update();



 }

 void ArtifactDiligentEngineComposition2DWindow::paintEvent(QPaintEvent* event)
 {
  impl_->renderOneFrame();

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

 void ArtifactDiligentEngineComposition2DWindow::setCanvasColor(const FloatColor& color)
 {
  impl_->setCanvasColor(color);
 }

 void ArtifactDiligentEngineComposition2DWindow::saveScreenShotToClipboard()
 {
  //impl_->saveScreenShotToClipboard();
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

 void ArtifactDiligentEngineComposition2DWidget::paintEvent(QPaintEvent* event)
 {
  Q_UNUSED(event);
 }

 void ArtifactDiligentEngineComposition2DWidget::setCanvasColor(const FloatColor& color)
 {

 }

};