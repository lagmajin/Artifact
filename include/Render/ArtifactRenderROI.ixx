module;
#include <utility>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>
#include <QPointF>
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

/**
 * @brief 1 frame内で処理するdirty tileの固定容量プラン
 *
 * 見えているtileを先に選び、残りはdeferとして呼び出し側へ返す。
 * キューの無制限成長やフレーム中のヒープ確保を避けるため、この型は
 * 値型の固定容量にする。
 */
struct BoundedTileRefinementPlan {
    static constexpr int MaxTiles = 16;

    std::array<TileKey, MaxTiles> tiles{};
    int scheduledCount = 0;
    int deferredCount = 0;
    int visibleDirtyTileCount = 0;
    // Row-major ordinals in the complete grid for stable cursor reuse as the
    // visible or dirty ROI changes between frames.
    std::uint64_t firstTileOrdinal = 0;
    std::uint64_t nextTileOrdinal = 0;
    bool requiresFullRedraw = false;

    bool empty() const { return scheduledCount == 0; }
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

    static int saturatedTileCount(int minX, int minY, int maxX, int maxY) {
        const std::int64_t width = static_cast<std::int64_t>(maxX) - minX + 1;
        const std::int64_t height = static_cast<std::int64_t>(maxY) - minY + 1;
        const std::int64_t count = width * height;
        return static_cast<int>(std::min<std::int64_t>(
            count, std::numeric_limits<int>::max()));
    }

    bool tileRangeForROI(const RenderROI& roi, int& minX, int& minY,
                         int& maxX, int& maxY) const {
        if (!isValid() || roi.isEmpty() ||
            !std::isfinite(roi.x()) || !std::isfinite(roi.y()) ||
            !std::isfinite(roi.right()) || !std::isfinite(roi.bottom())) {
            return false;
        }

        const double left = std::clamp(static_cast<double>(roi.x()), 0.0,
                                       static_cast<double>(imageWidth));
        const double top = std::clamp(static_cast<double>(roi.y()), 0.0,
                                      static_cast<double>(imageHeight));
        const double right = std::clamp(static_cast<double>(roi.right()), 0.0,
                                        static_cast<double>(imageWidth));
        const double bottom = std::clamp(static_cast<double>(roi.bottom()), 0.0,
                                         static_cast<double>(imageHeight));
        if (right <= left || bottom <= top) {
            return false;
        }

        minX = static_cast<int>(std::floor(left / tileSize));
        minY = static_cast<int>(std::floor(top / tileSize));
        maxX = static_cast<int>(std::ceil(right / tileSize)) - 1;
        maxY = static_cast<int>(std::ceil(bottom / tileSize)) - 1;
        return true;
    }

    TileGrid() = default;
    TileGrid(int imgW, int imgH, int tileSz = 256)
    {
        if (imgW <= 0 || imgH <= 0 || tileSz <= 0) {
            return;
        }
        tileSize = tileSz;
        imageWidth = imgW;
        imageHeight = imgH;
        gridWidth = 1 + (imgW - 1) / tileSz;
        gridHeight = 1 + (imgH - 1) / tileSz;
    }

    TileKey keyForPixel(int px, int py) const {
        if (!isValid()) {
            return {};
        }
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

    int countTilesIntersecting(const RenderROI& roi) const {
        int minX = 0;
        int minY = 0;
        int maxX = -1;
        int maxY = -1;
        if (!tileRangeForROI(roi, minX, minY, maxX, maxY)) {
            return 0;
        }
        return saturatedTileCount(minX, minY, maxX, maxY);
    }

    BoundedTileRefinementPlan makeBoundedPlan(
        const RenderROI& roi,
        int maxTiles = BoundedTileRefinementPlan::MaxTiles,
        std::uint64_t startOrdinal = 0) const {
        BoundedTileRefinementPlan plan;
        if (roi.isEmpty() || !isValid()) {
            return plan;
        }
        const int capacity = std::clamp(
            maxTiles, 0, BoundedTileRefinementPlan::MaxTiles);
        int minX = 0;
        int minY = 0;
        int maxX = -1;
        int maxY = -1;
        if (!tileRangeForROI(roi, minX, minY, maxX, maxY)) {
            return plan;
        }
        const std::uint64_t rangeWidth =
            static_cast<std::uint64_t>(maxX - minX + 1);
        const std::uint64_t rangeHeight =
            static_cast<std::uint64_t>(maxY - minY + 1);
        const std::uint64_t totalTileCount = rangeWidth * rangeHeight;
        if (totalTileCount == 0) {
            return plan;
        }
        plan.visibleDirtyTileCount =
            saturatedTileCount(minX, minY, maxX, maxY);
        const std::uint64_t gridTileCount =
            static_cast<std::uint64_t>(gridWidth) * gridHeight;
        const std::uint64_t normalizedStart = startOrdinal % gridTileCount;
        const int startX = static_cast<int>(normalizedStart % gridWidth);
        const int startY = static_cast<int>(normalizedStart / gridWidth);
        int tileX = minX;
        int tileY = minY;
        if (startY >= minY && startY <= maxY) {
            tileY = startY;
            if (startX >= minX && startX <= maxX) {
                tileX = startX;
            } else if (startX > maxX) {
                tileY = startY < maxY ? startY + 1 : minY;
            }
        }
        plan.firstTileOrdinal =
            static_cast<std::uint64_t>(tileY) * gridWidth + tileX;
        // Keep each batch on one row so its GPU recompose ROI stays compact.
        const int scheduledRow = tileY;
        while (plan.scheduledCount < capacity &&
               tileY == scheduledRow) {
            plan.tiles[static_cast<size_t>(plan.scheduledCount++)] =
                TileKey{tileX, tileY};
            if (tileX < maxX) {
                ++tileX;
            } else {
                break;
            }
        }
        if (plan.scheduledCount > 0) {
            const TileKey lastTile =
                plan.tiles[static_cast<size_t>(plan.scheduledCount - 1)];
            const std::uint64_t lastOrdinal =
                static_cast<std::uint64_t>(lastTile.tileY) * gridWidth +
                lastTile.tileX;
            plan.nextTileOrdinal = (lastOrdinal + 1) % gridTileCount;
        } else {
            plan.nextTileOrdinal = normalizedStart;
        }
        plan.deferredCount = plan.visibleDirtyTileCount - plan.scheduledCount;
        return plan;
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
    // Keep disjoint damage bounded without allocating during frame updates.
    static constexpr int MaxDirtyRegions = 16;

    void add(const LayerInvalidationRegion& region) {
        if (region.requiresFullRedraw) {
            dirtyRect_ = QRectF();
            dirtyRegionCount_ = 0;
            requiresFullRedraw_ = true;
            return;
        }
        if (region.isEmpty()) {
            return;
        }

        QRectF pendingRegion = region.region.normalized();
        if (!isFiniteNonEmpty(pendingRegion)) {
            dirtyRect_ = QRectF();
            dirtyRegionCount_ = 0;
            requiresFullRedraw_ = true;
            return;
        }

        for (int index = 0; index < dirtyRegionCount_;) {
            if (!dirtyRegions_[static_cast<size_t>(index)].intersects(
                    pendingRegion)) {
                ++index;
                continue;
            }
            pendingRegion = pendingRegion.united(
                dirtyRegions_[static_cast<size_t>(index)]);
            removeRegionAt(index);
            index = 0;
        }

        if (dirtyRegionCount_ == MaxDirtyRegions) {
            // Coalescing can cause extra redraws, but never drops pending damage.
            for (int index = 0; index < dirtyRegionCount_; ++index) {
                pendingRegion = pendingRegion.united(
                    dirtyRegions_[static_cast<size_t>(index)]);
            }
            dirtyRegionCount_ = 1;
            dirtyRegions_[0] = pendingRegion;
        } else {
            dirtyRegions_[static_cast<size_t>(dirtyRegionCount_++)] =
                pendingRegion;
        }
        recomputeDirtyRect();
    }

    void consume(const QRectF& region) {
        if (requiresFullRedraw_ || dirtyRegionCount_ == 0 ||
            !isFiniteNonEmpty(region)) {
            return;
        }

        std::array<QRectF, MaxDirtyRegions * 4> remaining{};
        int remainingCount = 0;
        const QRectF cut = region.normalized();
        for (int index = 0; index < dirtyRegionCount_; ++index) {
            const QRectF source = dirtyRegions_[static_cast<size_t>(index)];
            const QRectF intersection = source.intersected(cut);
            if (intersection.isEmpty()) {
                remaining[static_cast<size_t>(remainingCount++)] = source;
                continue;
            }

            appendIfNonEmpty(
                remaining, remainingCount,
                QRectF(QPointF(source.left(), source.top()),
                       QPointF(source.right(), intersection.top())));
            appendIfNonEmpty(
                remaining, remainingCount,
                QRectF(QPointF(source.left(), intersection.bottom()),
                       QPointF(source.right(), source.bottom())));
            appendIfNonEmpty(
                remaining, remainingCount,
                QRectF(QPointF(source.left(), intersection.top()),
                       QPointF(intersection.left(), intersection.bottom())));
            appendIfNonEmpty(
                remaining, remainingCount,
                QRectF(QPointF(intersection.right(), intersection.top()),
                       QPointF(source.right(), intersection.bottom())));
        }

        dirtyRegionCount_ = 0;
        for (int index = 0; index < remainingCount; ++index) {
            QRectF pendingRegion = remaining[static_cast<size_t>(index)];
            for (int existing = 0; existing < dirtyRegionCount_;) {
                if (!dirtyRegions_[static_cast<size_t>(existing)].intersects(
                        pendingRegion)) {
                    ++existing;
                    continue;
                }
                pendingRegion = pendingRegion.united(
                    dirtyRegions_[static_cast<size_t>(existing)]);
                removeRegionAt(existing);
                existing = 0;
            }
            if (dirtyRegionCount_ == MaxDirtyRegions) {
                for (int existing = 0; existing < dirtyRegionCount_; ++existing) {
                    pendingRegion = pendingRegion.united(
                        dirtyRegions_[static_cast<size_t>(existing)]);
                }
                dirtyRegionCount_ = 1;
                dirtyRegions_[0] = pendingRegion;
            } else {
                dirtyRegions_[static_cast<size_t>(dirtyRegionCount_++)] =
                    pendingRegion;
            }
        }
        recomputeDirtyRect();
    }

    void reset() {
        dirtyRect_ = QRectF();
        dirtyRegionCount_ = 0;
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
    friend class RenderDamageTracker;

    int dirtyRegionCount() const { return dirtyRegionCount_; }
    const QRectF& dirtyRegionAt(int index) const {
        return dirtyRegions_[static_cast<size_t>(index)];
    }

    static bool isFiniteNonEmpty(const QRectF& region) {
        return std::isfinite(region.x()) && std::isfinite(region.y()) &&
               std::isfinite(region.width()) &&
               std::isfinite(region.height()) &&
               std::isfinite(region.right()) &&
               std::isfinite(region.bottom()) && !region.isEmpty();
    }

    template <size_t Capacity>
    static void appendIfNonEmpty(std::array<QRectF, Capacity>& regions,
                                 int& count, const QRectF& region) {
        if (isFiniteNonEmpty(region) && count < static_cast<int>(Capacity)) {
            regions[static_cast<size_t>(count++)] = region;
        }
    }

    void removeRegionAt(int index) {
        for (int moveIndex = index + 1; moveIndex < dirtyRegionCount_;
             ++moveIndex) {
            dirtyRegions_[static_cast<size_t>(moveIndex - 1)] =
                dirtyRegions_[static_cast<size_t>(moveIndex)];
        }
        --dirtyRegionCount_;
    }

    void recomputeDirtyRect() {
        dirtyRect_ = QRectF();
        for (int index = 0; index < dirtyRegionCount_; ++index) {
            const QRectF& region = dirtyRegions_[static_cast<size_t>(index)];
            dirtyRect_ = dirtyRect_.isEmpty()
                             ? region
                             : dirtyRect_.united(region);
        }
    }

    QRectF dirtyRect_;
    std::array<QRectF, MaxDirtyRegions> dirtyRegions_{};
    int dirtyRegionCount_ = 0;
    bool requiresFullRedraw_ = false;
    int64_t frameNumber_ = 0;
};

/**
 * @brief レンダーダメージ（再描画必要性）を追跡するトラッカー
 */
class RenderDamageTracker {
public:
    void markFullRedraw() {
        requiresFullRedraw_ = true;
    }

    void markDirty(const QString& layerId, const LayerInvalidationRegion& region) {
        if (region.isEmpty()) {
            return;
        }
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

    void consumeRegion(const QRectF& region) {
        if (requiresFullRedraw_) {
            return;
        }
        auto it = dirtyRegions_.begin();
        while (it != dirtyRegions_.end()) {
            it.value().consume(region);
            if (it.value().dirtyRect().isEmpty() &&
                !it.value().needsFullRedraw()) {
                it = dirtyRegions_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clearAll() {
        dirtyRegions_.clear();
        requiresFullRedraw_ = false;
    }

    RenderROI dirtyROI(const QString& layerId) const {
        if (auto it = dirtyRegions_.find(layerId); it != dirtyRegions_.end()) {
            return it->accumulatedROI();
        }
        return RenderROI();
    }

    RenderROI combinedDirtyROI() const {
        if (requiresFullRedraw_) {
            return RenderROI();
        }
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
        return requiresFullRedraw_ || !dirtyRegions_.isEmpty();
    }

    bool combinedNeedsFullRedraw() const {
        if (requiresFullRedraw_) {
            return true;
        }
        for (auto it = dirtyRegions_.cbegin(); it != dirtyRegions_.cend(); ++it) {
            if (it.value().needsFullRedraw()) {
                return true;
            }
        }
        return false;
    }

    int dirtyLayerCount() const {
        return dirtyRegions_.size() + (requiresFullRedraw_ ? 1 : 0);
    }

    bool needsFullRedraw(const QString& layerId) const {
        if (requiresFullRedraw_) {
            return true;
        }
        if (auto it = dirtyRegions_.find(layerId); it != dirtyRegions_.end()) {
            return it->needsFullRedraw();
        }
        return false;
    }

    int dirtyTileCount(const TileGrid& grid, const QString& layerId = QString()) const {
        if (!grid.isValid()) {
            return 0;
        }
        const RenderROI roi = layerId.isEmpty() ? combinedDirtyROI()
                                                 : dirtyROI(layerId);
        const bool fullRedraw = layerId.isEmpty()
                                    ? combinedNeedsFullRedraw()
                                    : needsFullRedraw(layerId);
        if (roi.isEmpty() || fullRedraw) {
            return 0;
        }
        return grid.countTilesIntersecting(roi);
    }

    BoundedTileRefinementPlan makeBoundedDirtyTilePlan(
        const TileGrid& grid, const RenderROI& visibleROI,
        int maxTiles = BoundedTileRefinementPlan::MaxTiles,
        const QString& layerId = QString(),
        std::uint64_t startOrdinal = 0) const {
        BoundedTileRefinementPlan plan;
        if (!grid.isValid()) {
            return plan;
        }
        const bool fullRedraw = layerId.isEmpty() ? combinedNeedsFullRedraw()
                                                   : needsFullRedraw(layerId);
        if (fullRedraw) {
            plan.requiresFullRedraw = true;
            return plan;
        }
        const int capacity = std::clamp(
            maxTiles, 0, BoundedTileRefinementPlan::MaxTiles);
        const std::uint64_t gridTileCount =
            static_cast<std::uint64_t>(grid.gridWidth) * grid.gridHeight;
        if (gridTileCount == 0) {
            return plan;
        }
        const std::uint64_t normalizedStart = startOrdinal % gridTileCount;
        std::uint64_t bestFirstTileDistance = gridTileCount;
        BoundedTileRefinementPlan bestRegionPlan;

        // Pick one cursor-nearest damage rectangle instead of combining distant
        // regions into a large GPU recompose rectangle.
        const auto addAccumulator = [&](const DirtyRegionAccumulator& accumulator) {
            for (int regionIndex = 0;
                 regionIndex < accumulator.dirtyRegionCount(); ++regionIndex) {
                const RenderROI regionROI(
                    accumulator.dirtyRegionAt(regionIndex));
                const BoundedTileRefinementPlan regionPlan =
                    grid.makeBoundedPlan(regionROI.intersected(visibleROI),
                                         capacity, normalizedStart);
                if (regionPlan.empty()) {
                    continue;
                }
                const std::uint64_t ordinal = regionPlan.firstTileOrdinal;
                const std::uint64_t distance =
                    ordinal >= normalizedStart
                        ? ordinal - normalizedStart
                        : gridTileCount - normalizedStart + ordinal;
                if (distance < bestFirstTileDistance) {
                    bestFirstTileDistance = distance;
                    bestRegionPlan = regionPlan;
                }
            }
        };

        if (layerId.isEmpty()) {
            for (auto it = dirtyRegions_.cbegin(); it != dirtyRegions_.cend();
                 ++it) {
                addAccumulator(it.value());
            }
        } else {
            const auto it = dirtyRegions_.constFind(layerId);
            if (it != dirtyRegions_.cend()) {
                addAccumulator(it.value());
            }
        }
        plan.visibleDirtyTileCount =
            bestRegionPlan.visibleDirtyTileCount;
        plan.tiles = bestRegionPlan.tiles;
        plan.scheduledCount = bestRegionPlan.scheduledCount;
        plan.firstTileOrdinal = bestRegionPlan.firstTileOrdinal;
        plan.nextTileOrdinal = bestRegionPlan.scheduledCount > 0
                                   ? bestRegionPlan.nextTileOrdinal
                                   : normalizedStart;
        plan.deferredCount = std::max(
            0, plan.visibleDirtyTileCount - plan.scheduledCount);
        return plan;
    }

private:
    QHash<QString, DirtyRegionAccumulator> dirtyRegions_;
    bool requiresFullRedraw_ = false;
};

} // namespace Artifact
