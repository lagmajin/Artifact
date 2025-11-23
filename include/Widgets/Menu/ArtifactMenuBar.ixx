module;
#include <QtWidgets>

export module Menu.MenuBar;


import std;

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

 class ArtifactMainWindow;


 class ArtifactMenuBar :public QMenuBar{
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactMenuBar(ArtifactMainWindow* mainWindow,QWidget*parent=nullptr);
  ~ArtifactMenuBar();
  void setMainWindow(ArtifactMainWindow* window);
 };


};