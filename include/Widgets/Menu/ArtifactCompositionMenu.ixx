module;


#include <QtCore/QScopedPointer>
#include <QtWidgets/QtWidgets>
#include <Qtwidgets/QMenu>
#include <wobjectdefs.h>


export module Menu:ArtifactCompositionMenu;





export namespace Artifact {

 class ArtifactCompositionMenuPrivate;

 class ArtifactCompositionMenu:public QMenu {
  W_OBJECT(ArtifactCompositionMenu)
 private:
  QScopedPointer<ArtifactCompositionMenuPrivate> pImpl_;
  
 public:
  explicit ArtifactCompositionMenu(QWidget*parent=nullptr);
  ~ArtifactCompositionMenu();
 };









};