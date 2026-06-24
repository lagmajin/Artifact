module;
#include <utility>
#include <vector>
#include <QObject>
#include <wobjectdefs.h>
export module Artifact.Audio.ScrubController;

import Frame.Position;
import Artifact.Composition.Abstract;
import Core.Diagnostics.ProjectDiagnostic;

W_REGISTER_ARGTYPE(ArtifactCore::FramePosition)

export namespace Artifact
{
    using namespace ArtifactCore;

    class ArtifactAudioScrubController : public QObject
    {
        W_OBJECT(ArtifactAudioScrubController)

    public:
        static ArtifactAudioScrubController& instance();

        void setComposition(ArtifactCompositionPtr comp);

        void setEnabled(bool enabled);
        bool isEnabled() const;

        void setLatencyTargetMs(int ms);
        int latencyTargetMs() const;

        void setVolumeScale(float scale);
        float volumeScale() const;

        void startScrub();
        void stopScrub();
        void updateScrubPosition(FramePosition frame);

        bool isScrubbing() const;
        int latencyMs() const;
        float scrubSpeedFps() const;
        float scrubVolume() const;

        void loadSettings();
        void saveSettings();

        std::vector<ArtifactCore::ProjectDiagnostic> gatherDiagnostics() const;

        void scrubStarted() W_SIGNAL(scrubStarted);
        void scrubStopped() W_SIGNAL(scrubStopped);
        void latencyUpdated(int ms) W_SIGNAL(latencyUpdated, ms);
        void cacheMiss(FramePosition frame) W_SIGNAL(cacheMiss, frame);

        ArtifactAudioScrubController(const ArtifactAudioScrubController&) = delete;
        ArtifactAudioScrubController& operator=(const ArtifactAudioScrubController&) = delete;

        ArtifactAudioScrubController(QObject* parent = nullptr);
        ~ArtifactAudioScrubController() override;

    private:
        class Impl;
        Impl* impl_;
    };
}
