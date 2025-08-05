module;
#include <QMenu>

export module Artifact.Menu.Effect;


export namespace Artifact {

 class ArtifactEffectMenu :public QMenu{
 private:

 public:
  ArtifactEffectMenu(QWidget* parent = nullptr);
  ~ArtifactEffectMenu();
 };

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {

 }

 ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/)
 {

 }

}