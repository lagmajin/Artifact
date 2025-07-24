module;

#include <QMenu>
export module Menu.Animation;



export namespace Artifact {

 class ArtifactAnimationMenu :public QMenu{
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactAnimationMenu(QWidget*parent=nullptr);
  ~ArtifactAnimationMenu();
 };






};
