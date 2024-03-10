#pragma once
#include <QtCore/QtCore>
#include <QtWidgets/QtWidgets>

namespace Artifact {

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
  Help
 };

 class ArtifactHelpMenu {
 private:

 public:
  explicit ArtifactHelpMenu(QWidget* parent = nullptr);
  ~ArtifactHelpMenu();
 };

 class ArtifactMenuBarPrivate;

 class ArtifactMenuBar :public QMenuBar{
 private:
  
 public:
  explicit ArtifactMenuBar(QWidget*parent=nullptr);
  ~ArtifactMenuBar();
 };


};