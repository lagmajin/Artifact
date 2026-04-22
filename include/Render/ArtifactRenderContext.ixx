module;
#include <utility>
#include <memory>
#include <vector>
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QColor>
export module Artifact.Render.Context;


import Artifact.Render.ROI;
import Color.ColorSpace;

export namespace Artifact {

/**
 * @brief レンダリング用途
 */
enum class RenderPurpose {
    EditorInteractive,
    EditorPreview,
    FinalExport,
    ProxyBuild,
    Thumbnail
};

inline bool isInteractiveRenderPurpose(RenderPurpose purpose)
{
    return purpose == RenderPurpose::EditorInteractive ||
           purpose == RenderPurpose::EditorPreview;
}

inline QString renderPurposeToString(RenderPurpose purpose)
{
    switch (purpose) {
    case RenderPurpose::EditorInteractive:
        return QStringLiteral("editor-interactive");
    case RenderPurpose::EditorPreview:
        return QStringLiteral("editor-preview");
    case RenderPurpose::FinalExport:
        return QStringLiteral("final-export");
    case RenderPurpose::ProxyBuild:
        return QStringLiteral("proxy-build");
    case RenderPurpose::Thumbnail:
        return QStringLiteral("thumbnail");
    }
    return QStringLiteral("unknown");
}

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

    /// 解像度スケール
    float resolutionScale = 1.0f;

    /// インタラクティブ用途かどうか
    bool interactive = true;

    /// カラースペース
    ArtifactCore::ColorSpace colorSpace = ArtifactCore::ColorSpace::sRGB;
    
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
        resolutionScale = modeSettings.resolutionScale;
        interactive = true;
        colorSpace = ArtifactCore::ColorSpace::sRGB;
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
        resolutionScale = modeSettings.resolutionScale;
        interactive = (newMode != RenderMode::Final);
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
     * @brief 解像度スケールを設定
     */
    void setResolutionScale(float newResolutionScale)
    {
        resolutionScale = std::max(0.01f, newResolutionScale);
    }

    /**
     * @brief インタラクティブ用途かどうかを設定
     */
    void setInteractive(bool isInteractive)
    {
        interactive = isInteractive;
    }

    /**
     * @brief カラースペースを設定
     */
    void setColorSpace(ArtifactCore::ColorSpace newColorSpace)
    {
        colorSpace = newColorSpace;
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
 * @brief レンダリングコンテキストの snapshot
 */
struct RenderContextSnapshot {
    QString key;
    RenderPurpose purpose = RenderPurpose::EditorPreview;
    RenderMode mode = RenderMode::Preview;
    float time = 0.0f;
    float resolutionScale = 1.0f;
    bool interactive = true;
    ArtifactCore::ColorSpace colorSpace = ArtifactCore::ColorSpace::sRGB;
    QSize viewportSize = QSize(1920, 1080);
    QSizeF canvasSize = QSizeF(1920, 1080);
    float zoom = 1.0f;
    QPointF pan = QPointF(0, 0);
    int64_t currentFrame = 0;
    float frameRate = 30.0f;
    int64_t startFrame = 0;
    int64_t endFrame = 299;
    RenderROI roi;
    RenderROI viewportROI;
    RenderROI scissorROI;
    QColor clearColor = QColor(0, 0, 0, 0);
    bool drawBackground = true;
    bool drawCheckerboard = false;
    bool drawGrid = false;
    int sampleCount = 1;
    int textureFilterQuality = 2;
    int colorDepth = 8;
    bool useROICache = true;
    bool useTextureCache = true;
    bool debugMode = false;
    bool visualizeROI = false;
    bool showRenderTime = false;
};

/**
 * @brief レンダリングコンテキストポインタ
 */
using RenderContextPtr = std::shared_ptr<RenderContext>;

inline RenderContextSnapshot createRenderContextSnapshot(const RenderContext& ctx,
                                                         RenderPurpose purpose = RenderPurpose::EditorPreview,
                                                         const QString& key = QString())
{
    RenderContextSnapshot snapshot;
    snapshot.key = key;
    snapshot.purpose = purpose;
    snapshot.mode = ctx.mode;
    snapshot.time = static_cast<float>(ctx.currentFrame) / std::max(1.0f, ctx.frameRate);
    snapshot.resolutionScale = ctx.resolutionScale;
    snapshot.interactive = ctx.interactive;
    snapshot.colorSpace = ctx.colorSpace;
    snapshot.viewportSize = ctx.viewportSize;
    snapshot.canvasSize = ctx.canvasSize;
    snapshot.zoom = ctx.zoom;
    snapshot.pan = ctx.pan;
    snapshot.currentFrame = ctx.currentFrame;
    snapshot.frameRate = ctx.frameRate;
    snapshot.startFrame = ctx.startFrame;
    snapshot.endFrame = ctx.endFrame;
    snapshot.roi = ctx.roi;
    snapshot.viewportROI = ctx.viewportROI;
    snapshot.scissorROI = ctx.scissorROI;
    snapshot.clearColor = ctx.clearColor;
    snapshot.drawBackground = ctx.drawBackground;
    snapshot.drawCheckerboard = ctx.drawCheckerboard;
    snapshot.drawGrid = ctx.drawGrid;
    snapshot.sampleCount = ctx.sampleCount;
    snapshot.textureFilterQuality = ctx.textureFilterQuality;
    snapshot.colorDepth = ctx.colorDepth;
    snapshot.useROICache = ctx.useROICache;
    snapshot.useTextureCache = ctx.useTextureCache;
    snapshot.debugMode = ctx.debugMode;
    snapshot.visualizeROI = ctx.visualizeROI;
    snapshot.showRenderTime = ctx.showRenderTime;
    return snapshot;
}

inline RenderContextPtr restoreRenderContext(const RenderContextSnapshot& snapshot)
{
    auto ctx = std::make_shared<RenderContext>();
    ctx->setMode(snapshot.mode);
    ctx->currentFrame = snapshot.currentFrame;
    ctx->frameRate = snapshot.frameRate;
    ctx->setResolutionScale(snapshot.resolutionScale);
    ctx->setInteractive(snapshot.interactive);
    ctx->setColorSpace(snapshot.colorSpace);
    ctx->viewportSize = snapshot.viewportSize;
    ctx->canvasSize = snapshot.canvasSize;
    ctx->zoom = snapshot.zoom;
    ctx->pan = snapshot.pan;
    ctx->startFrame = snapshot.startFrame;
    ctx->endFrame = snapshot.endFrame;
    ctx->roi = snapshot.roi;
    ctx->viewportROI = snapshot.viewportROI;
    ctx->scissorROI = snapshot.scissorROI;
    ctx->clearColor = snapshot.clearColor;
    ctx->drawBackground = snapshot.drawBackground;
    ctx->drawCheckerboard = snapshot.drawCheckerboard;
    ctx->drawGrid = snapshot.drawGrid;
    ctx->sampleCount = snapshot.sampleCount;
    ctx->textureFilterQuality = snapshot.textureFilterQuality;
    ctx->colorDepth = snapshot.colorDepth;
    ctx->useROICache = snapshot.useROICache;
    ctx->useTextureCache = snapshot.useTextureCache;
    ctx->debugMode = snapshot.debugMode;
    ctx->visualizeROI = snapshot.visualizeROI;
    ctx->showRenderTime = snapshot.showRenderTime;
    return ctx;
}

class RenderContextRegistry {
public:
    static RenderContextRegistry& instance()
    {
        static RenderContextRegistry registry;
        return registry;
    }

    QString makeKey(RenderPurpose purpose,
                    const QString& ownerId = QString(),
                    int64_t frameNumber = 0,
                    float resolutionScale = 1.0f) const
    {
        return QStringLiteral("%1|%2|%3|%4")
            .arg(renderPurposeToString(purpose))
            .arg(ownerId)
            .arg(frameNumber)
            .arg(QString::number(resolutionScale, 'f', 3));
    }

    QString registerSnapshot(const RenderContextSnapshot& snapshot)
    {
        const QString key = snapshot.key.isEmpty()
            ? makeKey(snapshot.purpose, QString(), snapshot.currentFrame, snapshot.resolutionScale)
            : snapshot.key;
        snapshots_[key] = snapshot;
        return key;
    }

    bool contains(const QString& key) const
    {
        return snapshots_.contains(key);
    }

    RenderContextSnapshot snapshot(const QString& key) const
    {
        return snapshots_.value(key);
    }

    RenderContextSnapshot snapshotOrDefault(const QString& key,
                                            const RenderContextSnapshot& fallback = RenderContextSnapshot()) const
    {
        return snapshots_.contains(key) ? snapshots_.value(key) : fallback;
    }

    void clear()
    {
        snapshots_.clear();
    }

private:
    QHash<QString, RenderContextSnapshot> snapshots_;
};

/**
 * @brief レンダリングコンテキストを作成
 */
inline RenderContextPtr createRenderContext(RenderMode mode = RenderMode::Preview)
{
    auto ctx = std::make_shared<RenderContext>();
    ctx->setMode(mode);
    ctx->setInteractive(mode != RenderMode::Final);
    ctx->setResolutionScale(ctx->modeSettings.resolutionScale);
    return ctx;
}

inline RenderContextPtr createRenderContextForPurpose(RenderPurpose purpose,
                                                      RenderMode mode,
                                                      int64_t currentFrame,
                                                      const QSize& viewportSize,
                                                      const QSizeF& canvasSize,
                                                      const RenderROI& roi,
                                                      float resolutionScale = 1.0f,
                                                      bool interactive = true,
                                                      ArtifactCore::ColorSpace colorSpace = ArtifactCore::ColorSpace::sRGB,
                                                      const QPointF& pan = QPointF(0, 0))
{
    auto ctx = createRenderContext(mode);
    ctx->currentFrame = currentFrame;
    ctx->setInteractive(interactive && isInteractiveRenderPurpose(purpose));
    ctx->setResolutionScale(resolutionScale);
    ctx->setColorSpace(colorSpace);
    ctx->viewportSize = viewportSize;
    ctx->canvasSize = canvasSize;
    ctx->pan = pan;
    ctx->setROI(roi);
    return ctx;
}

inline RenderContextPtr createEditorPreviewContext(int64_t currentFrame,
                                                   const QSize& viewportSize,
                                                   const QSizeF& canvasSize,
                                                   const RenderROI& roi,
                                                   float resolutionScale = 1.0f,
                                                   const QPointF& pan = QPointF(0, 0))
{
    return createRenderContextForPurpose(RenderPurpose::EditorPreview,
                                         RenderMode::Preview,
                                         currentFrame,
                                         viewportSize,
                                         canvasSize,
                                         roi,
                                         resolutionScale,
                                         true,
                                         ArtifactCore::ColorSpace::sRGB,
                                         pan);
}

inline RenderContextPtr createFinalExportContext(int64_t currentFrame,
                                                 const QSize& viewportSize,
                                                 const QSizeF& canvasSize,
                                                 const RenderROI& roi,
                                                 float resolutionScale = 1.0f,
                                                 const QPointF& pan = QPointF(0, 0))
{
    return createRenderContextForPurpose(RenderPurpose::FinalExport,
                                         RenderMode::Final,
                                         currentFrame,
                                         viewportSize,
                                         canvasSize,
                                         roi,
                                         resolutionScale,
                                         false,
                                         ArtifactCore::ColorSpace::sRGB,
                                         pan);
}

inline RenderContextPtr createProxyBuildContext(int64_t currentFrame,
                                                const QSize& viewportSize,
                                                const QSizeF& canvasSize,
                                                const RenderROI& roi,
                                                float resolutionScale = 1.0f,
                                                const QPointF& pan = QPointF(0, 0))
{
    return createRenderContextForPurpose(RenderPurpose::ProxyBuild,
                                         RenderMode::Preview,
                                         currentFrame,
                                         viewportSize,
                                         canvasSize,
                                         roi,
                                         resolutionScale,
                                         false,
                                         ArtifactCore::ColorSpace::sRGB,
                                         pan);
}

} // namespace Artifact
