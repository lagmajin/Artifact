module;
#include <utility>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QtWidgets>
#include <Qtwidgets/QMenu>
#include <wobjectdefs.h>
export module Menu.Composition;


export namespace Artifact {

class ArtifactCompositionMenuPrivate;

class ArtifactCompositionMenu : public QMenu {
 W_OBJECT(ArtifactCompositionMenu)
private:
 class Impl;
 Impl* impl_;
public:
 void rebuildMenu();
 W_SLOT(rebuildMenu, ());

public:
 explicit ArtifactCompositionMenu(QWidget* mainWindow, QWidget* parent);
 ArtifactCompositionMenu(QWidget* parent = nullptr);
 ~ArtifactCompositionMenu();
 void handleCreateCompositionRequested();
 W_SLOT(handleCreateCompositionRequested, ());
};

}
