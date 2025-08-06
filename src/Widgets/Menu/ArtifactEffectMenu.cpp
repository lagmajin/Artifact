module;
#include <QWidget>
module Artifact.Menu.Effect;


namespace Artifact
{
 class ArtifactEffectMenu::Impl
 {
 private:

 public:
  Impl(QMenu*menu);
  ~Impl();
 };

 ArtifactEffectMenu::Impl::Impl(QMenu*menu)
{

 }

 ArtifactEffectMenu::Impl::~Impl()
 {

 }

 ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/):QMenu(parent)
 {
  setTitle(tr("Effect"));


 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {

 }



};