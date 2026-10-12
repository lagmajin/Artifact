module;
#include <cstdint>
#include <vector>
#include <QJsonObject>
#include <QObject>
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVariant>

export module Artifact.Layer.MpmSand2D;

import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Core.ArtifactArray;
import FloatRGBA;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

class ArtifactMpmSand2DLayer : public ArtifactAbstractLayer {
private:
    mutable ArtifactCore::ArtifactArray<float> densityField_;
    mutable ArtifactCore::ArtifactArray<float> densityAlphaField_;
    mutable ArtifactCore::ImageF32x4_RGBA cpuSurface_;
    mutable ArtifactCore::ArtifactStaticArray<QImage, 2>
        cpuRenderSurfaceBuffers_;
    ArtifactCore::ArtifactStaticArray<
        ArtifactCore::ArtifactArray<MpmSurfaceVertex>, 2> surfaceVertexBuffers_;
    float youngModulus_ = 12000.0f;
    float poissonRatio_ = 0.2f;
    float frictionAngleRadians_ = 0.5235988f;
    float density_ = 1.6f;
    float damping_ = 0.02f;
    float gravityY_ = 980.0f;
    int columns_ = 96;
    int rows_ = 48;
    bool configureSolver();
public:
    ArtifactMpmSand2DLayer();
    void draw(ArtifactIRenderer* renderer) override;
    void draw(ArtifactIRenderer* renderer, float opacityOverride);
    void setComposition(QObject* composition) override;
    bool isCompositionAttachmentReady() const override;
    bool isGridExpansionLimitReached() const;
    float simulationBacklogSeconds() const;
    QRectF particleRenderBounds() const;
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;
    QImage getSoftwareRenderSurface(int width, int height,
                                    QRectF* outLocalBounds = nullptr,
                                    bool preserveRequestedSize = false) const;
    QImage getThumbnail(int width = 128, int height = 128) const override;
    QJsonObject toJson() const override;
    void fromJsonProperties(const QJsonObject& object) override;
    bool isAdjustmentLayer() const override;
    bool isNullLayer() const override;
};

}
