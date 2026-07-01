module;
#include <utility>


module Artifact.Layer.InitParams;
import Utils.String.UniString;
import Artifact.Layer.Abstract;
import Color.Float;


namespace Artifact {

 class ArtifactLayerInitParams::Impl {
 private:
  LayerType	layerType_;
  UniString name_;
 public:
  Impl();
  Impl(LayerType type, const UniString& name);
  ~Impl();
  LayerType layerType() const;
  UniString layerName() const;
 };

 ArtifactLayerInitParams::Impl::Impl()
 {

 }

 ArtifactLayerInitParams::Impl::Impl(LayerType type, const UniString& name) : layerType_(type), name_(name)
 {

 }

 ArtifactLayerInitParams::Impl::~Impl()
 {

 }

 LayerType ArtifactLayerInitParams::Impl::layerType() const
 {
  return layerType_;
 }

UniString ArtifactLayerInitParams::Impl::layerName() const
 {
 return name_;
 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(const QString& name, LayerType type) :impl_(new Impl(type, UniString(name)))
 {
 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(const UniString& name, LayerType type):impl_(new Impl(type, name))
 {
 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(const ArtifactLayerInitParams& other) : impl_(new Impl(*other.impl_))
 {
 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(ArtifactLayerInitParams&& other) noexcept : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 ArtifactLayerInitParams& ArtifactLayerInitParams::operator=(const ArtifactLayerInitParams& other)
 {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 ArtifactLayerInitParams& ArtifactLayerInitParams::operator=(ArtifactLayerInitParams&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 ArtifactLayerInitParams::~ArtifactLayerInitParams()
 {
  delete impl_;
 }

UniString ArtifactLayerInitParams::name() const
 {

 return impl_->layerName();
}

void ArtifactLayerInitParams::setName(const UniString& name)
{
 if (impl_) {
  *impl_ = Impl(impl_->layerType(), name);
 }
}

LayerType ArtifactLayerInitParams::layerType() const
{

 return impl_->layerType();
}

 class ArtifactSolidLayerInitParams::Impl {
 public:
  int width_ = 1920;
  int height_ = 1080;
  FloatColor color_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  ArtifactSolidFillType fillType_ = ArtifactSolidFillType::Solid;
  FloatColor gradientStartColor_ = FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
  FloatColor gradientEndColor_ = FloatColor(0.2f, 0.2f, 0.2f, 1.0f);
  float gradientAngleDegrees_ = 90.0f;
  bool gradientReverse_ = false;
  float gradientCenterX_ = 0.5f;
  float gradientCenterY_ = 0.5f;
  float gradientScale_ = 1.0f;
  float gradientOffset_ = 0.0f;
 };

 ArtifactSolidLayerInitParams::ArtifactSolidLayerInitParams(const QString& name) :ArtifactLayerInitParams(name,LayerType::Solid), impl_(new Impl())
 {
 }

 ArtifactSolidLayerInitParams::ArtifactSolidLayerInitParams(const ArtifactSolidLayerInitParams& other) : ArtifactLayerInitParams(other), impl_(new Impl(*other.impl_))
 {
 }

 ArtifactSolidLayerInitParams::ArtifactSolidLayerInitParams(ArtifactSolidLayerInitParams&& other) noexcept : ArtifactLayerInitParams(std::move(other)), impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 ArtifactSolidLayerInitParams& ArtifactSolidLayerInitParams::operator=(const ArtifactSolidLayerInitParams& other)
 {
  if (this != &other) {
   ArtifactLayerInitParams::operator=(other);
   *impl_ = *other.impl_;
  }
  return *this;
 }

 ArtifactSolidLayerInitParams& ArtifactSolidLayerInitParams::operator=(ArtifactSolidLayerInitParams&& other) noexcept
 {
  if (this != &other) {
   ArtifactLayerInitParams::operator=(std::move(other));
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 ArtifactSolidLayerInitParams::~ArtifactSolidLayerInitParams()
 {
  delete impl_;
 }

 int ArtifactSolidLayerInitParams::width() const { return impl_->width_; }
 void ArtifactSolidLayerInitParams::setWidth(int width) { impl_->width_ = width; }
 int ArtifactSolidLayerInitParams::height() const { return impl_->height_; }
 void ArtifactSolidLayerInitParams::setHeight(int height) { impl_->height_ = height; }
 FloatColor ArtifactSolidLayerInitParams::color() const { return impl_->color_; }
 void ArtifactSolidLayerInitParams::setColor(const FloatColor& color) { impl_->color_ = color; }
 ArtifactSolidFillType ArtifactSolidLayerInitParams::fillType() const { return impl_->fillType_; }
 void ArtifactSolidLayerInitParams::setFillType(ArtifactSolidFillType fillType) { impl_->fillType_ = fillType; }
 FloatColor ArtifactSolidLayerInitParams::gradientStartColor() const { return impl_->gradientStartColor_; }
 void ArtifactSolidLayerInitParams::setGradientStartColor(const FloatColor& color) { impl_->gradientStartColor_ = color; }
 FloatColor ArtifactSolidLayerInitParams::gradientEndColor() const { return impl_->gradientEndColor_; }
 void ArtifactSolidLayerInitParams::setGradientEndColor(const FloatColor& color) { impl_->gradientEndColor_ = color; }
 float ArtifactSolidLayerInitParams::gradientAngleDegrees() const { return impl_->gradientAngleDegrees_; }
 void ArtifactSolidLayerInitParams::setGradientAngleDegrees(float degrees) { impl_->gradientAngleDegrees_ = degrees; }
 bool ArtifactSolidLayerInitParams::gradientReverse() const { return impl_->gradientReverse_; }
 void ArtifactSolidLayerInitParams::setGradientReverse(bool reverse) { impl_->gradientReverse_ = reverse; }
 float ArtifactSolidLayerInitParams::gradientCenterX() const { return impl_->gradientCenterX_; }
 void ArtifactSolidLayerInitParams::setGradientCenterX(float value) { impl_->gradientCenterX_ = value; }
 float ArtifactSolidLayerInitParams::gradientCenterY() const { return impl_->gradientCenterY_; }
 void ArtifactSolidLayerInitParams::setGradientCenterY(float value) { impl_->gradientCenterY_ = value; }
 float ArtifactSolidLayerInitParams::gradientScale() const { return impl_->gradientScale_; }
 void ArtifactSolidLayerInitParams::setGradientScale(float value) { impl_->gradientScale_ = value; }
 float ArtifactSolidLayerInitParams::gradientOffset() const { return impl_->gradientOffset_; }
 void ArtifactSolidLayerInitParams::setGradientOffset(float value) { impl_->gradientOffset_ = value; }

 ArtifactTextLayerInitParams::ArtifactTextLayerInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Text)
 {
 }

 class ArtifactNullLayerInitParams::Impl {
 public:
  int width_ = 100;
  int height_ = 100;
 };

 ArtifactNullLayerInitParams::ArtifactNullLayerInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Null), impl_(new Impl())
 {
 }

 ArtifactNullLayerInitParams::ArtifactNullLayerInitParams(const ArtifactNullLayerInitParams& other) : ArtifactLayerInitParams(other), impl_(new Impl(*other.impl_))
 {
 }

 ArtifactNullLayerInitParams::ArtifactNullLayerInitParams(ArtifactNullLayerInitParams&& other) noexcept : ArtifactLayerInitParams(std::move(other)), impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 ArtifactNullLayerInitParams& ArtifactNullLayerInitParams::operator=(const ArtifactNullLayerInitParams& other)
 {
  if (this != &other) {
   ArtifactLayerInitParams::operator=(other);
   *impl_ = *other.impl_;
  }
  return *this;
 }

 ArtifactNullLayerInitParams& ArtifactNullLayerInitParams::operator=(ArtifactNullLayerInitParams&& other) noexcept
 {
  if (this != &other) {
   ArtifactLayerInitParams::operator=(std::move(other));
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 ArtifactNullLayerInitParams::~ArtifactNullLayerInitParams()
 {
  delete impl_;
 }

 int ArtifactNullLayerInitParams::width() const { return impl_->width_; }
 void ArtifactNullLayerInitParams::setWidth(int width) { impl_->width_ = width; }
 int ArtifactNullLayerInitParams::height() const { return impl_->height_; }
 void ArtifactNullLayerInitParams::setHeight(int height) { impl_->height_ = height; }

 ArtifactImageInitParams::ArtifactImageInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Image)
 {

 }

 ArtifactImageInitParams::~ArtifactImageInitParams()
 {

 }

 ArtifactSvgInitParams::ArtifactSvgInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Shape)
 {

 }

 ArtifactSvgInitParams::~ArtifactSvgInitParams()
 {

 }

 ArtifactAudioInitParams::ArtifactAudioInitParams(const QString& name)
     : ArtifactLayerInitParams(name, LayerType::Audio)
 {
 }

 ArtifactAudioInitParams::~ArtifactAudioInitParams()
 {
 }

 ArtifactVideoInitParams::ArtifactVideoInitParams(const QString& name)
     : ArtifactLayerInitParams(name, LayerType::Video)
 {
 }

 ArtifactVideoInitParams::~ArtifactVideoInitParams()
 {
 }

 ArtifactCameraLayerInitParams::ArtifactCameraLayerInitParams() :ArtifactLayerInitParams(UniString("Camera 1"), LayerType::Camera)
 {

 }

 ArtifactCameraLayerInitParams::~ArtifactCameraLayerInitParams()
 {

 }

 ArtifactCompositionLayerInitParams::ArtifactCompositionLayerInitParams() :ArtifactLayerInitParams(UniString("name"), LayerType::Precomp)
 {

 }

 ArtifactCompositionLayerInitParams::~ArtifactCompositionLayerInitParams()
 {

 }

 ArtifactCompositionBackgroundLayerInitParams::ArtifactCompositionBackgroundLayerInitParams()
     :ArtifactLayerInitParams(UniString("Composition Background"), LayerType::CompositionBackground)
 {

 }

 ArtifactCompositionBackgroundLayerInitParams::~ArtifactCompositionBackgroundLayerInitParams()
 {

 }
 ArtifactParametricCompositionLayerInitParams::ArtifactParametricCompositionLayerInitParams()
     :ArtifactLayerInitParams(UniString("Parametric Composition"), LayerType::ParametricComposition)
 {

 }

 ArtifactParametricCompositionLayerInitParams::~ArtifactParametricCompositionLayerInitParams()
 {

 }

ArtifactModel3DLayerInitParams::ArtifactModel3DLayerInitParams(const QString& name)
     : ArtifactLayerInitParams(name, LayerType::Model3D)
 {
 }

 ArtifactModel3DLayerInitParams::~ArtifactModel3DLayerInitParams()
 {
 }

 ArtifactFixedGeometry3DLayerInitParams::ArtifactFixedGeometry3DLayerInitParams(const QString& name, FixedGeometry3D geometry)
     : ArtifactLayerInitParams(name, LayerType::Model3D), geometry_(geometry)
 {
 }

 ArtifactFixedGeometry3DLayerInitParams::~ArtifactFixedGeometry3DLayerInitParams()
 {
 }

FixedGeometry3D ArtifactFixedGeometry3DLayerInitParams::geometry() const
{
 return geometry_;
}

void ArtifactFixedGeometry3DLayerInitParams::setGeometry(FixedGeometry3D geometry)
{
 geometry_ = geometry;
}


};





























