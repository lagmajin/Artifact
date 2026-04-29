module;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QImage>
export module Artifact.Layer.Text;




import Artifact.Layers.Abstract._2D;
import Utils.String.UniString;
import Color.Float;
import Image.ImageF32x4_RGBA;
import Text.Style;
import Artifact.Layers;

export namespace Artifact {

enum class TextLayoutMode : int {
    Point = 0,
    Box = 1
};

class ArtifactTextLayer : public ArtifactAbstract2DLayer {
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

    void setTextColor(const ArtifactCore::FloatColor& color);
    ArtifactCore::FloatColor textColor() const;

    void setStrokeEnabled(bool enabled);
    bool isStrokeEnabled() const;
    void setStrokeColor(const ArtifactCore::FloatColor& color);
    ArtifactCore::FloatColor strokeColor() const;
    void setStrokeWidth(float width);
    float strokeWidth() const;

    void setShadowEnabled(bool enabled);
    bool isShadowEnabled() const;
    void setShadowColor(const ArtifactCore::FloatColor& color);
    ArtifactCore::FloatColor shadowColor() const;
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

    void setUnderline(bool enabled);
    bool isUnderline() const;

    void setStrikethrough(bool enabled);
    bool isStrikethrough() const;

    void setHorizontalAlignment(ArtifactCore::TextHorizontalAlignment alignment);
    ArtifactCore::TextHorizontalAlignment horizontalAlignment() const;

    void setVerticalAlignment(ArtifactCore::TextVerticalAlignment alignment);
    ArtifactCore::TextVerticalAlignment verticalAlignment() const;

    void setWrapMode(ArtifactCore::TextWrapMode wrapMode);
    ArtifactCore::TextWrapMode wrapMode() const;

    void setLayoutMode(TextLayoutMode mode);
    TextLayoutMode layoutMode() const;
    bool isBoxText() const;

    void setMaxWidth(float width);
    float maxWidth() const;

    void setBoxHeight(float height);
    float boxHeight() const;

     void setParagraphSpacing(float spacing);
     float paragraphSpacing() const;

     void addAnimator();
     void removeAnimator(int index);
     void setAnimatorCount(int count);
     int animatorCount() const;

     QImage toQImage() const;
     const ArtifactCore::ImageF32x4_RGBA& currentFrameBuffer() const;
     bool hasCurrentFrameBuffer() const;

     // Trigger update of internal image
     void updateImage();
     QJsonObject toJson() const override;
     void fromJsonProperties(const QJsonObject& obj) override;
     std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
     bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

    void draw(ArtifactIRenderer* renderer) override;
};

}
