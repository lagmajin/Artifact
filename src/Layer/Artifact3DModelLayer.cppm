module;
#include <utility>
#include <QObject>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <QFileDialog>
#include <QFileInfo>
module Artifact.Layers.Model3D;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Color.Float;
import Mesh;
import Time.Rational;
import MeshImporter;
import Utils.String.UniString;

namespace Artifact {

namespace {
Artifact::Detail::float3 toFloat3(const QVector3D& v)
{
    return {v.x(), v.y(), v.z()};
}
}

class Artifact3DLayer::Impl {
public:
    RenderMode renderMode_ = RenderMode::Wireframe;
    Mesh mesh_; // The 3D mesh data
    bool meshLoaded_ = false;
    Impl() {}
    ~Impl() {}
};

Artifact3DLayer::Artifact3DLayer() : impl_(new Impl()) {
    // Set the 3D flag directly as well (redundant but safe)
    setIs3D(true);
    // Load default mesh (cube)
    loadFromFile();
}
Artifact3DLayer::~Artifact3DLayer() { delete impl_; }

void Artifact3DLayer::loadFromFile() {
    // Try loading via MeshImporter (ufbx for FBX, tinyobj for OBJ)
    ArtifactCore::MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString("")); // Will be set by user
    
    if (mesh && mesh->vertexCount() > 0) {
        impl_->mesh_ = *mesh;
        impl_->meshLoaded_ = true;
        return;
    }
    
    // Fallback: create a simple cube mesh programmatically
    if (!impl_->meshLoaded_) {
        createCubeMesh();
        impl_->meshLoaded_ = true;
    }
}

void Artifact3DLayer::loadFromFile(const QString& filePath) {
    if (filePath.isEmpty()) {
        loadFromFile();
        return;
    }
    
    ArtifactCore::MeshImporter importer;
    auto mesh = importer.importMeshFromFile(UniString(filePath));
    
    if (mesh && mesh->vertexCount() > 0) {
        impl_->mesh_ = *mesh;
        impl_->meshLoaded_ = true;
        setLayerName(QFileInfo(filePath).baseName());
        return;
    }
    
    // Fallback to cube on failure
    qWarning() << "Failed to load mesh from:" << filePath << "- using default cube";
    if (!impl_->meshLoaded_) {
        createCubeMesh();
        impl_->meshLoaded_ = true;
    }
}

void Artifact3DLayer::createCubeMesh() {
    // Create a simple cube mesh
    const float halfSize = 0.5f;
    QVector<QVector3D> positions = {
        QVector3D(-halfSize, -halfSize, -halfSize), // 0
        QVector3D( halfSize, -halfSize, -halfSize), // 1
        QVector3D( halfSize,  halfSize, -halfSize), // 2
        QVector3D(-halfSize,  halfSize, -halfSize), // 3
        QVector3D(-halfSize, -halfSize,  halfSize), // 4
        QVector3D( halfSize, -halfSize,  halfSize), // 5
        QVector3D( halfSize,  halfSize,  halfSize), // 6
        QVector3D(-halfSize,  halfSize,  halfSize)  // 7
    };

    impl_->mesh_.setVertexCount(8);
    auto& vertexAttrs = impl_->mesh_.vertexAttributes();
    auto positionAttr = vertexAttrs.add<QVector3D>("position");
    positionAttr->data() = positions;

    // Add polygons (triangulated for simplicity)
    // Bottom face
    impl_->mesh_.addPolygon({0, 1, 2});
    impl_->mesh_.addPolygon({0, 2, 3});
    // Top face
    impl_->mesh_.addPolygon({4, 5, 6});
    impl_->mesh_.addPolygon({4, 6, 7});
    // Front face
    impl_->mesh_.addPolygon({0, 1, 5});
    impl_->mesh_.addPolygon({0, 5, 4});
    // Back face
    impl_->mesh_.addPolygon({3, 2, 6});
    impl_->mesh_.addPolygon({3, 6, 7});
    // Left face
    impl_->mesh_.addPolygon({0, 3, 7});
    impl_->mesh_.addPolygon({0, 7, 4});
    // Right face
    impl_->mesh_.addPolygon({1, 2, 6});
    impl_->mesh_.addPolygon({1, 6, 5});
}

RenderMode Artifact3DLayer::renderMode() const {
    return impl_->renderMode_;
}

void Artifact3DLayer::setRenderMode(RenderMode mode) {
    impl_->renderMode_ = mode;
}

void Artifact3DLayer::draw(ArtifactIRenderer* renderer) {
    if (!renderer || !isVisible() || !impl_->meshLoaded_) {
        return;
    }

    // Get transform
    const auto& t3 = transform3D();
    const RationalTime frameTime(currentFrame(), 30); // Assume 30fps for now
    const QVector3D position(t3.positionXAt(frameTime), t3.positionYAt(frameTime), t3.positionZAt(frameTime));
    const QVector3D scale(t3.scaleXAt(frameTime), t3.scaleYAt(frameTime), 1.0f); // Z scale not implemented yet
    const QVector3D anchor(t3.anchorXAt(frameTime), t3.anchorYAt(frameTime), t3.anchorZAt(frameTime));

    // Get mesh data
    const auto& vertexAttrs = impl_->mesh_.vertexAttributes();
    const auto positions = vertexAttrs.get<QVector3D>("position");
    if (!positions || positions->data().isEmpty()) {
        return;
    }

    // Transform vertices
    QVector<QVector3D> transformedVertices;
    transformedVertices.reserve(positions->data().size());
    for (const auto& pos : positions->data()) {
        QVector3D v = pos;
        v *= scale;
        v += position - anchor; // Apply anchor offset
        transformedVertices.append(v);
    }

    // Draw based on render mode
    const FloatColor wireframeColor{1.0f, 1.0f, 1.0f, opacity()};
    const FloatColor solidColor{0.8f, 0.8f, 0.8f, opacity()};
    const float thickness = 2.0f;

    if (impl_->renderMode_ == RenderMode::Solid) {
        // Draw filled polygons with fan triangulation for N-gons.
        // PrimitiveRenderer3D only exposes quad filling, so use a degenerate
        // quad where the first triangle carries the face and the second
        // triangle collapses away.
        const FloatColor color = solidColor;
        for (int i = 0; i < impl_->mesh_.polygonCount(); ++i) {
            const auto vertexIndices = impl_->mesh_.getPolygonVertices(i);
            if (vertexIndices.size() >= 3) {
                // Fan triangulation: v0-v1-v2, v0-v2-v3, v0-v3-v4, ...
                for (size_t j = 1; j + 1 < vertexIndices.size(); ++j) {
                    const QVector3D& v0 = transformedVertices[vertexIndices[0]];
                    const QVector3D& v1 = transformedVertices[vertexIndices[j]];
                    const QVector3D& v2 = transformedVertices[vertexIndices[j + 1]];
                    renderer->draw3DQuad(toFloat3(v0), toFloat3(v1), toFloat3(v2), toFloat3(v0), color);
                }
            }
        }
    } else {
        // Wireframe mode: draw edges
        const FloatColor color = wireframeColor;
        for (int i = 0; i < impl_->mesh_.polygonCount(); ++i) {
            const auto vertexIndices = impl_->mesh_.getPolygonVertices(i);
            for (size_t j = 0; j < vertexIndices.size(); ++j) {
                const QVector3D& v0 = transformedVertices[vertexIndices[j]];
                const QVector3D& v1 = transformedVertices[vertexIndices[(j + 1) % vertexIndices.size()]];
                renderer->draw3DLine(toFloat3(v0), toFloat3(v1), color, thickness);
            }
        }
    }
}

void Artifact3DLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
    // For now, same as regular draw
    draw(renderer);
}

std::vector<ArtifactCore::PropertyGroup> Artifact3DLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    using namespace ArtifactCore;
    PropertyGroup renderGroup(QStringLiteral("3D Render"));

    auto renderModeProp = persistentLayerProperty(QStringLiteral("render.mode"), PropertyType::Integer,
                                                  static_cast<int>(renderMode()), -50);
    renderModeProp->setDisplayLabel(QStringLiteral("Render Mode"));
    renderGroup.addProperty(renderModeProp);

    groups.push_back(renderGroup);
    return groups;
}

bool Artifact3DLayer::setLayerPropertyValue(const QString &propertyPath, const QVariant &value) {
    if (propertyPath == QStringLiteral("render.mode")) {
        int modeInt = value.toInt();
        if (modeInt >= static_cast<int>(RenderMode::Wireframe) && modeInt <= static_cast<int>(RenderMode::Solid)) {
            setRenderMode(static_cast<RenderMode>(modeInt));
            Q_EMIT changed();
            return true;
        }
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
