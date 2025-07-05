module;
#include <QtCore/QtCore>
#include <QtWidgets/QtWidgets>


export module Menu.MenuBar;






export namespace Artifact {

 enum class eMenuType {
  File,
  Edit,
  Create,
  Composition,
  Layer,
  Effect,
  Animation,
  Script,
  Render,
  Tool,
  Link,
  Test,
  Window,
  Option,
  Help,

 };



 class ArtifactMenuBarPrivate;

 class ArtifactMenuBar :public QMenuBar{
 private:
  
 public:
  explicit ArtifactMenuBar(QWidget*parent=nullptr);
  ~ArtifactMenuBar();
 };


};