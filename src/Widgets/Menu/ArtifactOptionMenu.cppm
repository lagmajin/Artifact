module;
#include <QMenu>
#include <wobjectimpl.h>

module Menu.Option;



namespace Artifact {

 W_OBJECT_IMPL(ArtifactOptionMenu)

  class ArtifactOptionMenu::Impl {
  private:

  public:
   Impl();
   ~Impl();
 };

 ArtifactOptionMenu::Impl::Impl()
 {

 }

 ArtifactOptionMenu::Impl::~Impl()
 {

 }

 ArtifactOptionMenu::ArtifactOptionMenu(QWidget* parent/*=nullptr*/):QMenu(parent),impl_(new Impl())
 {
  setTitle("Options");
  setTearOffEnabled(true);
  setSeparatorsCollapsible(true);
  setMinimumWidth(160);
 }

 ArtifactOptionMenu::~ArtifactOptionMenu()
 {
  delete impl_;
 }

};

