module;
#include <QtWidgets/QMenu>
export module Menu.Test;







export namespace Artifact {

 class ArtifactRenderTestMenuPrivate;

 export class ArtifactRenderTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactRenderTestMenu(QWidget* parent = nullptr);
  ~ArtifactRenderTestMenu();
 signals:
  void serialImageRenderTestRequested();
 public slots:
 };

 class ArtifactEffectTestMenuPrivate;

 export class ArtifactEffectTestMenu {
  private:

 public:

 };

 class ArtifactMediaTestMenuPrivate;

 export class ArtifactMediaTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactMediaTestMenu(QWidget* parent = nullptr);
  ~ArtifactMediaTestMenu();
 };


 

 class ArtifactWidgetTestMenu :public QMenu {

 };

 

 class ArtifactTestMenu :public QMenu {
 private:

 public:
  explicit ArtifactTestMenu(QWidget* parent = nullptr);
  ~ArtifactTestMenu();
 signals:
  
  
 };










};