module;
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <wobjectdefs.h>

export module Artifact.Widgets.RenderQueuePresetSelector;

import Artifact.Render.Queue.Presets;

export namespace Artifact {

/**
 * @brief フォーマットプリセット選択ウィジェット
 * 
 * After Effects のようにコンテナ＋コーデックのセットを選択可能にする
 */
class ArtifactRenderQueuePresetSelector : public QWidget {
    W_OBJECT(ArtifactRenderQueuePresetSelector)
public:
    explicit ArtifactRenderQueuePresetSelector(QWidget* parent = nullptr);
    ~ArtifactRenderQueuePresetSelector() override;
    
    // 選択中のプリセット ID を取得
    QString selectedPresetId() const;
    
    // 選択中のプリセット情報を取得
    const ArtifactRenderFormatPreset* selectedPreset() const;
    
    // プリセットを設定
    void setSelectedPresetId(const QString& presetId);
    
    // カテゴリフィルターを設定
    void setCategoryFilter(ArtifactRenderFormatCategory category);
    
    // 複数選択モード
    void setMultiSelectMode(bool enabled);
    bool isMultiSelectMode() const;
    
    // 選択中のプリセット ID リスト（複数選択モード）
    QVector<QString> selectedPresetIds() const;
    
signals:
    void presetSelected(const QString& presetId);
    void presetsConfirmed(const QVector<QString>& presetIds);
    void canceled();
    
private:
    class Impl;
    Impl* impl_;
};

/**
 * @brief フォーマットプリセット選択ダイアログ
 */
class ArtifactRenderQueuePresetDialog : public QWidget {
    W_OBJECT(ArtifactRenderQueuePresetDialog)
public:
    explicit ArtifactRenderQueuePresetDialog(QWidget* parent = nullptr);
    ~ArtifactRenderQueuePresetDialog() override;
    
    // 選択されたプリセット ID リストを取得
    QVector<QString> selectedPresetIds() const;
    
    // プリセット ID を直接設定（テスト用）
    void setInitialSelection(const QVector<QString>& presetIds);
    
private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
