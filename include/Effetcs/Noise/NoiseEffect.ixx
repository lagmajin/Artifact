module;
export module NoiseEffect;

import Artifact.Effect.Abstract;

export namespace Artifact
{
  class NoiseEffect:public ArtifactAbstractEffect{
   class Impl;
   Impl* impl_;
 public:
   NoiseEffect();
   ~NoiseEffect();
 };
	
	
};