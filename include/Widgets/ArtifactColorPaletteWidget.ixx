module;

#include <QWidget>
#include <QString>

export module Artifact.Widgets.ColorPaletteWidget;

import std;
import Artifact.Color.Palette;

export namespace Artifact {

class ArtifactColorPaletteWidget : public QWidget {
    Q_OBJECT
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactColorPaletteWidget(QWidget* parent = nullptr);
    ~ArtifactColorPaletteWidget() override;

    // 現在のマネージャー状態をUIに反映
    void updatePaletteList();

    // デフォルトのパレットマネージャーを外部からセットする
    void setPaletteManager(std::shared_ptr<ColorPaletteManager> manager);

public Q_SLOTS:
    void onGenerateHarmonicPalette();
    void onSmartExtractPalette();
    void onLoadPalettes();
    void onSavePalettes();

Q_SIGNALS:
    void paletteSelected(const ColorPalette& palette);
};

} // namespace Artifact
