module;


module Artifact.Layers.Factory;


namespace Artifact {

 class LayerFactory::Impl {
 private:
 public:
  Impl();
  ~Impl();
 };

 LayerFactory::Impl::Impl()
 {

 }

 LayerFactory::Impl::~Impl()
 {

 }

 LayerFactory::LayerFactory() :impl_(new Impl())
 {

 }

 LayerFactory::~LayerFactory()
 {
  delete impl_;
 }





}


