module;
#include <QList>
#include <QString>
#include <wobjectimpl.h>
module Artifact.Service.Effect;

import Utils.String.UniString;
import Artifact.Effects.Manager;


namespace Artifact
{
 using namespace ArtifactCore;


W_OBJECT_IMPL(ArtifactEffectService)


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


 ArtifactEffectService::ArtifactEffectService(QObject* parent/*=nullptr*/):QObject(parent),impl_(new Impl)
 {

 }

 ArtifactEffectService::~ArtifactEffectService()
 {
  delete impl_;
 }

};