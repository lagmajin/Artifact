module;

#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.CompositionAudioMixer;

import std;

export namespace Artifact
{
class ArtifactCompositionAudioMixerWidget : public QWidget
{
    W_OBJECT(ArtifactCompositionAudioMixerWidget)

private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactCompositionAudioMixerWidget(QWidget* parent = nullptr);
    ~ArtifactCompositionAudioMixerWidget();

public:
    void refreshFromCurrentComposition(); W_SLOT(refreshFromCurrentComposition);
};
}
