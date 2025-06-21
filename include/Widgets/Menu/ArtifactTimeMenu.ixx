module;
#include <QMenu>
#include <wobjectdefs.h>
export module Menu:Time;


export namespace Artifact {

 class ArtifactTimeMenu :public QMenu{
 private:

 public:
  explicit ArtifactTimeMenu(QWidget* parent = nullptr);
  ~ArtifactTimeMenu();
 };









};