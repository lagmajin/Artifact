module;
#include <QMenu>
#include <wobjectdefs.h>
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