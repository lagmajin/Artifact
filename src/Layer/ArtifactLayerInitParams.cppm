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

 ArtifactCameraLayerInitParams::ArtifactCameraLayerInitParams() :ArtifactLayerInitParams(UniString("Camera 1"), LayerType::Camera)
 {

 }

 ArtifactCameraLayerInitParams::~ArtifactCameraLayerInitParams()
 {

 }

 ArtifactCompositionLayerInitParams::ArtifactCompositionLayerInitParams() :ArtifactLayerInitParams(UniString("name"), LayerType::Null)
 {

 }

 ArtifactCompositionLayerInitParams::~ArtifactCompositionLayerInitParams()
 {

 }


};





























