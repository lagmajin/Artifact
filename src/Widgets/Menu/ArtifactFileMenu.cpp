module;

#include <mutex>

#include <QMenu>

#include <wobjectimpl.h>
module Menu:File;




namespace Artifact {

 W_OBJECT_IMPL(ArtifactFileMenu)

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

 ArtifactFileMenu::ArtifactFileMenu(QWidget* parent /*= nullptr*/):QMenu(parent),pImpl_(new ArtifactFileMenuPrivate)
 {
  setObjectName("FileMenu");

  setTitle("File");

  QPalette p = palette();
  p.setColor(QPalette::Window, QColor(30, 30, 30));

  setPalette(p);

  setAutoFillBackground(true);


  setAttribute(Qt::WA_TranslucentBackground);
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
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