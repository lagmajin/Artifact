
module;
#include <QMenu>
#include <QWidget>
export module Menu.Help;



namespace Artifact {

 class ArtifactHelpMenu :public QMenu{
 private:

 public:
  explicit ArtifactHelpMenu(QWidget* parent = nullptr);
  ~ArtifactHelpMenu();
 };









};