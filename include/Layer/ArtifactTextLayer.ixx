module;
export module Artifact.Layer.Text;

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



import Artifact.Layer.Abstract;
import Utils.String.UniString;
import FloatRGBA;
import Artifact.Layers;

export namespace Artifact {

class ArtifactTextLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;
public:
    ArtifactTextLayer();
    ~ArtifactTextLayer();

    void setText(const UniString& text);
    UniString text() const;

    void setFontSize(float size);
    float fontSize() const;

    void setFontFamily(const UniString& family);
    UniString fontFamily() const;

    void setTextColor(const FloatRGBA& color);
    FloatRGBA textColor() const;

    // Trigger update of internal image
    void updateImage();

    void draw(ArtifactIRenderer* renderer) override;
};

}
