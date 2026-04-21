module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>
#include <QRectF>
#include <QHash>
#include <QString>

export module Artifact.Render.ROI;

import Utils.Numeric;

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

/**
 * @brief エフェクトが出力に必要な ROI 拡張のヒント
 */
enum class EffectROIHintKind {
    None,
    Blur,
    Glow,
    Displacement,
    Matte,
    Mask,
    Custom
};

/**
 * @brief エフェクトの ROI 拡張ヒント
 */
struct EffectROIHint {
    EffectROIHintKind kind = EffectROIHintKind::None;
    float expansionPixels = 0.0f;
    float expansionFraction = 0.0f;
    bool requiresFullFrame = false;

    bool isEmpty() const {
        return kind == EffectROIHintKind::None &&
               expansionPixels <= 0.0f &&
               expansionFraction <= 0.0f &&
               !requiresFullFrame;
    }

    RenderROI apply(const RenderROI& base) const {
        if (requiresFullFrame || base.isEmpty()) {
            return base;
        }
        if (isEmpty()) {
            return base;
        }
        float totalExpansion = expansionPixels;
        if (expansionFraction > 0.0f) {
            totalExpansion += std::max(base.width(), base.height()) * expansionFraction;
        }
        return base.expanded(totalExpansion);
    }
};

/**
 * @brief レイヤー変更による無効化領域
 */
struct LayerInvalidationRegion {
    enum class Source {
        Transform,
        Property,
        Content,
        Visibility,
        Effect,
        Precompose,
        Unknown
    };

    Source source = Source::Unknown;
    QRectF region;
    bool requiresFullRedraw = false;
    int64_t frameNumber = 0;
    QString layerId;

    bool isEmpty() const {
        return region.isEmpty() && !requiresFullRedraw;
    }

    RenderROI toROI() const {
        if (requiresFullRedraw) {
            return RenderROI();
        }
        return RenderROI(region);
    }
};

/**
 * @brief タイルの位置を特定するキー
 */
struct TileKey {
    int tileX = 0;
    int tileY = 0;

    bool operator==(const TileKey& other) const {
        return tileX == other.tileX && tileY == other.tileY;
    }
    bool operator!=(const TileKey& other) const { return !(*this == other); }
};

inline size_t qHash(const TileKey& key, size_t seed = 0) noexcept
{
    const size_t x = std::hash<int>{}(key.tileX);
    const size_t y = std::hash<int>{}(key.tileY);
    seed ^= x + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    seed ^= y + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    return seed;
}

/**
 * @brief タイルグリッドの定義
 */
struct TileGrid {
    int tileSize = 256;
    int gridWidth = 0;
    int gridHeight = 0;
    int imageWidth = 0;
    int imageHeight = 0;

    TileGrid() = default;
    TileGrid(int imgW, int imgH, int tileSz = 256)
        : tileSize(tileSz)
        , gridWidth((imgW + tileSz - 1) / tileSz)
        , gridHeight((imgH + tileSz - 1) / tileSz)
        , imageWidth(imgW)
        , imageHeight(imgH)
    {}

    TileKey keyForPixel(int px, int py) const {
        return TileKey{
            std::clamp(px / tileSize, 0, gridWidth - 1),
            std::clamp(py / tileSize, 0, gridHeight - 1)
        };
    }

    QRectF tileRect(const TileKey& key) const {
        return QRectF(
            key.tileX * tileSize,
            key.tileY * tileSize,
            tileSize,
            tileSize
        );
    }

    QRectF tileRectClamped(const TileKey& key) const {
        QRectF r = tileRect(key);
        const qreal right = ArtifactCore::min_same(r.right(), static_cast<qreal>(imageWidth));
        const qreal bottom = ArtifactCore::min_same(r.bottom(), static_cast<qreal>(imageHeight));
        return QRectF(r.left(), r.top(), right - r.left(), bottom - r.top());
    }

    std::vector<TileKey> tilesIntersecting(const RenderROI& roi) const {
        std::vector<TileKey> result;
        if (roi.isEmpty() || imageWidth <= 0 || imageHeight <= 0) {
            return result;
        }
        const int minX = std::max(0, static_cast<int>(roi.x()) / tileSize);
        const int minY = std::max(0, static_cast<int>(roi.y()) / tileSize);
        const int maxX = std::min(gridWidth - 1, static_cast<int>(roi.right()) / tileSize);
        const int maxY = std::min(gridHeight - 1, static_cast<int>(roi.bottom()) / tileSize);
        for (int ty = minY; ty <= maxY; ++ty) {
            for (int tx = minX; tx <= maxX; ++tx) {
                result.push_back(TileKey{tx, ty});
            }
        }
        return result;
    }

    bool isValid() const {
        return tileSize > 0 && gridWidth > 0 && gridHeight > 0;
    }
};

/**
 * @brief dirty region を蓄積するアキュムレータ
 */
class DirtyRegionAccumulator {
public:
    void add(const LayerInvalidationRegion& region) {
        if (region.requiresFullRedraw) {
            dirtyRect_ = QRectF();
            requiresFullRedraw_ = true;
            return;
        }
        if (region.isEmpty()) {
            return;
        }
        if (dirtyRect_.isEmpty()) {
            dirtyRect_ = region.region;
        } else {
            dirtyRect_ = dirtyRect_.united(region.region);
        }
    }

    void reset() {
        dirtyRect_ = QRectF();
        requiresFullRedraw_ = false;
        frameNumber_ = 0;
    }

    RenderROI accumulatedROI() const {
        if (requiresFullRedraw_ || dirtyRect_.isEmpty()) {
            return RenderROI();
        }
        return RenderROI(dirtyRect_);
    }

    bool needsFullRedraw() const { return requiresFullRedraw_; }
    QRectF dirtyRect() const { return dirtyRect_; }
    int64_t frameNumber() const { return frameNumber_; }
    void setFrameNumber(int64_t f) { frameNumber_ = f; }

private:
    QRectF dirtyRect_;
    bool requiresFullRedraw_ = false;
    int64_t frameNumber_ = 0;
};

/**
 * @brief レンダーダメージ（再描画必要性）を追跡するトラッカー
 */
class RenderDamageTracker {
public:
    void markDirty(const QString& layerId, const LayerInvalidationRegion& region) {
        dirtyRegions_[layerId].add(region);
    }

    void markFullRedraw(const QString& layerId) {
        LayerInvalidationRegion region;
        region.requiresFullRedraw = true;
        region.layerId = layerId;
        markDirty(layerId, region);
    }

    void clear(const QString& layerId) {
        dirtyRegions_.remove(layerId);
    }

    void clearAll() {
        dirtyRegions_.clear();
    }

    RenderROI dirtyROI(const QString& layerId) const {
        if (auto it = dirtyRegions_.find(layerId); it != dirtyRegions_.end()) {
            return it->accumulatedROI();
        }
        return RenderROI();
    }

    RenderROI combinedDirtyROI() const {
        RenderROI result;
        for (auto it = dirtyRegions_.cbegin(); it != dirtyRegions_.cend(); ++it) {
            const auto& accumulator = it.value();
            if (accumulator.needsFullRedraw()) {
                return RenderROI();
            }
            const auto region = accumulator.accumulatedROI();
            if (!region.isEmpty()) {
                result = result.united(region);
            }
        }
        return result;
    }

    bool hasDirtyRegions() const {
        return !dirtyRegions_.isEmpty();
    }

    bool needsFullRedraw(const QString& layerId) const {
        if (auto it = dirtyRegions_.find(layerId); it != dirtyRegions_.end()) {
            return it->needsFullRedraw();
        }
        return false;
    }

    std::vector<TileKey> dirtyTiles(const TileGrid& grid, const QString& layerId = QString()) const {
        if (!grid.isValid()) {
            return {};
        }
        RenderROI roi;
        if (layerId.isEmpty()) {
            roi = combinedDirtyROI();
        } else {
            roi = dirtyROI(layerId);
        }
        if (roi.isEmpty() || needsFullRedraw(layerId)) {
            return {};
        }
        return grid.tilesIntersecting(roi);
    }

private:
    QHash<QString, DirtyRegionAccumulator> dirtyRegions_;
};

/**
 * @brief スパースなタイルサーフェス
 *
 * 必要なタイルだけを確保する遅延確保型バッファ。
 * full-frame の巨大バッファを避けて、ROI 単位でメモリを割り当てる。
 */
class SparseTileSurface {
public:
    explicit SparseTileSurface(const TileGrid& grid = TileGrid())
        : grid_(grid)
    {}

    void setGrid(const TileGrid& grid) {
        grid_ = grid;
        tiles_.clear();
    }

    const TileGrid& grid() const { return grid_; }

    bool hasTile(const TileKey& key) const {
        return tiles_.contains(key);
    }

    void* tileData(const TileKey& key) {
        return tiles_.value(key, nullptr);
    }

    const void* tileData(const TileKey& key) const {
        return tiles_.value(key, nullptr);
    }

    void ensureTile(const TileKey& key) {
        if (!tiles_.contains(key)) {
            const QRectF r = grid_.tileRectClamped(key);
            const int bytesPerPixel = 16;
            const int w = static_cast<int>(std::ceil(r.width()));
            const int h = static_cast<int>(std::ceil(r.height()));
            tiles_[key] = new std::vector<uint8_t>(w * h * bytesPerPixel, 0);
        }
    }

    void releaseTile(const TileKey& key) {
        if (auto it = tiles_.find(key); it != tiles_.end()) {
            delete it.value();
            tiles_.erase(it);
        }
    }

    void clear() {
        for (auto* data : tiles_) {
            delete data;
        }
        tiles_.clear();
    }

    int tileCount() const { return static_cast<int>(tiles_.size()); }
    std::vector<TileKey> activeTileKeys() const {
        std::vector<TileKey> keys;
        keys.reserve(tiles_.size());
        for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
            keys.push_back(it.key());
        }
        return keys;
    }

private:
    TileGrid grid_;
    QHash<TileKey, std::vector<uint8_t>*> tiles_;
};

/**
 * @brief タイルレンダースケジューラ
 *
 * タイル単位でレンダリングタスクをキューイングし、
 * 優先度付きで実行する（将来的にマルチスレッド対応）。
 */
class TileRenderScheduler {
public:
    struct TileTask {
        TileKey key;
        int priority = 0;
        std::function<void(const TileKey&)> renderFn;
    };

    void enqueue(const TileTask& task) {
        tasks_.push_back(task);
    }

    void enqueueTiles(const std::vector<TileKey>& keys,
                      std::function<void(const TileKey&)> fn,
                      int priority = 0) {
        for (const auto& key : keys) {
            tasks_.push_back(TileTask{key, priority, fn});
        }
    }

    void processAll() {
        std::sort(tasks_.begin(), tasks_.end(),
                  [](const TileTask& a, const TileTask& b) {
                      return a.priority > b.priority;
                  });
        for (const auto& task : tasks_) {
            if (task.renderFn) {
                task.renderFn(task.key);
            }
        }
        tasks_.clear();
    }

    void clear() { tasks_.clear(); }
    int pendingCount() const { return static_cast<int>(tasks_.size()); }

private:
    std::vector<TileTask> tasks_;
};

} // namespace Artifact
