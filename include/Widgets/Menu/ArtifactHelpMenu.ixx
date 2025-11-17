
module;
#include <QMenu>
#include <QWidget>
export module Menu.Help;



namespace Artifact {

 class ArtifactHelpMenu :public QMenu{
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactHelpMenu(QWidget* parent = nullptr);
  ~ArtifactHelpMenu();
 };









};