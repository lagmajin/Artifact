module;
#include <utility>

#include <QMainWindow>
#include <wobjectdefs.h>
export module Artifact.Widgets.ReactiveEventEditorWindow;

export namespace Artifact {

class ArtifactReactiveEventEditorWindow : public QMainWindow {
  W_OBJECT(ArtifactReactiveEventEditorWindow)

private:
  class Impl;
  Impl* impl_;

public:
  explicit ArtifactReactiveEventEditorWindow(QWidget* parent = nullptr);
  ~ArtifactReactiveEventEditorWindow();

  void present();
};

}
