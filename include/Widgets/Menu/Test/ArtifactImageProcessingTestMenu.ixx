module;


#include <wobjectdefs.h>
#include <QtWidgets/QMenu>
export module Menu:Test2;



export namespace Artifact {

 class ArtifactImageProcessingTestMenuPrivate;

 class ArtifactImageProcessingTestMenu :public QMenu {
  W_OBJECT(ArtifactImageProcessingTestMenu)
 private:
  QScopedPointer<ArtifactImageProcessingTestMenuPrivate> pImpl_;
 public:
  explicit ArtifactImageProcessingTestMenu(QWidget* parent = nullptr);
  ~ArtifactImageProcessingTestMenu();

 public slots:
  void imageProcessingTest();
 };





};