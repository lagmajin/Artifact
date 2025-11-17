module;
#include <QCoreApplication>
#include <QDir>
#include <wobjectimpl.h>
module Menu.Test2;





namespace Artifact {

 W_OBJECT_IMPL(ArtifactImageProcessingTestMenu)
 //using namespace ArtifactCore;

 class ArtifactImageProcessingTestMenuPrivate {
 private:
  //CompositionBuffer2D buffer_;
 public:
  ArtifactImageProcessingTestMenuPrivate();
  ~ArtifactImageProcessingTestMenuPrivate();

  void test();
 };

 ArtifactImageProcessingTestMenuPrivate::ArtifactImageProcessingTestMenuPrivate()
 {

 }

 ArtifactImageProcessingTestMenuPrivate::~ArtifactImageProcessingTestMenuPrivate()
 {

 }

 void ArtifactImageProcessingTestMenuPrivate::test()
 {
  QString executableDir = QCoreApplication::applicationDirPath();

  QDir imageDir(executableDir);
  QString imageFolderPath = imageDir.filePath("Test");



 }

 ArtifactImageProcessingTestMenu::ArtifactImageProcessingTestMenu(QWidget* parent /*= nullptr*/) :QMenu(parent), pImpl_(new ArtifactImageProcessingTestMenuPrivate())
 {
  setTitle("ImageProcessing");
  auto test1 = addAction("Test1");

  connect(test1, SIGNAL(triggered), this, SLOT(imageProcessingTest));
 }

 ArtifactImageProcessingTestMenu::~ArtifactImageProcessingTestMenu()
 {

 }

 void ArtifactImageProcessingTestMenu::imageProcessingTest()
 {

 }
};