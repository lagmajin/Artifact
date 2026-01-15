module;
#include <QObject>
#include <wobjectdefs.h>
#include <wobjectimpl.h>
export module Artifact.Service.Effect;



export namespace Artifact
{
 struct EffectServiceResult {
   
 };

 class ArtifactEffectService :public QObject
 {
   W_OBJECT(ArtifactEffectService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactEffectService(QObject*parent=nullptr);
  ~ArtifactEffectService();
 public/*signals*/:
   
 public/*slots*/:
   
 };


};