module;
#include <memory>
#include <vector>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Rasterizer.DataMosh;

import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {
using namespace ArtifactCore;

/// Simulates digital datamosh glitches by repeating motion
/// vectors and duplicating frame blocks. Creates I-frame/P-frame
/// corruption style artifacts.
class DataMoshEffect : public ArtifactAbstractEffect {
public:
    DataMoshEffect();
    ~DataMoshEffect() override;

    /// Probability a block is moshed (0-1).
    float intensity() const;
    void  setIntensity(float v);

    /// How many frames to hold a moshed block.
    int   holdFrames() const;
    void  setHoldFrames(int v);

    /// Block size for moshing (4-64 pixels).
    int   blockSize() const;
    void  setBlockSize(int v);

    /// Blend between moshed and original.
    float blend() const;
    void  setBlend(float v);

    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& n, const QVariant& v) override;
    bool supportsGPU() const override { return false; }

private:
    float intensity_=0.5f; int holdFrames_=4,blockSize_=16; float blend_=0.8f;
    void syncImpls();
};

} // namespace Artifact
