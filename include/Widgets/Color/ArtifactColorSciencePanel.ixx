module;

#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.ColorSciencePanel;

export namespace Artifact {

class ArtifactColorScienceManager;

class ArtifactColorSciencePanel : public QWidget {
  W_OBJECT(ArtifactColorSciencePanel)

private:
  class Impl;
  Impl *impl_;

public:
  explicit ArtifactColorSciencePanel(QWidget *parent = nullptr);
  ~ArtifactColorSciencePanel();

  ArtifactColorScienceManager *colorScienceManager() const;
};

} // namespace Artifact