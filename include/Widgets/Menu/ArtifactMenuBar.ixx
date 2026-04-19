module;
#include <utility>

#include <QtWidgets>
#include <wobjectdefs.h>
export module Menu.MenuBar;

export namespace Artifact {

enum class eMenuType {
 File,
 Edit,
 Create,
 Composition,
 Layer,
 Effect,
 Animation,
 Script,
 Render,
 Tool,
 Link,
 Test,
 Window,
 Option,
 Help,
};

class ArtifactMenuBar : public QMenuBar {
 W_OBJECT(ArtifactMenuBar)
private:
 class Impl;
 Impl* impl_;
public:
 explicit ArtifactMenuBar(QWidget* mainWindow, QWidget* parent = nullptr);
 ~ArtifactMenuBar();
 void setMainWindow(QWidget* window);
 void refreshFontFromSettings();
};

}
