module;


#include <QtCore/QScopedPointer>
#include <QtWidgets/QtWidgets>
#include <Qtwidgets/QMenu>
#include <wobjectdefs.h>


export module Menu.Composition;





export namespace Artifact {

 class ArtifactCompositionMenuPrivate;

 class ArtifactCompositionMenu:public QMenu {
  W_OBJECT(ArtifactCompositionMenu)
 private:
  class Impl;
  Impl* impl_;
  //QScopedPointer<ArtifactCompositionMenuPrivate> pImpl_;
  
 public:
  explicit ArtifactCompositionMenu(QWidget*parent=nullptr);
  ~ArtifactCompositionMenu();
  void handleCreateCompositionRequested();
  W_SLOT(handleCreateCompositionRequested,() );
 };









};