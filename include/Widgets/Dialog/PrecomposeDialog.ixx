module;
#include <utility>

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <wobjectdefs.h>
export module Artifact.Widgets.PrecomposeDialog;

export namespace Artifact {

/// プリコンポーズ設定ダイアログ
/// docs/image/precomposeDialog.jpeg に基づく UI
class PrecomposeDialog final : public QDialog {
    W_OBJECT(PrecomposeDialog)

public:
    explicit PrecomposeDialog(QWidget* parent = nullptr);
    ~PrecomposeDialog();

    // Input: populate before exec()
    void setSelectedLayerNames(const QStringList& names);
    void setTotalLayerCount(int total);

    // Output: read after exec() == Accepted
    QString newCompositionName() const;
    bool    moveSelectedOnly() const;   // true = move selected, false = move all attributes
    bool    openNewComposition() const;
    bool    addAsAdjustmentLayer() const;
    bool    matchWorkspaceDuration() const;

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
