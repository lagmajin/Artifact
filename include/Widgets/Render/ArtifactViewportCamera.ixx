module;
#include <utility>
#include <QPointF>
#include <QTransform>

export module Artifact.Widgets.Render.Camera;

export namespace Artifact {

class ArtifactViewportCamera {
public:
    ArtifactViewportCamera() = default;
    ~ArtifactViewportCamera() = default;

    QPointF pan() const { return pan_; }
    void setPan(const QPointF& offset) { pan_ = offset; }
    void translate(const QPointF& delta) { pan_ += delta; }

    float zoom() const { return zoom_; }
    void setZoom(float scale) { zoom_ = std::max(0.01f, std::min(scale, 100.0f)); }
    void zoomRelative(float factor) { setZoom(zoom_ * factor); }
    void zoomAdd(float delta) { setZoom(zoom_ + delta); }

    QTransform getViewTransform(float widgetWidth, float widgetHeight) const {
        QTransform t;
        // Pan in screen space so zoom does not scale pan speed
        t.translate(widgetWidth / 2.0 + pan_.x(), widgetHeight / 2.0 + pan_.y());
        t.scale(zoom_, zoom_);
        return t;
    }

    QPointF mapToScene(const QPointF& widgetPos, float widgetWidth, float widgetHeight) const {
        QTransform vt = getViewTransform(widgetWidth, widgetHeight);
        bool invertible = false;
        QTransform inv = vt.inverted(&invertible);
        if (!invertible) return widgetPos;
        return inv.map(widgetPos);
    }

    QPointF mapFromScene(const QPointF& scenePos, float widgetWidth, float widgetHeight) const {
        return getViewTransform(widgetWidth, widgetHeight).map(scenePos);
    }

    void reset() {
        pan_ = QPointF(0.0, 0.0);
        zoom_ = 1.0f;
    }

private:
    QPointF pan_ = {0.0f, 0.0f};
    float zoom_ = 1.0f;
};

} // namespace Artifact
