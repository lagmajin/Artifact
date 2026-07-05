module;
#include <utility>
#include <QString>
#include <QJsonObject>
#include <wobjectdefs.h>
#include <memory>
#include <vector>
#include <map>

export module Artifact.Layer.Paint;

import Artifact.Layers.Abstract._2D;
import Artifact.Render.IRenderer;
import FloatRGBA;
import Frame.Position;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

/// ブラシストローク（Undo用）
struct BrushStroke {
    std::vector<QPointF> points;
    float radius = 10.0f;
    FloatRGBA color = {0.0f, 0.0f, 0.0f, 1.0f};
    float opacity = 1.0f;
    bool eraser = false;
};

/// ペイントレイヤー: フレームごとにラスター描画可能
class ArtifactPaintLayer : public ArtifactAbstract2DLayer {
    W_OBJECT(ArtifactPaintLayer)
private:
    class Impl;
    Impl* impl_;
public:
    ArtifactPaintLayer();
    ~ArtifactPaintLayer();
    ArtifactPaintLayer(const ArtifactPaintLayer&) = delete;
    ArtifactPaintLayer& operator=(const ArtifactPaintLayer&) = delete;

    void setComposition(void* comp) override;
    void draw(ArtifactIRenderer* renderer) override;
    bool isPaintLayer() const { return true; }
    QRectF localBounds() const override;

    // フレーム管理
    void newFrame(const FramePosition& pos);
    bool hasFrame(const FramePosition& pos) const;
    void removeFrame(const FramePosition& pos);
    void duplicateFrame(const FramePosition& src, const FramePosition& dst);
    void clearAllFrames();

    // ブラシ操作
    void applyStroke(const BrushStroke& stroke);
    void applyStrokeAtFrame(const BrushStroke& stroke, const FramePosition& frame);
    void undoLastStroke();
    bool canUndo() const;

    // フレームバッファアクセス
    ArtifactCore::ImageF32x4_RGBA* frameBuffer(const FramePosition& pos);
    void markDirty(const FramePosition& pos);

    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::ArtifactPaintLayer)
