module;
#include <QMenu>
#include <qcoro6/qcoro/qcorotask.h>
module Menu.Animation;

namespace Artifact {

 class ArtifactAnimationMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
  QAction* keyframeSupportAction = nullptr;
  QAction* removeKeyframeAction = nullptr;

  QAction* easyEaseAction = nullptr;
  QAction* linearAction = nullptr;
  QAction* holdAction = nullptr;
  QAction* toggleVelocityGraphAction = nullptr;
  QAction* selectAllKeyframesAction = nullptr;
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