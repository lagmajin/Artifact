module;
export module GlowEffect;

import Artifact.Effect.Abstract;

export namespace Artifact
{
 class GlowEffect:public ArtifactAbstractEffect{
  class Impl;
  Impl* impl_;
 public:
  GlowEffect();
  ~GlowEffect();
 };
	

};