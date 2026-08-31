module;
#include <utility>
#include <memory>

#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.ColorSwatchWidget;


import Color.Float;
import Color.Swatch;

W_REGISTER_ARGTYPE(ArtifactCore::FloatColor)

export namespace Artifact {

using ArtifactCore::FloatColor;

/**
 * @brief カラースウォッチ（カラーパレット）をグリッド表示・選択するためのウィジェット
 */
class ArtifactColorSwatchWidget : public QWidget {
    W_OBJECT(ArtifactColorSwatchWidget)

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit ArtifactColorSwatchWidget(QWidget* parent = nullptr);
    ~ArtifactColorSwatchWidget() override;

    // パレットデータの設定/取得
    void setSwatch(const ArtifactCore::ColorSwatch& swatch);
    const ArtifactCore::ColorSwatch& getSwatch() const;

    // UI操作
    void updateListView();

public Q_SLOTS:
    void onLoadGPL();
    void onSaveGPL();
    void onClear();
    void onColorDoubleClicked(const QModelIndex& index);
};

} // namespace Artifact
