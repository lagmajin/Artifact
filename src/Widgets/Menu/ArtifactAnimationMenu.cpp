module;
#include <QMenu>

module Menu.Animation;

namespace Artifact {

 class ArtifactAnimationMenu::Impl {
 public:
  Impl();
  ~Impl();
 };

 ArtifactAnimationMenu::Impl::Impl()
 {

 }

 ArtifactAnimationMenu::Impl::~Impl()
 {

 }

 ArtifactAnimationMenu::ArtifactAnimationMenu(QWidget* parent/*=nullptr*/) :QMenu(parent), impl_(new Impl ())
 {
  setTitle(u8"Animation");

 }


 ArtifactAnimationMenu::~ArtifactAnimationMenu()
 {
  delete impl_;
 }


};