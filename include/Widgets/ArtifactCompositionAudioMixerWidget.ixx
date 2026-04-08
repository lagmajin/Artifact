module;
#include <utility>

#include <wobjectdefs.h>
#include <QWidget>
export module Artifact.Widgets.CompositionAudioMixer;


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
