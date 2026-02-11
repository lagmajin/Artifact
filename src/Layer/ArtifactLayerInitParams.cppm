module;


module Artifact.Layer.InitParams;
import Utils.String.UniString;
import Artifact.Layer.Abstract;


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

 ArtifactLayerInitParams::~ArtifactLayerInitParams()
 {
  delete impl_;
 }

UniString ArtifactLayerInitParams::name() const
 {

 return impl_->layerName();
 }

LayerType ArtifactLayerInitParams::layerType() const
{

 return LayerType::Unknown;
}

 ArtifactSolidLayerInitParams::ArtifactSolidLayerInitParams(const QString& name) :ArtifactLayerInitParams(name,LayerType::Solid)
 {

 }

 ArtifactSolidLayerInitParams::~ArtifactSolidLayerInitParams()
 {
  
 }

 ArtifactTextLayerInitParams::ArtifactTextLayerInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Text)
 {
 }

 ArtifactNullLayerInitParams::ArtifactNullLayerInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Null)
 {

 }

 ArtifactNullLayerInitParams::~ArtifactNullLayerInitParams()
 {

 }

 ArtifactImageInitParams::ArtifactImageInitParams(const QString& name) :ArtifactLayerInitParams(name, LayerType::Image)
 {

 }

 ArtifactImageInitParams::~ArtifactImageInitParams()
 {

 }

 ArtifactCameraLayerInitParams::ArtifactCameraLayerInitParams() :ArtifactLayerInitParams(UniString("name"), LayerType::Camera)
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





























