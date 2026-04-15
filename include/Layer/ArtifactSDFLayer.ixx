module;
#include <utility>
#include <memory>
#include <vector>
#include <wobjectdefs.h>
#include <QString>
#include <QVariant>
#include <QImage>

export module Artifact.Layer.SDF;

import Artifact.Layer.Abstract;
import Property.Group;
import Color.Float;

export namespace Artifact {

// ---------------------------------------------------------------------------
// SDFObject: シーン内の SDF プリミティブ一つを表す
// ---------------------------------------------------------------------------
enum class SDFShapeType {
    Sphere = 0,
    Box,
    Torus
};

struct SDFObject {
    SDFShapeType shapeType = SDFShapeType::Sphere;

    // Transform (ワールド空間 px 単位)
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;

    float rotX = 0.0f;  // degrees
    float rotY = 0.0f;
    float rotZ = 0.0f;

    float scaleX = 50.0f;
    float scaleY = 50.0f;
    float scaleZ = 50.0f;

    // Surface
    ArtifactCore::FloatColor color{0.8f, 0.5f, 0.2f, 1.0f};

    // Shape-specific params
    float param0 = 0.0f;  // Torus: minor radius ratio (0..1)
};

// ---------------------------------------------------------------------------
// SDFOp: シーン結合演算
// ---------------------------------------------------------------------------
enum class SDFOp {
    Union,
    SmoothUnion
};

// ---------------------------------------------------------------------------
// ArtifactSDFLayer
// ---------------------------------------------------------------------------
class ArtifactSDFLayer : public ArtifactAbstractLayer {
    W_OBJECT(ArtifactSDFLayer)
public:
    ArtifactSDFLayer();
    virtual ~ArtifactSDFLayer();

    // ArtifactAbstractLayer overrides
    void draw(ArtifactIRenderer* renderer) override;
    UniString className() const override { return "ArtifactSDFLayer"; }

    // SDF scene management
    void clearObjects();
    void addObject(const SDFObject& obj);
    void setObjectAt(int index, const SDFObject& obj);
    void removeObjectAt(int index);
    int objectCount() const;
    const SDFObject& objectAt(int index) const;

    SDFOp combineOp() const;
    void setCombineOp(SDFOp op);

    float smoothK() const;
    void setSmoothing(float k);

    // Output resolution (independent of canvas; SDF is rendered at this size then composited)
    int renderWidth() const;
    int renderHeight() const;
    void setRenderSize(int w, int h);

    // CPU-side SDF evaluation → QImage (ARGB32_Premultiplied)
    QImage toQImage() const;

    // Inspector
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

private:
    struct Impl;
    Impl* sdfImpl_;
};

using ArtifactSDFLayerPtr = std::shared_ptr<ArtifactSDFLayer>;

} // namespace Artifact
