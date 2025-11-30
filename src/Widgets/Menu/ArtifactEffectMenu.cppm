module;
#include <QAction>
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
  QAction* inspectorAction_ = nullptr;
 };

 ArtifactEffectMenu::Impl::Impl(QMenu*menu)
{
  inspectorAction_ = new QAction("Inspector");
  //inspectorAction_->setText()

 }

 ArtifactEffectMenu::Impl::~Impl()
 {

 }

 ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl(this))
 {
  setTitle(tr("Effect"));

  addAction(impl_->inspectorAction_);
  addSeparator();
 }

 ArtifactEffectMenu::~ArtifactEffectMenu()
 {
  delete impl_;
 }



};