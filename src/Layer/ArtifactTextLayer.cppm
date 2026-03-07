module;
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QString>
#include <QRect>
#include <QSize>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

module Artifact.Layer.Text;

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
import Image.ImageF32x4_RGBA;
import Size;

namespace Artifact
{
using namespace ArtifactCore;

class ArtifactTextLayer::Impl
{
public:
    UniString text_;
    float fontSize_;
    UniString fontFamily_;
    FloatRGBA color_;

    Impl();
};

ArtifactTextLayer::Impl::Impl()
    : text_(QString("New Text Layer"))
    , fontSize_(60.0f)
    , fontFamily_(QString("Arial"))
    , color_(1.0f, 1.0f, 1.0f, 1.0f)
{
}

ArtifactTextLayer::ArtifactTextLayer() 
    : ArtifactAbstractLayer()
    , impl_(new Impl())
{
    updateImage();
}

ArtifactTextLayer::~ArtifactTextLayer()
{
    delete impl_;
}

void ArtifactTextLayer::setText(const UniString& text)
{
    impl_->text_ = text;
    updateImage();
}

UniString ArtifactTextLayer::text() const
{
    return impl_->text_;
}

void ArtifactTextLayer::setFontSize(float size)
{
    if (size <= 0.0f) size = 1.0f;
    impl_->fontSize_ = size;
    updateImage();
}

float ArtifactTextLayer::fontSize() const
{
    return impl_->fontSize_;
}

void ArtifactTextLayer::setFontFamily(const UniString& family)
{
    impl_->fontFamily_ = family;
    updateImage();
}

UniString ArtifactTextLayer::fontFamily() const
{
    return impl_->fontFamily_;
}

void ArtifactTextLayer::setTextColor(const FloatRGBA& color)
{
    impl_->color_ = color;
    updateImage();
}

FloatRGBA ArtifactTextLayer::textColor() const
{
    return impl_->color_;
}

void ArtifactTextLayer::draw(ArtifactIRenderer* renderer)
{
    // Text layer drawing implementation
    // This would typically render the text to the current render target
    // For now, we can call updateImage() to ensure the image is up to date
    updateImage();
}

// Provide a default implementation for updateImage to satisfy linkage.
void ArtifactTextLayer::updateImage() {
    // Render text to an internal QImage or no-op placeholder
    // Keep minimal to avoid heavy dependencies here
    QImage img(1,1,QImage::Format_RGBA8888);
    img.fill(Qt::transparent);
    (void)img;
}

} // namespace Artifact
