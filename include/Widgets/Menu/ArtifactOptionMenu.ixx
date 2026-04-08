module;
#include <utility>
#include <wobjectdefs.h>
#include <QMenu>
export module Menu.Option;





export namespace Artifact {

 class ArtifactOptionMenuPrivate;

 class ArtifactOptionMenu :public QMenu {
  W_OBJECT(ArtifactOptionMenu)
 private:
  class Impl;
  Impl* impl_=nullptr;
 public:
  explicit ArtifactOptionMenu(QWidget *parent=nullptr);
  ~ArtifactOptionMenu();
  /*signals:*/

 };

}
