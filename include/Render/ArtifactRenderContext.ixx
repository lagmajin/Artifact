module;
#include <QRectF>
#include <QSize>
#include <QColor>
#include <memory>
#include <vector>

export module Artifact.Render.Context;

import Artifact.Render.ROI;

export namespace Artifact {

/**
 * @brief レンダリングコンテキスト
 * 
 * レンダリングに必要な状態と設定をカプセル化
 */
struct RenderContext {
    // === 基本設定 ===
    
    /// レンダリングモード
    RenderMode mode = RenderMode::Preview;
    
    /// モード別設定
    RenderModeSettings modeSettings = getModeSettings(RenderMode::Preview);
    
    // === 座標系 ===
    
    /// ビューポートサイズ（ピクセル）
    QSize viewportSize = QSize(1920, 1080);
    
    /// キャンバスサイズ（composition 座標系）
    QSizeF canvasSize = QSizeF(1920, 1080);
    
    /// ズーム倍率
    float zoom = 1.0f;
    
    /// パンオフセット（ビューポート座標系）
    QPointF pan = QPointF(0, 0);
    
    // === フレーム情報 ===
    
    /// 現在フレーム
    int64_t currentFrame = 0;
    
    /// フレームレート
    float frameRate = 30.0f;
    
    /// 開始フレーム
    int64_t startFrame = 0;
    
    /// 終了フレーム
    int64_t endFrame = 299;
    
    // === ROI ===
    
    /// 処理対象 ROI（composition 座標系）
    RenderROI roi;
    
    /// ビューポート ROI（ビューポート座標系）
    RenderROI viewportROI;
    
    /// Scissor ROI（スクリーン座標系）
    RenderROI scissorROI;
    
    // === クリア設定 ===
    
    /// クリアカラー
    QColor clearColor = QColor(0, 0, 0, 0);
    
    /// 背景を描画するか
    bool drawBackground = true;
    
    /// チェッカーボードを描画するか
    bool drawCheckerboard = false;
    
    /// グリッドを描画するか
    bool drawGrid = false;
    
    // === 品質設定 ===
    
    /// アンチエイリアシングサンプル数
    int sampleCount = 1;
    
    /// テクスチャフィルタリング品質（0-3）
    int textureFilterQuality = 2;
    
    /// 色深度（8, 16, 32）
    int colorDepth = 8;
    
    // === キャッシュ ===
    
    /// ROI キャッシュを使用するか
    bool useROICache = true;
    
    /// テクスチャキャッシュを使用するか
    bool useTextureCache = true;
    
    // === デバッグ ===
    
    /// デバッグ表示を有効にするか
    bool debugMode = false;
    
    /// ROI 可視化（デバッグ用）
    bool visualizeROI = false;
    
    /// 描画時間を表示するか
    bool showRenderTime = false;
    
    // === ヘルパーメソッド ===
    
    /**
     * @brief コンテキストをリセット
     */
    void reset()
    {
        mode = RenderMode::Preview;
        modeSettings = getModeSettings(RenderMode::Preview);
        viewportSize = QSize(1920, 1080);
        canvasSize = QSizeF(1920, 1080);
        zoom = 1.0f;
        pan = QPointF(0, 0);
        currentFrame = 0;
        frameRate = 30.0f;
        startFrame = 0;
        endFrame = 299;
        roi = RenderROI();
        viewportROI = RenderROI();
        scissorROI = RenderROI();
        clearColor = QColor(0, 0, 0, 0);
        drawBackground = true;
        drawCheckerboard = false;
        drawGrid = false;
        sampleCount = 1;
        textureFilterQuality = 2;
        colorDepth = 8;
        useROICache = true;
        useTextureCache = true;
        debugMode = false;
        visualizeROI = false;
        showRenderTime = false;
    }
    
    /**
     * @brief モードを設定して関連設定を更新
     */
    void setMode(RenderMode newMode)
    {
        mode = newMode;
        modeSettings = getModeSettings(newMode);
        sampleCount = modeSettings.sampleCount;
        useROICache = modeSettings.useROICache;
    }
    
    /**
     * @brief ビューポートサイズを設定
     */
    void setViewportSize(int width, int height)
    {
        viewportSize = QSize(width, height);
        updateViewportROI();
    }
    
    /**
     * @brief ズームを設定
     */
    void setZoom(float newZoom)
    {
        zoom = std::max(0.01f, newZoom);
    }
    
    /**
     * @brief パンを設定
     */
    void setPan(float x, float y)
    {
        pan = QPointF(x, y);
    }
    
    /**
     * @brief 現在フレームを設定
     */
    void setCurrentFrame(int64_t frame)
    {
        currentFrame = frame;
    }
    
    /**
     * @brief ROI を設定
     */
    void setROI(const RenderROI& newROI)
    {
        roi = newROI;
        updateViewportROI();
        updateScissorROI();
    }
    
    /**
     * @brief ビューポート ROI を更新
     */
    void updateViewportROI()
    {
        if (roi.isEmpty()) {
            viewportROI = RenderROI();
            return;
        }
        
        // Composition → Viewport 変換
        viewportROI = RenderROI(
            roi.x() * zoom + pan.x(),
            roi.y() * zoom + pan.y(),
            roi.width() * zoom,
            roi.height() * zoom
        );
    }
    
    /**
     * @brief Scissor ROI を更新
     */
    void updateScissorROI()
    {
        if (viewportROI.isEmpty()) {
            scissorROI = RenderROI();
            return;
        }
        
        // Viewport → Scissor 変換（Y 反転）
        scissorROI = RenderROI(
            viewportROI.x(),
            static_cast<float>(viewportSize.height()) - viewportROI.y() - viewportROI.height(),
            viewportROI.width(),
            viewportROI.height()
        );
    }
    
    /**
     * @brief 現在のモードが Editor か判定
     */
    bool isEditorMode() const
    {
        return mode == RenderMode::Editor;
    }
    
    /**
     * @brief 現在のモードが Final か判定
     */
    bool isFinalMode() const
    {
        return mode == RenderMode::Final;
    }
    
    /**
     * @brief エフェクトが有効か判定
     */
    bool effectsEnabled() const
    {
        return modeSettings.enableEffects;
    }
    
    /**
     * @brief 空 ROI のスキップが有効か判定
     */
    bool shouldSkipEmptyROI() const
    {
        return modeSettings.skipEmptyROI;
    }
    
    /**
     * @brief Scissor テストが有効か判定
     */
    bool scissorTestEnabled() const
    {
        return modeSettings.useScissorTest;
    }
};

/**
 * @brief レンダリングコンテキストポインタ
 */
using RenderContextPtr = std::shared_ptr<RenderContext>;

/**
 * @brief レンダリングコンテキストを作成
 */
inline RenderContextPtr createRenderContext(RenderMode mode = RenderMode::Preview)
{
    auto ctx = std::make_shared<RenderContext>();
    ctx->setMode(mode);
    return ctx;
}

} // namespace Artifact
