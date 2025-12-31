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
  ~Impl();
  LayerType layerType() const;
  UniString layerName() const;
 };

 ArtifactLayerInitParams::Impl::Impl()
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

 ArtifactLayerInitParams::ArtifactLayerInitParams(const QString& name, LayerType type) :impl_(new Impl())
 {

 }

 ArtifactLayerInitParams::ArtifactLayerInitParams(const UniString& name, LayerType type):impl_(new Impl())
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

 ArtifactCameraLayerInitParams::ArtifactCameraLayerInitParams() :ArtifactLayerInitParams("name", LayerType::Camera)
 {

 }

 ArtifactCameraLayerInitParams::~ArtifactCameraLayerInitParams()
 {

 }

 ArtifactCompositionLayerInitParams::ArtifactCompositionLayerInitParams() :ArtifactLayerInitParams("name", LayerType::Null)
 {

 }

 ArtifactCompositionLayerInitParams::~ArtifactCompositionLayerInitParams()
 {

 }


};


