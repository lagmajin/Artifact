module;

#include <QString>
#include <QColor>
#include <QHash>
#include <QDebug>
#include <cmath>
#include <opencv2/opencv.hpp>

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
module Generator.Effector;




import Utils.String.UniString;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;

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
    qDebug() << "[AbstractGeneratorEffector] apply() called";
    // TODO: 実装
  }

  void AbstractGeneratorEffector::applyToLayer(std::shared_ptr<ArtifactAbstractLayer> layer)
  {
    if (!layer || !impl_->enabled_) {
      return;
    }
    // TODO: レイヤーに適用
    qDebug() << "[AbstractGeneratorEffector] Applied to layer";
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
    impl_->outputWidth_ = width;
    impl_->outputHeight_ = height;
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
    impl_->startFrame_ = startFrame;
    impl_->endFrame_ = endFrame;
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
    float r = solidColor_.redF();
    float g = solidColor_.greenF();
    float b = solidColor_.blueF();
    float a = solidColor_.alphaF();

    cv::Mat mat(height, width, CV_32FC4, cv::Scalar(r, g, b, a));
    dst.image().setFromCVMat(mat);
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
    gradientType_ = type;
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
    cv::Mat mat(height, width, CV_32FC4);
    
    cv::Vec4f cStart(startColor_.redF(), startColor_.greenF(), startColor_.blueF(), startColor_.alphaF());
    cv::Vec4f cEnd(endColor_.redF(), endColor_.greenF(), endColor_.blueF(), endColor_.alphaF());

    if (gradientType_ == Linear) {
        for (int y = 0; y < height; ++y) {
            float t = static_cast<float>(y) / std::max(1, height - 1);
            cv::Vec4f color = cStart * (1.0f - t) + cEnd * t;
            mat.row(y).setTo(color);
        }
    } else {
        // Radial (簡易実装: 中心から円形に)
        float cx = width / 2.0f;
        float cy = height / 2.0f;
        float maxDist = std::sqrt(cx*cx + cy*cy);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float dist = std::sqrt((x-cx)*(x-cx) + (y-cy)*(y-cy));
                float t = std::min(1.0f, dist / maxDist);
                mat.at<cv::Vec4f>(y, x) = cStart * (1.0f - t) + cEnd * t;
            }
        }
    }

    dst.image().setFromCVMat(mat);
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
    noiseType_ = type;
  }

  NoiseGeneratorEffector::NoiseType NoiseGeneratorEffector::noiseType() const
  {
    return noiseType_;
  }

  void NoiseGeneratorEffector::setScale(float scale)
  {
    scale_ = scale;
  }

  float NoiseGeneratorEffector::scale() const
  {
    return scale_;
  }

  void NoiseGeneratorEffector::setAmplitude(float amplitude)
  {
    amplitude_ = amplitude;
  }

  float NoiseGeneratorEffector::amplitude() const
  {
    return amplitude_;
  }

  void NoiseGeneratorEffector::setOctaves(int octaves)
  {
    octaves_ = octaves;
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
    cv::Mat mat(height, width, CV_32FC4);
    
    // 簡易的なノイズ生成（本来はPerlin/Simplexノイズライブラリを使用する）
    cv::Mat noise(height, width, CV_32FC4);
    cv::randu(noise, cv::Scalar::all(0.0f), cv::Scalar::all(amplitude_));
    
    // アルファチャンネルは1.0に固定
    int from_to[] = { 0,0, 1,1, 2,2 };
    cv::mixChannels(&noise, 1, &mat, 1, from_to, 3);
    cv::Mat alpha(height, width, CV_32FC1, cv::Scalar(1.0f));
    int alpha_from_to[] = { 0, 3 };
    cv::mixChannels(&alpha, 1, &mat, 1, alpha_from_to, 1);

    dst.image().setFromCVMat(mat);
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
    shapeType_ = type;
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
    shapeSize_ = size;
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
    cv::Scalar bg(backgroundColor_.redF(), backgroundColor_.greenF(), backgroundColor_.blueF(), backgroundColor_.alphaF());
    cv::Scalar fg(shapeColor_.redF(), shapeColor_.greenF(), shapeColor_.blueF(), shapeColor_.alphaF());
    
    cv::Mat mat(height, width, CV_32FC4, bg);

    int cx = width / 2;
    int cy = height / 2;
    int size = static_cast<int>(std::min(width, height) * shapeSize_);

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

    dst.image().setFromCVMat(mat);
    dst.UpdateGpuTextureFromCpuData();

    const char* shapeNames[] = {"Rectangle", "Circle", "Triangle", "Polygon"};
    qDebug() << "[ShapeGenerator] Generated shape:" << shapeNames[static_cast<int>(shapeType_)]
             << "Color:" << shapeColor_.name();
  }

};
