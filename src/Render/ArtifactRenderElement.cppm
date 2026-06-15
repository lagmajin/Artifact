module;


module Artifact.Render.Element;


namespace Artifact
{

 class ArtifactImageElement::Impl
 {
 public:
  Impl();
  ~Impl()=default;
 };

 ArtifactImageElement::Impl::Impl()
 {

 }


 ArtifactImageElement::ArtifactImageElement():impl_(new Impl())
 {

 }

 ArtifactImageElement::~ArtifactImageElement()
 {
  delete impl_;
 }

};