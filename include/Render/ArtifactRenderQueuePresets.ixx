module;
#include <QString>
#include <QStringList>
#include <QVector>
#include <QIcon>

export module Artifact.Render.Queue.Presets;

export namespace Artifact {

/**
 * @brief 出力フォーマットプリセット
 * 
 * After Effects のようにコンテナ＋コーデックのセットを定義
 */
struct ArtifactRenderFormatPreset {
    QString id;           // 内部識別子
    QString name;         // 表示名（例："H.264 MP4"）
    QString container;    // コンテナ形式（例："mp4", "mov", "avi"）
    QString codec;        // コーデック（例："h264", "prores", "png"）
    QString codecProfile; // コーデックプロファイル（例："hq", "4444"）
    QString description;  // 説明
    bool isImageSequence; // 連番画像出力かどうか
    QIcon icon;           // アイコン（オプション）

    // プリセットファクトリ
    static QVector<ArtifactRenderFormatPreset> getStandardPresets();
    
    // 画像シーケンスプリセット
    static QVector<ArtifactRenderFormatPreset> getImageSequencePresets();
    
    // ビデオプリセット
    static QVector<ArtifactRenderFormatPreset> getVideoPresets();
};

/**
 * @brief 出力形式カテゴリ
 */
enum class ArtifactRenderFormatCategory {
    Video,          // ビデオ形式（MP4, MOV 等）
    ImageSequence,  // 連番画像（PNG, JPEG 等）
    Audio,          // オーディオ（WAV, MP3 等）
    Custom          // カスタム
};

/**
 * @brief フォーマットプリセットマネージャー
 */
class ArtifactRenderFormatPresetManager {
public:
    static ArtifactRenderFormatPresetManager& instance();
    
    // すべてのプリセットを取得
    QVector<ArtifactRenderFormatPreset> allPresets() const;
    
    // カテゴリ別にプリセットを取得
    QVector<ArtifactRenderFormatPreset> presetsByCategory(ArtifactRenderFormatCategory category) const;
    
    // ID でプリセットを検索
    const ArtifactRenderFormatPreset* findPresetById(const QString& id) const;
    
    // カスタムプリセットを追加
    void addCustomPreset(const ArtifactRenderFormatPreset& preset);
    
    // カスタムプリセットを削除
    void removeCustomPreset(const QString& id);
    
private:
    ArtifactRenderFormatPresetManager();
    ~ArtifactRenderFormatPresetManager();
    ArtifactRenderFormatPresetManager(const ArtifactRenderFormatPresetManager&) = delete;
    ArtifactRenderFormatPresetManager& operator=(const ArtifactRenderFormatPresetManager&) = delete;
    
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
