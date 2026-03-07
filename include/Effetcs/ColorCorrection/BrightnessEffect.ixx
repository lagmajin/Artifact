module;

#include <QString>
#include <QVariant>

export module BrightnessEffect;

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



import Artifact.Effect.Abstract;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

// 明度 (Brightness) エフェクト
// 画像全体の明るさを調整する基本的なカラーコレクション
class BrightnessEffect : public ArtifactAbstractEffect {
private:
    class Impl;
    Impl* impl_;

protected:
    void apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override;

public:
    BrightnessEffect();
    ~BrightnessEffect() override;

    // 明度 (-1.0 ~ 1.0, default: 0.0)
    void setBrightness(float brightness);
    float brightness() const;

    // コントラスト (-1.0 ~ 1.0, default: 0.0)
    void setContrast(float contrast);
    float contrast() const;

    // ハイライト調整 (-1.0 ~ 1.0, default: 0.0)
    void setHighlights(float highlights);
    float highlights() const;

    // シャドウ調整 (-1.0 ~ 1.0, default: 0.0)
    void setShadows(float shadows);
    float shadows() const;

    // プロパティ取得
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
};

} // namespace Artifact