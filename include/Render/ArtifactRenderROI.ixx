module;
#include <QRectF>
#include <algorithm>
#include <cmath>

export module Artifact.Render.ROI;

export namespace Artifact {

/**
 * @brief レンダリングモード
 */
enum class RenderMode {
    Editor,   // エディタ表示（リアルタイム重視）
    Preview,  // プレビュー表示（バランス）
    Final     // 最終レンダリング（品質重視）
};

/**
 * @brief レンダリングモード設定
 */
struct RenderModeSettings {
    // ROI 計算
    float roiExpansionFactor = 0.0f;  // ROI 拡張係数（ピクセル）
    
    // 解像度
    float resolutionScale = 1.0f;     // 解像度スケール
    
    // 最適化
    bool useScissorTest = true;       // Scissor テスト使用
    bool useROICache = true;          // ROI キャッシュ使用
    bool skipEmptyROI = true;         // 空 ROI スキップ
    
    // 品質
    int sampleCount = 1;              // サンプル数
    bool enableEffects = true;        // エフェクト有効
};

/**
 * @brief モード別デフォルト設定を取得
 */
inline RenderModeSettings getModeSettings(RenderMode mode)
{
    switch (mode) {
        case RenderMode::Editor:
            return RenderModeSettings {
                .roiExpansionFactor = 0.0f,
                .resolutionScale = 0.5f,
                .useScissorTest = true,
                .useROICache = true,
                .skipEmptyROI = true,
                .sampleCount = 1,
                .enableEffects = false
            };
            
        case RenderMode::Preview:
            return RenderModeSettings {
                .roiExpansionFactor = 1.0f,
                .resolutionScale = 1.0f,
                .useScissorTest = true,
                .useROICache = true,
                .skipEmptyROI = true,
                .sampleCount = 2,
                .enableEffects = true
            };
            
        case RenderMode::Final:
            return RenderModeSettings {
                .roiExpansionFactor = 2.0f,
                .resolutionScale = 1.0f,
                .useScissorTest = false,
                .useROICache = false,
                .skipEmptyROI = false,
                .sampleCount = 4,
                .enableEffects = true
            };
    }
    
    return RenderModeSettings();
}

/**
 * @brief ROI (Region of Interest) 構造体
 * 
 * レンダリング対象の矩形領域（composition pixel coordinates）
 */
struct RenderROI {
    QRectF rect;  // 矩形（composition 座標系）
    
    /**
     * @brief デフォルトコンストラクタ（空 ROI）
     */
    RenderROI() = default;
    
    /**
     * @brief 矩形を指定して作成
     */
    explicit RenderROI(const QRectF& r) : rect(r) {}
    
    /**
     * @brief 座標を指定して作成
     */
    RenderROI(float x, float y, float w, float h) 
        : rect(x, y, w, h) {}
    
    /**
     * @brief 空かどうかを判定
     */
    bool isEmpty() const {
        return rect.isEmpty() || 
               rect.width() <= 0.0 || 
               rect.height() <= 0.0;
    }
    
    /**
     * @brief 面積を計算
     */
    float area() const {
        return rect.width() * rect.height();
    }
    
    /**
     * @brief 指定ピクセル分拡張
     * @param pixels 拡張量（ピクセル）
     * @return 拡張された ROI
     */
    RenderROI expanded(float pixels) const {
        if (isEmpty()) {
            return *this;
        }
        return RenderROI(
            rect.adjusted(-pixels, -pixels, pixels, pixels)
        );
    }
    
    /**
     * @brief 他の ROI との交差を取得
     * @param other 交差する ROI
     * @return 交差された ROI
     */
    RenderROI intersected(const RenderROI& other) const {
        return RenderROI(
            rect.intersected(other.rect)
        );
    }
    
    /**
     * @brief 他の ROI との結合を取得
     * @param other 結合する ROI
     * @return 結合された ROI
     */
    RenderROI united(const RenderROI& other) const {
        if (isEmpty()) {
            return other;
        }
        if (other.isEmpty()) {
            return *this;
        }
        return RenderROI(
            rect.united(other.rect)
        );
    }
    
    /**
     * @brief 指定係数でスケーリング
     * @param factor スケール係数（0.0-1.0）
     * @return スケーリングされた ROI
     */
    RenderROI scaled(float factor) const {
        if (isEmpty() || factor <= 0.0f) {
            return RenderROI();
        }
        
        float newW = rect.width() * factor;
        float newH = rect.height() * factor;
        float newX = rect.x() + (rect.width() - newW) / 2.0f;
        float newY = rect.y() + (rect.height() - newH) / 2.0f;
        
        return RenderROI(newX, newY, newW, newH);
    }
    
    /**
     * @brief 指定サイズにリサイズ（アスペクト比維持）
     * @param maxWidth 最大幅
     * @param maxHeight 最大高さ
     * @return リサイズされた ROI
     */
    RenderROI fitted(float maxWidth, float maxHeight) const {
        if (isEmpty()) {
            return *this;
        }
        
        float scaleX = maxWidth / rect.width();
        float scaleY = maxHeight / rect.height();
        float scale = std::min(scaleX, scaleY);
        
        return scaled(std::min(1.0f, scale));
    }
    
    /**
     * @brief 有効な ROI かどうかを判定
     */
    bool isValid() const {
        return !isEmpty() && 
               rect.x() >= -10000.0f && 
               rect.y() >= -10000.0f &&
               rect.width() <= 20000.0f && 
               rect.height() <= 20000.0f;
    }
    
    /**
     * @brief 整数座標に変換（切り上げ）
     */
    QRect toAlignedRect() const {
        return rect.toAlignedRect();
    }
    
    /**
     * @brief 中心点を取得
     */
    QPointF center() const {
        return rect.center();
    }
    
    /**
     * @brief 左端の X 座標
     */
    float left() const { return rect.left(); }
    
    /**
     * @brief 右端の X 座標
     */
    float right() const { return rect.right(); }
    
    /**
     * @brief 上端の Y 座標
     */
    float top() const { return rect.top(); }
    
    /**
     * @brief 下端の Y 座標
     */
    float bottom() const { return rect.bottom(); }
    
    /**
     * @brief 幅
     */
    float width() const { return rect.width(); }
    
    /**
     * @brief 高さ
     */
    float height() const { return rect.height(); }
    
    /**
     * @brief X 座標
     */
    float x() const { return rect.x(); }
    
    /**
     * @brief Y 座標
     */
    float y() const { return rect.y(); }
};

/**
 * @brief ROI が交差しているか判定
 */
inline bool intersects(const RenderROI& a, const RenderROI& b)
{
    return !a.isEmpty() && !b.isEmpty() && a.rect.intersects(b.rect);
}

/**
 * @brief ROI が点を含んでいるか判定
 */
inline bool contains(const RenderROI& roi, const QPointF& point)
{
    return !roi.isEmpty() && roi.rect.contains(point);
}

/**
 * @brief 複数 ROI の境界を計算
 */
inline RenderROI boundingRect(const std::vector<RenderROI>& rois)
{
    if (rois.empty()) {
        return RenderROI();
    }
    
    RenderROI result = rois[0];
    for (size_t i = 1; i < rois.size(); ++i) {
        result = result.united(rois[i]);
    }
    
    return result;
}

} // namespace Artifact
