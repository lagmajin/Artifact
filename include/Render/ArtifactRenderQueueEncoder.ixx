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

std::unique_ptr<IVideoEncodeBackend> createVideoEncodeBackend(
    const ArtifactRenderJob& job,
    QString* backendName,
    QString* errorMessage);

}
