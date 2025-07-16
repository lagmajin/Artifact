module;
#include <QWidget>
#include <QMenu>

module Menu:View;

//#include "../../../include/Widgets/Menu/ArtifactViewMenu.hpp"





namespace Artifact {

 class ArtifactViewMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactViewMenu::Impl::Impl()
 {

 }

 ArtifactViewMenu::Impl::~Impl()
 {

 }

 ArtifactViewMenu::ArtifactViewMenu(QWidget* parent/*=nullptr*/):QMenu(parent)
 {
  setObjectName("ViewMenu");

  setTitle("View");
 }

 ArtifactViewMenu::~ArtifactViewMenu()
 {

 }

 void ArtifactViewMenu::registerView(const QString& name, QWidget* view)
 {

 }

};