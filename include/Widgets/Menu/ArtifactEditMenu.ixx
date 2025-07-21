module;
#include <wobjectdefs.h>
#include <QMenu>

export module Menu.Edit;


export namespace Artifact {

 class ArtifactEditMenu :public QMenu {
  W_OBJECT(ArtifactEditMenu)
 private:

 public:
  explicit ArtifactEditMenu();
  ~ArtifactEditMenu();

 };




};
