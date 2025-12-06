module;
#include <wobjectdefs.h>
#include <QObject>
export module Artifact.Service.ActiveContext;


export namespace Artifact
{
 class ArtifactActiveContextService :public QObject
 {
  W_OBJECT(ArtifactActiveContextService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactActiveContextService(QObject* parent = nullptr);
  ~ArtifactActiveContextService();
  void setHandler(QObject* obj);
 };


};