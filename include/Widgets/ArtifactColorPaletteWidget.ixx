module;

#include <QWidget>
#include <QString>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Artifact.Widgets.ColorPaletteWidget;




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
