module;

#include <mutex>



module Menu:File;




namespace Artifact {

 class  ArtifactFileMenuPrivate {
 private:

  bool projectCreated_ = false;
 public:

  void projectCreated();
  void projectClosed();
 };

 void ArtifactFileMenuPrivate::projectCreated()
 {
  projectCreated_ = true;
 }

 void ArtifactFileMenuPrivate::projectClosed()
 {
  projectCreated_ = false;
 }

 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/):pImpl_(new ArtifactFileMenuPrivate)
 {
  setObjectName("FileMenu");

  setTitle("File");

  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);
 }

 ArtifactFileMenu::~ArtifactFileMenu()
 {

 }

 void ArtifactFileMenu::projectCreated()
 {

 }

 

 void ArtifactFileMenu::projectClosed()
 {

 }

};