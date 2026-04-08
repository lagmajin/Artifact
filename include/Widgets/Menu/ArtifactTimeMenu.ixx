module;
#include <utility>
#include <wobjectdefs.h>
#include <QMenu>
export module Menu.Time;



export namespace Artifact {

 class ArtifactTimeMenu :public QMenu{
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimeMenu(QWidget* parent = nullptr);
  ~ArtifactTimeMenu();
 };









};
