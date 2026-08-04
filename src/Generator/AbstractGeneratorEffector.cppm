module;

#include <QString>
#include <QColor>
#include <QHash>
#include <QDebug>
#include <cmath>
#include <cstdint>

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
#include <opencv2/opencv.hpp>
module Generator.Effector;




import Utils.String.UniString;
import Core.Parallel;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Artifact.Layer.Image;

namespace Artifact
{
  using namespace ArtifactCore;

  // ==================== AbstractGeneratorEffector ====================

  class AbstractGeneratorEffector::Impl
  {
  public:
    Impl();
    ~Impl();

    UniString name_;
    QHash<QString, float> parameters_;
    QHash<QString, QColor> colorParameters_;

    int outputWidth_ = 1920;
    int outputHeight_ = 1080;
    int startFrame_ = 0;
    int endFrame_ = 100;
    int currentFrame_ = 0;
    bool enabled_ = true;
    ImageF32x4RGBAWithCache output_;
  };

  AbstractGeneratorEffector::Impl::Impl()
  {
  }

  AbstractGeneratorEffector::Impl::~Impl()
  {
  }

  AbstractGeneratorEffector::AbstractGeneratorEffector()
    : impl_(new Impl())
  {
    impl_->name_ = UniString("Generator Effect");
  }

  AbstractGeneratorEffector::~AbstractGeneratorEffector()
  {
    delete impl_;
  }

  void AbstractGeneratorEffector::apply()
  {
    if (!impl_->enabled_) {
        return;
    }
    if (impl_->currentFrame_ < impl_->startFrame_ ||
        impl_->currentFrame_ > impl_->endFrame_) {
        return;
    }
    generateContent(impl_->output_, impl_->currentFrame_,
                    impl_->outputWidth_, impl_->outputHeight_);
  }

  const ImageF32x4RGBAWithCache& AbstractGeneratorEffector::output() const
  {
    return impl_->output_;
  }

  void AbstractGeneratorEffector::applyToLayer(ArtifactCore::SharedPtr<ArtifactAbstractLayer> layer)
  {
    if (!layer || !impl_->enabled_) {
      return;
    }
    apply();
    if (impl_->output_.image().isEmpty()) {
      qWarning() << "[AbstractGeneratorEffector] Generated output is empty";
      return;
    }
    auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer);
    if (!imageLayer) {
      qWarning() << "[AbstractGeneratorEffector] Target layer is not an image layer";
      return;
    }
    imageLayer->setFromImageBuffer(impl_->output_.image());
    qDebug() << "[AbstractGeneratorEffector] Applied generated image to layer"
             << impl_->output_.width() << "x" << impl_->output_.height();
  }

  void AbstractGeneratorEffector::setName(const UniString& name)
  {
    impl_->name_ = name;
  }

  UniString AbstractGeneratorEffector::name() const
  {
    return impl_->name_;
  }

  void AbstractGeneratorEffector::setParameter(const UniString& paramName, float value)
  {
    impl_->parameters_[paramName.toQString()] = value;
  }

  float AbstractGeneratorEffector::getParameter(const UniString& paramName) const
  {
    return impl_->parameters_.value(paramName.toQString(), 0.0f);
  }

  void AbstractGeneratorEffector::setParameterColor(const UniString& paramName, const QColor& color)
  {
    impl_->colorParameters_[paramName.toQString()] = color;
  }

  QColor AbstractGeneratorEffector::getParameterColor(const UniString& paramName) const
  {
    return impl_->colorParameters_.value(paramName.toQString(), QColor(255, 255, 255, 255));
  }

  void AbstractGeneratorEffector::setOutputSize(int width, int height)
  {
    const int normalizedWidth = std::max(1, width);
    const int normalizedHeight = std::max(1, height);
    if (impl_->outputWidth_ == normalizedWidth &&
        impl_->outputHeight_ == normalizedHeight) {
      return;
    }
    impl_->outputWidth_ = normalizedWidth;
    impl_->outputHeight_ = normalizedHeight;
    impl_->output_ = ImageF32x4RGBAWithCache();
  }

  int AbstractGeneratorEffector::outputWidth() const
  {
    return impl_->outputWidth_;
  }

  int AbstractGeneratorEffector::outputHeight() const
  {
    return impl_->outputHeight_;
  }

  void AbstractGeneratorEffector::setFrameRange(int startFrame, int endFrame)
  {
    impl_->startFrame_ = std::min(startFrame, endFrame);
    impl_->endFrame_ = std::max(startFrame, endFrame);
  }

  int AbstractGeneratorEffector::startFrame() const
  {
    return impl_->startFrame_;
  }

  int AbstractGeneratorEffector::endFrame() const
  {
    return impl_->endFrame_;
  }

  void AbstractGeneratorEffector::setCurrentFrame(int frameNumber)
  {
    impl_->currentFrame_ = frameNumber;
  }

  int AbstractGeneratorEffector::currentFrame() const
  {
    return impl_->currentFrame_;
  }

  void AbstractGeneratorEffector::setEnabled(bool enabled)
  {
    impl_->enabled_ = enabled;
  }

  bool AbstractGeneratorEffector::isEnabled() const
  {
    return impl_->enabled_;
  }

  // ==================== SolidGeneratorEffector ====================

  SolidGeneratorEffector::SolidGeneratorEffector()
    : AbstractGeneratorEffector(), solidColor_(Qt::white)
  {
    setName(UniString("Solid Generator"));
  }

  SolidGeneratorEffector::~SolidGeneratorEffector()
  {
  }

  void SolidGeneratorEffector::setSolidColor(const QColor& color)
  {
    solidColor_ = color;
  }

  QColor SolidGeneratorEffector::solidColor() const
  {
    return solidColor_;
  }

  void SolidGeneratorEffector::generateContent(ImageF32x4RGBAWithCache& dst, 
                                                int frameNumber, 
                                                int width, 
                                                int height)
  {
    dst.image().resize(width, height);
    dst.image().fill(FloatRGBA(solidColor_.redF(),
                               solidColor_.greenF(),
                               solidColor_.blueF(),
                               solidColor_.alphaF()));
    dst.UpdateGpuTextureFromCpuData();

    qDebug() << "[SolidGenerator] Generated solid color:" << solidColor_.name() << width << "x" << height;
  }

  // ==================== GradientGeneratorEffector ====================

  GradientGeneratorEffector::GradientGeneratorEffector()
    : AbstractGeneratorEffector(), gradientType_(Linear), 
      startColor_(Qt::black), endColor_(Qt::white)
  {
    setName(UniString("Gradient Generator"));
  }

  GradientGeneratorEffector::~GradientGeneratorEffector()
  {
  }

  void GradientGeneratorEffector::setGradientType(GradientType type)
  {
    const int value = std::clamp(static_cast<int>(type),
                                 static_cast<int>(Linear),
                                 static_cast<int>(Conic));
    gradientType_ = static_cast<GradientType>(value);
  }

  GradientGeneratorEffector::GradientType GradientGeneratorEffector::gradientType() const
  {
    return gradientType_;
  }

  void GradientGeneratorEffector::setStartColor(const QColor& color)
  {
    startColor_ = color;
  }

  void GradientGeneratorEffector::setEndColor(const QColor& color)
  {
    endColor_ = color;
  }

  QColor GradientGeneratorEffector::startColor() const
  {
    return startColor_;
  }

  QColor GradientGeneratorEffector::endColor() const
  {
    return endColor_;
  }

  void GradientGeneratorEffector::generateContent(ImageF32x4RGBAWithCache& dst, 
                                                  int frameNumber, 
                                                  int width, 
                                                  int height)
  {
    if (width <= 0 || height <= 0) {
      return;
    }
    cv::Mat mat(height, width, CV_32FC4);
    
    cv::Vec4f cStart(startColor_.redF(), startColor_.greenF(), startColor_.blueF(), startColor_.alphaF());
    cv::Vec4f cEnd(endColor_.redF(), endColor_.greenF(), endColor_.blueF(), endColor_.alphaF());

    if (gradientType_ == Linear) {
Parallel::For(0, height, width * height, [&](int y) {
            float t = static_cast<float>(y) / std::max(1, height - 1);
            cv::Vec4f color = cStart * (1.0f - t) + cEnd * t;
            mat.row(y).setTo(color);
        });
    } else if (gradientType_ == Radial) {
        // Radial (簡易実装: 中心から円形に)
        float cx = width / 2.0f;
        float cy = height / 2.0f;
        float maxDist = std::max(1.0f, std::sqrt(cx*cx + cy*cy));
Parallel::For(0, height, width * height, [&](int y) {
            cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                float dist = std::sqrt((x-cx)*(x-cx) + (y-cy)*(y-cy));
                float t = std::min(1.0f, dist / maxDist);
                row[x] = cStart * (1.0f - t) + cEnd * t;
            }
        });
    } else {
        // Conic: angle around the center, clockwise from the positive X axis.
        const float cx = width / 2.0f;
        const float cy = height / 2.0f;
        constexpr float twoPi = 6.28318530717958647692f;
        constexpr float pi = 3.14159265358979323846f;
        Parallel::For(0, height, width * height, [&](int y) {
            cv::Vec4f* row = mat.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                float angle = std::atan2(static_cast<float>(y) - cy,
                                         static_cast<float>(x) - cx);
                float t = (angle + pi) / twoPi;
                t = std::clamp(t, 0.0f, 1.0f);
                row[x] = cStart * (1.0f - t) + cEnd * t;
            }
        });
    }

    dst.image().setFromRGBA32F(mat.ptr<float>(), width, height);
    dst.UpdateGpuTextureFromCpuData();

    qDebug() << "[GradientGenerator] Generated gradient:" 
             << startColor_.name() << "to" << endColor_.name();
  }

  // ==================== NoiseGeneratorEffector ====================

  NoiseGeneratorEffector::NoiseGeneratorEffector()
    : AbstractGeneratorEffector(), noiseType_(Perlin), 
      scale_(1.0f), amplitude_(1.0f), octaves_(1)
  {
    setName(UniString("Noise Generator"));
  }

  NoiseGeneratorEffector::~NoiseGeneratorEffector()
  {
  }

  void NoiseGeneratorEffector::setNoiseType(NoiseType type)
  {
    const int value = std::clamp(static_cast<int>(type),
                                 static_cast<int>(Perlin),
                                 static_cast<int>(WhiteNoise));
    noiseType_ = static_cast<NoiseType>(value);
  }

  NoiseGeneratorEffector::NoiseType NoiseGeneratorEffector::noiseType() const
  {
    return noiseType_;
  }

  void NoiseGeneratorEffector::setScale(float scale)
  {
    scale_ = std::isfinite(scale) ? std::max(0.001f, std::abs(scale)) : 0.001f;
  }

  float NoiseGeneratorEffector::scale() const
  {
    return scale_;
  }

  void NoiseGeneratorEffector::setAmplitude(float amplitude)
  {
    amplitude_ = std::isfinite(amplitude) ? amplitude : 0.0f;
  }

  float NoiseGeneratorEffector::amplitude() const
  {
    return amplitude_;
  }

  void NoiseGeneratorEffector::setOctaves(int octaves)
  {
    octaves_ = std::clamp(octaves, 1, 12);
  }

  int NoiseGeneratorEffector::octaves() const
  {
    return octaves_;
  }

  void NoiseGeneratorEffector::generateContent(ImageF32x4RGBAWithCache& dst, 
                                               int frameNumber, 
                                               int width, 
                                               int height)
  {
    if (width <= 0 || height <= 0) {
      return;
    }

    // Build deterministic multi-octave value noise.  The previous implementation
    // ignored scale/octaves and produced a different image on every call.
    cv::Mat accumulated = cv::Mat::zeros(height, width, CV_32FC1);
    const int octaveCount = std::clamp(octaves_, 1, 12);
    const float baseScale = std::max(0.001f, std::abs(scale_));
    float normalization = 0.0f;
    for (int octave = 0; octave < octaveCount; ++octave) {
      const float frequency = baseScale * std::pow(2.0f, static_cast<float>(octave));
      const int maxGridWidth = std::max(2, std::min(width, 512));
      const int maxGridHeight = std::max(2, std::min(height, 512));
      const int gridWidth = std::clamp(
          static_cast<int>(std::ceil(width * frequency)), 2, maxGridWidth);
      const int gridHeight = std::clamp(
          static_cast<int>(std::ceil(height * frequency)), 2, maxGridHeight);
      const bool whiteNoise = noiseType_ == WhiteNoise;
      cv::Mat coarse(whiteNoise ? height : gridHeight,
                     whiteNoise ? width : gridWidth, CV_32FC1);
      cv::RNG rng(static_cast<std::uint64_t>(frameNumber + 1) * 1009u +
                  static_cast<std::uint64_t>(octave + 1) * 9176u);
      rng.fill(coarse, cv::RNG::UNIFORM, 0.0f, 1.0f);
      cv::Mat octaveNoise;
      if (whiteNoise) {
        octaveNoise = coarse;
      } else {
        cv::resize(coarse, octaveNoise, cv::Size(width, height), 0.0, 0.0,
                   cv::INTER_LINEAR);
      }
      const float weight = 1.0f / std::pow(2.0f, static_cast<float>(octave));
      accumulated += octaveNoise * weight;
      normalization += weight;
    }
    accumulated *= amplitude_ / std::max(normalization, 1.0e-6f);

    cv::Mat mat(height, width, CV_32FC4);
    for (int y = 0; y < height; ++y) {
      const float* sourceRow = accumulated.ptr<float>(y);
      cv::Vec4f* destinationRow = mat.ptr<cv::Vec4f>(y);
      for (int x = 0; x < width; ++x) {
        const float value = sourceRow[x];
        destinationRow[x] = cv::Vec4f(value, value, value, 1.0f);
      }
    }

    dst.image().setFromRGBA32F(mat.ptr<float>(), width, height);
    dst.UpdateGpuTextureFromCpuData();

    qDebug() << "[NoiseGenerator] Generated noise - Type:" << static_cast<int>(noiseType_)
             << "Scale:" << scale_ << "Octaves:" << octaves_;
  }

  // ==================== ShapeGeneratorEffector ====================

  ShapeGeneratorEffector::ShapeGeneratorEffector()
    : AbstractGeneratorEffector(), shapeType_(Rectangle),
      shapeColor_(Qt::white), backgroundColor_(Qt::black), shapeSize_(0.5f)
  {
    setName(UniString("Shape Generator"));
  }

  ShapeGeneratorEffector::~ShapeGeneratorEffector()
  {
  }

  void ShapeGeneratorEffector::setShapeType(ShapeType type)
  {
    const int value = std::clamp(static_cast<int>(type),
                                 static_cast<int>(Rectangle),
                                 static_cast<int>(Polygon));
    shapeType_ = static_cast<ShapeType>(value);
  }

  ShapeGeneratorEffector::ShapeType ShapeGeneratorEffector::shapeType() const
  {
    return shapeType_;
  }

  void ShapeGeneratorEffector::setShapeColor(const QColor& color)
  {
    shapeColor_ = color;
  }

  void ShapeGeneratorEffector::setBackgroundColor(const QColor& color)
  {
    backgroundColor_ = color;
  }

  QColor ShapeGeneratorEffector::shapeColor() const
  {
    return shapeColor_;
  }

  QColor ShapeGeneratorEffector::backgroundColor() const
  {
    return backgroundColor_;
  }

  void ShapeGeneratorEffector::setShapeSize(float size)
  {
    shapeSize_ = std::isfinite(size) ? std::clamp(size, 0.0f, 1.0f) : 0.5f;
  }

  float ShapeGeneratorEffector::shapeSize() const
  {
    return shapeSize_;
  }

  void ShapeGeneratorEffector::generateContent(ImageF32x4RGBAWithCache& dst, 
                                               int frameNumber, 
                                               int width, 
                                               int height)
  {
    if (width <= 0 || height <= 0) {
      return;
    }
    cv::Scalar bg(backgroundColor_.redF(), backgroundColor_.greenF(), backgroundColor_.blueF(), backgroundColor_.alphaF());
    cv::Scalar fg(shapeColor_.redF(), shapeColor_.greenF(), shapeColor_.blueF(), shapeColor_.alphaF());
    
    cv::Mat mat(height, width, CV_32FC4, bg);

    int cx = width / 2;
    int cy = height / 2;
    int size = std::max(0, static_cast<int>(
        std::min(width, height) * std::max(0.0f, shapeSize_)));

    switch (shapeType_) {
        case Rectangle:
            cv::rectangle(mat, 
                          cv::Point(cx - size/2, cy - size/2), 
                          cv::Point(cx + size/2, cy + size/2), 
                          fg, cv::FILLED);
            break;
        case Circle:
            cv::circle(mat, cv::Point(cx, cy), size/2, fg, cv::FILLED);
            break;
        case Triangle: {
            std::vector<cv::Point> pts = {
                cv::Point(cx, cy - size/2),
                cv::Point(cx - size/2, cy + size/2),
                cv::Point(cx + size/2, cy + size/2)
            };
            cv::fillPoly(mat, std::vector<std::vector<cv::Point>>{pts}, fg);
            break;
        }
        case Polygon: {
            // 仮実装: 5角形
            std::vector<cv::Point> pts;
            for (int i = 0; i < 5; ++i) {
                float angle = i * 2.0f * CV_PI / 5.0f - CV_PI / 2.0f;
                pts.push_back(cv::Point(cx + std::cos(angle) * size/2, cy + std::sin(angle) * size/2));
            }
            cv::fillPoly(mat, std::vector<std::vector<cv::Point>>{pts}, fg);
            break;
        }
    }

    dst.image().setFromRGBA32F(mat.ptr<float>(), width, height);
    dst.UpdateGpuTextureFromCpuData();

    const char* shapeNames[] = {"Rectangle", "Circle", "Triangle", "Polygon"};
    qDebug() << "[ShapeGenerator] Generated shape:" << shapeNames[static_cast<int>(shapeType_)]
             << "Color:" << shapeColor_.name();
  }

};
