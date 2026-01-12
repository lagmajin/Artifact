module;
#include <QList>
#include <QString>
module Artifact.Service.Effect;

import Utils.String.UniString;
import Artifact.Effects.Manager;


namespace Artifact
{
 using namespace ArtifactCore;

struct EffectFactory {
 UniString id;
 UniString name;
};


 class ArtifactEffectService::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactEffectService::Impl::Impl()
 {

 }

 ArtifactEffectService::Impl::~Impl()
 {

 }

 ArtifactEffectService::ArtifactEffectService()
 {

 }

 ArtifactEffectService::~ArtifactEffectService()
 {

 }

};