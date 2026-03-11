module;
#include <QMenu>
#include <wobjectdefs.h>

export module Artifact.Menu.Effect;


export namespace Artifact {

class ArtifactEffectMenu :public QMenu{
  W_OBJECT(ArtifactEffectMenu)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  ArtifactEffectMenu(QWidget* parent = nullptr);
  ~ArtifactEffectMenu();
 };



}
