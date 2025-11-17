module;
#include <QMenu>

export module Artifact.Menu.Effect;


export namespace Artifact {

 class ArtifactEffectMenu :public QMenu{
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  ArtifactEffectMenu(QWidget* parent = nullptr);
  ~ArtifactEffectMenu();
 };



}