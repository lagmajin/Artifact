module;
#include <QObject>
#include <wobjectdefs.h>
#include <algorithm>
#include <cmath>

export module Artifact.LOD.Manager;

export namespace Artifact {

/**
 * @brief LOD (Level of Detail) マネージャー
 * 
 * ズームレベルに応じて詳細度を自動切り替え
 */
class LODManager : public QObject {
    W_OBJECT(LODManager)

public:
    enum class DetailLevel {
        Low,    // 簡略化された描画（ズーム 0-25%）
        Medium, // 標準的な描画（ズーム 25-75%）
        High    // 高詳細な描画（ズーム 75-100%）
    };
    W_ENUM(DetailLevel)

    explicit LODManager(QObject* parent = nullptr);
    ~LODManager();

    /**
     * @brief ズームレベルから詳細度を取得
     * @param zoom ズーム倍率（1.0 = 100%）
     * @return 適切な詳細度
     */
    Q_INVOKABLE DetailLevel getDetailLevel(float zoom) const;

    /**
     * @brief 詳細度の閾値を設定
     * @param lowThreshold 低詳細度の最大ズーム（デフォルト：0.25）
     * @param mediumThreshold 中詳細度の最大ズーム（デフォルト：0.75）
     */
    Q_INVOKABLE void setThresholds(float lowThreshold, float mediumThreshold);

    /**
     * @brief 現在の低詳細度閾値を取得
     */
    Q_INVOKABLE float lowThreshold() const { return lowThreshold_; }

    /**
     * @brief 現在の中詳細度閾値を取得
     */
    Q_INVOKABLE float mediumThreshold() const { return mediumThreshold_; }

    /**
     * @brief ズームレベルから LOD ファクターを計算（0.0-1.0）
     * @param zoom ズーム倍率
     * @return LOD ファクター（0.0=Low, 1.0=High）
     */
    Q_INVOKABLE float calculateLODFactor(float zoom) const;

public:
    /**
     * @brief 詳細度が変更されたときに発火
     * @param oldLevel 以前の詳細度 (int cast)
     * @param newLevel 新しい詳細度 (int cast)
     */
    void detailLevelChanged(int oldLevel, int newLevel) W_SIGNAL(detailLevelChanged, oldLevel, newLevel);

    /**
     * @brief 閾値が変更されたときに発火
     */
    void thresholdsChanged() W_SIGNAL(thresholdsChanged);

private:
    float lowThreshold_ = 0.25f;
    float mediumThreshold_ = 0.75f;
    DetailLevel currentLevel_ = DetailLevel::Medium;
};

} // namespace Artifact
