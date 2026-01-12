module;

module Artifact.Effect.GauusianBlur;

import std;
import Artifact.Effect.ImplBase;

namespace Artifact
{
 class GauusianBlurCPUMode : public ArtifactEffectImplBase
 {
 public:
  void apply();
 };
 class GauusianBlurGPUMode :public ArtifactEffectImplBase
 {

 };

 class GauusianBlur::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 GauusianBlur::Impl::Impl()
 {

 }

 GauusianBlur::Impl::~Impl()
 {

 }

 GauusianBlur::GauusianBlur():impl_(new Impl())
 {

 }

 GauusianBlur::~GauusianBlur()
 {
  delete impl_;
 }

};