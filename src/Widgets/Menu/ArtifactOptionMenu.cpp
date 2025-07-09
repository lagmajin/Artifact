module;
#include <QMenu>
#include <wobjectimpl.h>

module Menu.Option;



namespace Artifact {

 W_OBJECT_IMPL(ArtifactOptionMenu)

  class ArtifactOptionMenu::Impl {
  private:

  public:

 };

 ArtifactOptionMenu::ArtifactOptionMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {

 }

 ArtifactOptionMenu::~ArtifactOptionMenu()
 {

 }

};

