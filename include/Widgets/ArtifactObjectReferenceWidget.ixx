module;

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPointer>
#include <wobjectdefs.h>

export module Artifact.Widgets.ObjectReference;

export namespace Artifact {

class ArtifactObjectReferenceWidget : public QWidget {
    W_OBJECT(ArtifactObjectReferenceWidget)
public:
    explicit ArtifactObjectReferenceWidget(QWidget* parent = nullptr);
    ~ArtifactObjectReferenceWidget() override;
    
    // 設定
    void setReferenceType(const QString& typeName);
    void setCurrentReferenceId(qint64 id);
    void setAllowNull(bool allow);
    
    // 取得
    qint64 currentReferenceId() const;
    QString referenceType() const;
    bool allowNull() const;

signals:
    void referenceChanged(qint64 newId);
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
    qint64 currentId_ = -1;      // 現在の参照 ID（-1 は null）
    bool allowNull_ = true;
};

} // namespace Artifact
