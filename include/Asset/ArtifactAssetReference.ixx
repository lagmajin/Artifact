module;
#include <QString>

export module Artifact.Asset.Reference;

export namespace Artifact {

enum class ArtifactAssetReferenceKind {
    LocalPath,
    OpenAssetIOEntity
};

struct ArtifactAssetReference {
    ArtifactAssetReferenceKind kind = ArtifactAssetReferenceKind::LocalPath;
    QString identifier;
    QString localPath;
    QString displayName;

    bool isExternal() const noexcept {
        return kind == ArtifactAssetReferenceKind::OpenAssetIOEntity;
    }

    bool isValid() const noexcept {
        return !identifier.trimmed().isEmpty() || !localPath.trimmed().isEmpty();
    }
};

} // namespace Artifact
