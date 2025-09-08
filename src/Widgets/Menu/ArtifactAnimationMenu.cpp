module;
#include <QMenu>

module Menu.Animation;

namespace Artifact {

 class ArtifactAnimationMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
  QAction* keyframeSupportAction = nullptr;

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
  setTearOffEnabled(true);
 }


 ArtifactAnimationMenu::~ArtifactAnimationMenu()
 {
  delete impl_;
 }


};