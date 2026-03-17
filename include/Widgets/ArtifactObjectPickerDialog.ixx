module;

#include <QDialog>
#include <QTreeWidget>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <wobjectdefs.h>

export module Artifact.Widgets.ObjectPicker;

export namespace Artifact {

class ArtifactObjectPickerDialog : public QDialog {
    W_OBJECT(ArtifactObjectPickerDialog)
public:
    explicit ArtifactObjectPickerDialog(QWidget* parent = nullptr);
    ~ArtifactObjectPickerDialog() override;
    
    // 設定
    void setReferenceType(const QString& typeName);
    void setCurrentSelectionId(qint64 id);
    
    // 結果取得
    qint64 selectedId() const;

private slots:
    void onObjectDoubleClicked(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);
    void onOkClicked();
    void onCancelClicked();

private:
    void buildObjectTree();
    void filterObjectTree(const QString& filter);
    void addCompositionTree(QTreeWidgetItem* parent);
    void addLayerTree(QTreeWidgetItem* parent);
    
    QTreeWidget* objectTree_;      // オブジェクトツリー
    QLineEdit* searchEdit_;        // 検索フィルター
    QDialogButtonBox* buttonBox_;  // OK/Cancel
    QString referenceType_;
    qint64 currentSelectionId_ = -1;
};

} // namespace Artifact
