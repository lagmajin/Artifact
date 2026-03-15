module;
#include <QMenu>
#include <wobjectdefs.h>

export module Menu.Render;

export namespace Artifact {

 class ArtifactRenderMenu : public QMenu {
  W_OBJECT(ArtifactRenderMenu)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactRenderMenu(QWidget* mainWindow, QWidget* parent = nullptr);
  ~ArtifactRenderMenu();

  void rebuildMenu();
 };

}
