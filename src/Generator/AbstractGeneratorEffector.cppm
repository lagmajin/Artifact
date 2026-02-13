module;

#include <QString>
#include <QColor>
#include <QHash>
#include <QDebug>
#include <cmath>

module Generator.Effector;

import std;
import Utils.String.UniString;
import Image.ImageF32x4RGBAWithCache;

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

    // TODO: cv::Mat を使ってピクセル塗りつぶし
    qDebug() << "[SolidGenerator] Generated solid color:" << solidColor_.name();
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
    // TODO: cv::Mat を使ってグラデーションを描画
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
    // TODO: Perlin/Simplex ノイズライブラリを使用
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
    // TODO: cv::rectangle, cv::circle などで描画
    const char* shapeNames[] = {"Rectangle", "Circle", "Triangle", "Polygon"};
    qDebug() << "[ShapeGenerator] Generated shape:" << shapeNames[static_cast<int>(shapeType_)]
             << "Color:" << shapeColor_.name();
  }

};
