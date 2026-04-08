module;
#include <utility>
#include <QWidget>

#include <wobjectdefs.h>
export module Artifact.Widgets.ColorSciencePanel;

import Color.ScienceManager;

export namespace Artifact {

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
