module;

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPointer>
#include <wobjectdefs.h>

export module Artifact.Widgets.ObjectReference;

import Utils.Id;

export namespace Artifact {

class ArtifactObjectReferenceWidget : public QWidget {
    W_OBJECT(ArtifactObjectReferenceWidget)
public:
    explicit ArtifactObjectReferenceWidget(QWidget* parent = nullptr);
    ~ArtifactObjectReferenceWidget() override;
    
    // 設定
    void setReferenceType(const QString& typeName);
    void setCurrentReferenceId(const ArtifactCore::LayerID& id);
    void setAllowNull(bool allow);
    
    // 取得
    ArtifactCore::LayerID currentReferenceId() const;
    QString referenceType() const;
    bool allowNull() const;

signals:
    void referenceChanged(const ArtifactCore::LayerID& newId);
    void referenceCleared();
    void referencePicked();

private slots:
    void onPickButtonClicked();
    void onClearButtonClicked();

private:
    void updateDisplay();
    
    QLineEdit* nameEdit_;        // 参照 ID 表示
    QPushButton* pickButton_;    // ○ ピッカーボタン
    QPushButton* clearButton_;   // × クリアボタン
    QString referenceType_;      // 参照可能タイプ
    ArtifactCore::LayerID currentId_; // Nil ID (-1 相当)
    bool allowNull_ = true;
};

} // namespace Artifact
