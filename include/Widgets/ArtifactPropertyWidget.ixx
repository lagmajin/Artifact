module;

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>

export module Artifact.Widgets.ArtifactPropertyWidget;

import std;
import Artifact.Layer.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPropertyWidget : public QScrollArea {
    Q_OBJECT
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactPropertyWidget(QWidget* parent = nullptr);
    ~ArtifactPropertyWidget();

    void setLayer(ArtifactAbstractLayerPtr layer);
    void clear();

    void updateProperties();
};

} // namespace Artifact
