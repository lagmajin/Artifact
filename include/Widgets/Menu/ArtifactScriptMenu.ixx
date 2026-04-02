module;
#include <QMenu>
#include <wobjectdefs.h>

export module Artifact.Menu.Script;

export namespace Artifact {

class ArtifactScriptMenu : public QMenu {
 W_OBJECT(ArtifactScriptMenu)
private:
 class Impl;
 Impl* impl_;

public:
 explicit ArtifactScriptMenu(QWidget* parent = nullptr);
 ~ArtifactScriptMenu();
};

}
