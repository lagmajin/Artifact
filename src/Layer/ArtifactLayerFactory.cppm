module;


module Artifact.Layers.Factory;


namespace Artifact {

 class LayerFactory::Impl {
 private:
 public:
  Impl();
  ~Impl();
  ArtifactAbstractLayerPtr createNewLayer(LayerType type) noexcept;
 };

 LayerFactory::Impl::Impl()
 {

 }

 LayerFactory::Impl::~Impl()
 {

 }

 ArtifactAbstractLayerPtr LayerFactory::Impl::createNewLayer(LayerType type) noexcept
 {

  return nullptr;
 }

 LayerFactory::LayerFactory() :impl_(new Impl())
 {

 }

 LayerFactory::~LayerFactory()
 {
  delete impl_;
 }





}


