
module;
#include <QMenu>
#include <QWidget>
#include <wobjectdefs.h>
export module Menu.Help;


namespace Artifact {

 class ArtifactHelpMenu :public QMenu{
  W_OBJECT(ArtifactHelpMenu)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactHelpMenu(QWidget* parent = nullptr);
  ~ArtifactHelpMenu();
 };









};