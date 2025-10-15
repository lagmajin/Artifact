module;
#include <QJsonObject>
module Composition._2D;

namespace Artifact {


 class ArtifactComposition2D::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactComposition2D::Impl::Impl()
 {

 }

 ArtifactComposition2D::Impl::~Impl()
 {

 }

ArtifactComposition2D::ArtifactComposition2D()
{

}

ArtifactComposition2D::~ArtifactComposition2D()
{

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
