module;
#include <cstdint>
#include <utility>

#include <QDialog>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.CreateNoiseLayerDialog;

import ImageProcessing.ProceduralTexture;

export namespace Artifact {

using namespace ArtifactCore;

/// ノイズレイヤー作成ダイアログ
/// 名前 / ノイズ種別 / シード / サイズを指定して ArtifactNoiseLayer を作成する
class CreateNoiseLayerDialog final : public QDialog {
    W_OBJECT(CreateNoiseLayerDialog)

public:
    explicit CreateNoiseLayerDialog(QWidget* parent = nullptr);
    ~CreateNoiseLayerDialog();

    // Composition size を初期幅/高さのデフォルトとして設定する
    void setCompositionSize(int width, int height);

    // Returned settings
    QString layerName() const;
    ArtifactCore::ProceduralTextureGeneratorKind kind() const;
    std::uint32_t seed() const;
    int width() const;
    int height() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
