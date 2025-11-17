module;
#include <QJsonObject>
module Composition._2D;

namespace Artifact {


 class ArtifactComposition2D::Impl
 {
 private:
  QVector<ArtifactAbstractLayerPtr> layers_;
 public:
  Impl();
  ~Impl();
  void resize(int width, int height);
  void setCompositionBackgroundColor(const FloatColor& color);

  void addLayer();
  void removeLayer();
  void removeAllLayer();
 };

 ArtifactComposition2D::Impl::Impl()
 {

 }

 ArtifactComposition2D::Impl::~Impl()
 {

 }

 void ArtifactComposition2D::Impl::resize(int width, int height)
 {

 }

 void ArtifactComposition2D::Impl::removeAllLayer()
 {

 }

ArtifactComposition2D::ArtifactComposition2D():impl_(new Impl)
{

}

ArtifactComposition2D::~ArtifactComposition2D()
{
 delete impl_;
}



void ArtifactComposition2D::addLayer()
{
 //auto layer = std::make_shared<ArtifactAbstractLayer>();
 //layer->id = QUuid::createUuid();
}

void ArtifactComposition2D::resize(int width, int height)
{
 impl_->resize(width, height);
}

void ArtifactComposition2D::removeLayer(ArtifactAbstractLayer* layer)
{

}

void ArtifactComposition2D::removeAllLayer()
{
 impl_->removeAllLayer();
}

ArtifactAbstractLayer* ArtifactComposition2D::frontMostLayer() const
{

 return nullptr;
}

ArtifactAbstractLayer* ArtifactComposition2D::backMostLayer() const
{

 return nullptr;
}

void ArtifactComposition2D::bringToFront(ArtifactAbstractLayer* layer)
{

}

void ArtifactComposition2D::sendToBack(ArtifactAbstractLayer* layer)
{

}

};
