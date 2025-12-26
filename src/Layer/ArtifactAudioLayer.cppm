module;

module Artifact.Layer.Audio;





namespace Artifact
{

 class ArtifactAudioLayer::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };
	
 ArtifactAudioLayer::Impl::Impl()
 {
 }

 ArtifactAudioLayer::Impl::~Impl()
 {
 }
	
 ArtifactAudioLayer::ArtifactAudioLayer():impl_(new Impl())
 {

 }

 ArtifactAudioLayer::~ArtifactAudioLayer()
 {
  delete impl_;
 }

 void ArtifactAudioLayer::mute()
 {

 }

}