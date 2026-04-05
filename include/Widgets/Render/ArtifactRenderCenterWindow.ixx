module;
#include <QMainWindow>
#include <wobjectdefs.h>

export module Artifact.Widgets.RenderCenterWindow;

export namespace Artifact {

class ArtifactRenderCenterWindow : public QMainWindow {
 W_OBJECT(ArtifactRenderCenterWindow)

private:
 class Impl;
 Impl* impl_;

public:
 explicit ArtifactRenderCenterWindow(QWidget* parent = nullptr);
 ~ArtifactRenderCenterWindow();

 void present();
};

}
