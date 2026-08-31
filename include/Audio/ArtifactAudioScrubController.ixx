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

    enum class AudioScrubChangeKind {
        Started,
        Stopped,
        LatencyUpdated,
        CacheMiss
    };

    struct AudioScrubChangedEvent {
        AudioScrubChangeKind kind = AudioScrubChangeKind::Started;
        FramePosition frame;
        int latencyMs = 0;
    };

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

        // Cross-widget notifications use AudioScrubChangedEvent via EventBus.
        // Qt metadata remains only for the private worker/thread boundary.

        ArtifactAudioScrubController(const ArtifactAudioScrubController&) = delete;
        ArtifactAudioScrubController& operator=(const ArtifactAudioScrubController&) = delete;

        ArtifactAudioScrubController(QObject* parent = nullptr);
        ~ArtifactAudioScrubController() override;

    private:
        class Impl;
        Impl* impl_;
    };
}
