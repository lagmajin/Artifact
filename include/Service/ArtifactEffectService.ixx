module;
#include <QObject>
export module Artifact.Service.Effect;


export namespace Artifact
{

 class ArtifactEffectService :public QObject
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactEffectService();
  ~ArtifactEffectService();
 };


};