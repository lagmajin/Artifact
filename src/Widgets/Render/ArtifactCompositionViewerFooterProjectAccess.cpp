module;

module Artifact.Widgets.CompositionFooter;

import Artifact.Service.Project;

namespace Artifact {

int compositionFooterPreviewQualityPreset()
{
    if (auto* service = ArtifactProjectService::instance()) {
        return static_cast<int>(service->previewQualityPreset());
    }
    return -1;
}

int compositionFooterQualityPresetValue(const int presetKind)
{
    switch (presetKind) {
    case 0:
        return static_cast<int>(PreviewQualityPreset::Draft);
    case 1:
        return static_cast<int>(PreviewQualityPreset::Preview);
    default:
        return static_cast<int>(PreviewQualityPreset::Final);
    }
}

void setCompositionFooterPreviewQualityPreset(const int preset)
{
    if (auto* service = ArtifactProjectService::instance()) {
        service->setPreviewQualityPreset(
            static_cast<PreviewQualityPreset>(preset));
    }
}

} // namespace Artifact
