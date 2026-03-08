module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Texture.h>
#include <RefCntAutoPtr.hpp>
#include <QImage>
#include <QDebug>

module Artifact.Render.Offscreen;

import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Color.Float;
import Core.Point2D;
import Image.ImageF32x4_RGBA;
import Frame.Position;
import std;

namespace Artifact
{
    using namespace Diligent;

    /**
     * @brief 汎用コンポジションレンダラー
     * コンポジション内の各レイヤーを走査し、オフスクリーンバッファにレンダリングします。
     */
    class CompositionRenderer
    {
    public:
        CompositionRenderer(RefCntAutoPtr<IRenderDevice> pDevice, Uint32 width, Uint32 height);
        ~CompositionRenderer();

        // 指定された時刻のフレームをレンダリング
        void renderFrame(const FramePosition& position, ArtifactAbstractComposition* composition);

        // レンダリング結果をImageF32x4_RGBAとして取得（エンコーダ用）
        ArtifactCore::ImageF32x4_RGBA captureImage();

        // 特定のファイルパスに画像として保存
        bool saveFrame(const QString& path);

        // レンダラーへのアクセス
        ArtifactIRenderer* renderer() { return renderer_.get(); }

    private:
        RefCntAutoPtr<IRenderDevice> pDevice_;
        RefCntAutoPtr<IDeviceContext> pContext_;
        RefCntAutoPtr<ITexture> pRenderTarget_;
        std::unique_ptr<ArtifactIRenderer> renderer_;
        Uint32 width_;
        Uint32 height_;

        void initializeResources();
    };
}



namespace Artifact
{
    CompositionRenderer::CompositionRenderer(RefCntAutoPtr<IRenderDevice> pDevice, Uint32 width, Uint32 height)
        : pDevice_(pDevice), width_(width), height_(height)
    {
        // 独自のイミディエイト/ディファードコンテキストを作成
        pDevice_->CreateDeferredContext(&pContext_);
        initializeResources();
        
        // IRendererを初期化（ウィジェットなしモード）
        renderer_ = std::make_unique<ArtifactIRenderer>(pDevice_, pContext_, nullptr);
        renderer_->setCanvasSize((float)width_, (float)height_);
        renderer_->setViewportSize((float)width_, (float)height_);
    }

    CompositionRenderer::~CompositionRenderer() = default;

    void CompositionRenderer::initializeResources()
    {
        TextureDesc TexDesc;
        TexDesc.Name = "Composition Render Target";
        TexDesc.Type = RESOURCE_DIM_TEX_2D;
        TexDesc.Width = width_;
        TexDesc.Height = height_;
        TexDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        TexDesc.Usage = USAGE_DEFAULT;

        pDevice_->CreateTexture(TexDesc, nullptr, &pRenderTarget_);
    }

    void CompositionRenderer::renderFrame(const FramePosition& position, ArtifactAbstractComposition* composition)
    {
        if (!composition) return;

        ITextureView* pRTV = pRenderTarget_->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        // 背景色はコンポジションの設定から取得すべきだが、一旦クリア
        float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f }; 
        
        pContext_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext_->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // レイヤーリストを取得
        auto layers = composition->allLayer();
        
        // After Effectsのようにインデックスが大きいレイヤーが背面にあると仮定し、
        // 背面から順に描画（ペインターズ・アルゴリズム）
        std::reverse(layers.begin(), layers.end());

        for (auto& layer : layers) {
            if (layer && layer->isActiveAt(position) && layer->isVisible()) {
                // 各レイヤーの描画ロジック（render）を呼び出し
                layer->draw(renderer_.get());
            }
        }
        
        // コマンドの完了を待機
        pContext_->Flush();
    }

    ArtifactCore::ImageF32x4_RGBA CompositionRenderer::captureImage()
    {
        // 1. Staging Textureの作成（読み取り用）
        TextureDesc StagingDesc;
        StagingDesc.Name           = "Staging Texture";
        StagingDesc.Type           = RESOURCE_DIM_TEX_2D;
        StagingDesc.Width          = width_;
        StagingDesc.Height         = height_;
        StagingDesc.Format         = pRenderTarget_->GetDesc().Format;
        StagingDesc.Usage          = USAGE_STAGING;
        StagingDesc.CPUAccessFlags = CPU_ACCESS_READ;

        RefCntAutoPtr<ITexture> pStagingTex;
        pDevice_->CreateTexture(StagingDesc, nullptr, &pStagingTex);

        // 2. GPUバッファからStagingへコピー
        CopyTextureAttribs CopyAttribs(pRenderTarget_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                       pStagingTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext_->CopyTexture(CopyAttribs);
        
        // 描画完了を待機
        pContext_->Flush();
        pContext_->WaitForIdle(); // 簡略化のためアイドル待機

        // 3. Mapしてデータを取得
        MappedTextureSubresource MappedData;
        pContext_->MapTextureSubresource(pStagingTex, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, MappedData);

        ArtifactCore::ImageF32x4_RGBA image;
        image.resize(width_, height_);
        
        // ピクセルデータのコピー (RGBA8 -> FloatRGBA)
        const uint8_t* pSrc = static_cast<const uint8_t*>(MappedData.pData);
        for (Uint32 y = 0; y < height_; ++y) {
            for (Uint32 x = 0; x < width_; ++x) {
                const uint8_t* pPixel = pSrc + y * MappedData.Stride + x * 4;
                ArtifactCore::FloatRGBA col(pPixel[0]/255.0f, pPixel[1]/255.0f, pPixel[2]/255.0f, pPixel[3]/255.0f);
                image.setPixel(x, y, col);
            }
        }

        pContext_->UnmapTextureSubresource(pStagingTex, 0, 0);
        return image;
    }

    bool CompositionRenderer::saveFrame(const QString& path)
    {
        ArtifactCore::ImageF32x4_RGBA img = captureImage();
        // ImageF32x4_RGBAをQImageに変換して保存
        QImage qimg(width_, height_, QImage::Format_RGBA8888);
        for (Uint32 y = 0; y < height_; ++y) {
            for (Uint32 x = 0; x < width_; ++x) {
                auto col = img.getPixel(x, y);
                qimg.setPixelColor(x, y, QColor::fromRgbF(col.r(), col.g(), col.b(), col.a()));
            }
        }
        return qimg.save(path);
    }
}
