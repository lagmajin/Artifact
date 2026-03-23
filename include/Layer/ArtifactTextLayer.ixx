module;
#include <QImage>
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
export module Artifact.Layer.Text;




import Artifact.Layer.Abstract;
import Utils.String.UniString;
import FloatRGBA;
import Text.Style;
import Artifact.Layers;

export namespace Artifact {

class ArtifactTextLayer : public ArtifactAbstractLayer {
private:
    class Impl;
    Impl* impl_;
    void markDirty();
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

    void setStrokeEnabled(bool enabled);
    bool isStrokeEnabled() const;
    void setStrokeColor(const FloatRGBA& color);
    FloatRGBA strokeColor() const;
    void setStrokeWidth(float width);
    float strokeWidth() const;

    void setShadowEnabled(bool enabled);
    bool isShadowEnabled() const;
    void setShadowColor(const FloatRGBA& color);
    FloatRGBA shadowColor() const;
    void setShadowOffset(float x, float y);
    float shadowOffsetX() const;
    float shadowOffsetY() const;
    void setShadowBlur(float blur);
    float shadowBlur() const;

    void setTracking(float tracking);    float tracking() const;

    void setLeading(float leading);
    float leading() const;

    void setBold(bool enabled);
    bool isBold() const;

    void setItalic(bool enabled);
    bool isItalic() const;

    void setAllCaps(bool enabled);
    bool isAllCaps() const;

    void setHorizontalAlignment(ArtifactCore::TextHorizontalAlignment alignment);
    ArtifactCore::TextHorizontalAlignment horizontalAlignment() const;
    QImage toQImage() const;

    // Trigger update of internal image
    void updateImage();
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

    void draw(ArtifactIRenderer* renderer) override;
};

}
