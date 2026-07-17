module;

#include <memory>
#include <QString>

export module Artifact.Service.FootageInterpret;

import Media.SourceInterpret;
import Artifact.Project.Items;

export namespace Artifact {

class FootageInterpretService {
public:
    static FootageInterpretService& instance();

    ArtifactCore::InterpretImpactReport preflightChange(
        FootageItem* footage,
        double newFrameRate) const;

    bool applyFrameRateChange(
        FootageItem* footage,
        double newFrameRate,
        ArtifactCore::FrameRatePreserveMode mode,
        QString* errorOut = nullptr);

    ArtifactCore::SourceInterpretOverride currentOverride(
        const FootageItem* footage) const;

    bool clearOverride(FootageItem* footage);

    bool applyColorInterpretation(
        FootageItem* footage,
        const QString& inputColorSpace,
        const QString& inputTransferFunction,
        QString* errorOut = nullptr);

private:
    FootageInterpretService() = default;

    struct Impl;
    mutable std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
