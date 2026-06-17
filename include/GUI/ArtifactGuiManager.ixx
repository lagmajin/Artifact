module;

#include <QtCore/QObject>

export module Artifact.Gui.Manager;

export namespace Artifact {

class ArtifactGuiManager : public QObject {
public:
  ArtifactGuiManager();
  ~ArtifactGuiManager();
};

} // namespace Artifact
