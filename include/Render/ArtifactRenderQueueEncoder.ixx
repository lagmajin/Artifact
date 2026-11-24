module;

#include <QImage>
#include <QString>

#include <memory>

export module Artifact.Render.Queue.Encoder;

import Artifact.Render.Queue.Job;

export namespace Artifact {

class IVideoEncodeBackend {
public:
    virtual ~IVideoEncodeBackend() = default;
    virtual bool open(const ArtifactRenderJob& job, QString* errorMessage) = 0;
    virtual bool addFrame(const QImage& frame, int frameIndex, QString* errorMessage) = 0;
    virtual bool close(QString* errorMessage) = 0;
    // True when addFrame() may be called from a dedicated encode thread.
    // Backends that own thread-affine Qt objects (for example QProcess) must
    // keep the default false so the caller stays on the consumer thread.
    virtual bool encodeOnWorkerThread() const { return false; }
};

QString normalizeVideoEncodeBackendName(const QString& backend);
QString normalizeRenderBackend(const QString& backend);
QString deriveContainerFromJob(const ArtifactRenderJob& job);
QString defaultOutputPathForJob(const ArtifactRenderJob& job);
bool isImageSequenceContainer(const QString& format);
QString sequenceExtension(const QString& format, const QString& codec);
bool isVideoContainer(const QString& format);
bool isAudioContainer(const QString& format);
QString normalizeCodecName(const QString& codec);

// Alpha contract helpers. The canonical names are stable enough to be used
// in job JSON, service APIs and UI item data.
QString alphaStateName(ArtifactRenderJob::AlphaState state);
ArtifactRenderJob::AlphaState alphaStateFromName(const QString& name, bool* ok = nullptr);
bool alphaStateCarriesAlpha(ArtifactRenderJob::AlphaState state);
ArtifactRenderJob::AlphaState effectiveAlphaStateForJob(const ArtifactRenderJob& job);
// Single source of truth for the encoder pixel format. Video backends and the
// PNG-sequence-to-video fallback must pick the same format for a given job.
QString ffmpegPixelFormatForJob(const ArtifactRenderJob& job);
// True when the container/codec pair is capable of carrying an alpha channel.
bool outputFormatCarriesAlpha(const ArtifactRenderJob& job);

std::unique_ptr<IVideoEncodeBackend> createVideoEncodeBackend(
    const ArtifactRenderJob& job,
    QString* backendName,
    QString* errorMessage);

}
