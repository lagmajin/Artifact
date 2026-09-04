module;
#include <utility>
#include <wobjectimpl.h>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialog>
#include <QPlainTextEdit>
#include <QStyleOptionGraphicsItem>
#include <qevent.h>
#include <QApplication>
#include <QClipboard>
#include <QWidget>
#include <QPalette>
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <functional>

module Artifact.Widgets.ShaderGraphWidget;

import Artifact.ShaderNode.Core;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.CompositionRenderController;

namespace Artifact {

W_OBJECT_IMPL(ArtifactShaderGraphWidget)

namespace {

constexpr float kNodeWidth = 190.0f;
constexpr float kHeaderHeight = 26.0f;
constexpr float kRowHeight = 20.0f;
constexpr float kPortRadius = 5.0f;
constexpr float kPortHitRadius = 9.0f;

QColor shaderPortColor(ShaderNode::PinType type) {
    using PinType = ShaderNode::PinType;
    switch (type) {
        case PinType::Float: return QColor(160, 160, 160);
        case PinType::Vector2: return QColor(120, 200, 120);
        case PinType::Vector3: return QColor(220, 220, 120);
        case PinType::Vector4: return QColor(200, 140, 220);
        case PinType::Texture2D: return QColor(220, 150, 80);
        case PinType::Shader: return QColor(120, 200, 235);
        default: return QColor(140, 140, 140);
    }
}

QString shaderPinTypeName(ShaderNode::PinType type) {
    using PinType = ShaderNode::PinType;
    switch (type) {
        case PinType::Float: return QStringLiteral("F");
        case PinType::Vector2: return QStringLiteral("V2");
        case PinType::Vector3: return QStringLiteral("V3");
        case PinType::Vector4: return QStringLiteral("V4");
        case PinType::Texture2D: return QStringLiteral("T");
        case PinType::Shader: return QStringLiteral("S");
        default: return QStringLiteral("?");
    }
}

bool parseFloatList(const QString& text, std::vector<float>& out, int count) {
    out.clear();
    const QString normalized = QString(text).replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != count) {
        return false;
    }
    for (const QString& part : parts) {
        bool ok = false;
        const float v = part.toFloat(&ok);
        if (!ok || !std::isfinite(v)) {
            return false;
        }
        out.push_back(v);
    }
    return true;
}

ArtifactCompositionEditor* findActiveCompositionEditor(QWidget* origin) {
    QWidget* window = origin ? origin->window() : nullptr;
    if (!window) {
        return nullptr;
    }
    const auto editors = window->findChildren<ArtifactCompositionEditor*>();
    if (editors.isEmpty()) {
        return nullptr;
    }
    QWidget* focus = QApplication::focusWidget();
    for (ArtifactCompositionEditor* editor : editors) {
        if (!editor || !editor->isVisible()) {
            continue;
        }
        if (focus && (editor == focus || editor->isAncestorOf(focus))) {
            return editor;
        }
    }
    for (ArtifactCompositionEditor* editor : editors) {
        if (editor && editor->isVisible()) {
            return editor;
        }
    }
    return editors.front();
}

} // namespace

enum class ShaderGraphNodeKind {
    Value, RGB, TexCoord, ImageTexture, Noise, Voronoi,
    Math, MixRGB, ColorRamp, Principled, MixShader,
    Geometry, Fresnel, LayerWeight, Blackbody, NormalMap, Bump,
    Invert, Gamma, HueSatVal, Luminance, MapRange, Clamp, MaterialOutput
};

struct ShaderGraphNodeDef {
    ShaderGraphNodeKind kind;
    const char* menuLabel;
    const char* title;
    const char* nodeId;
};

const std::vector<ShaderGraphNodeDef>& shaderGraphNodeDefs() {    static const std::vector<ShaderGraphNodeDef> defs = {
        {ShaderGraphNodeKind::Value, "Input > Value", "Value", "value"},
        {ShaderGraphNodeKind::RGB, "Input > RGB", "RGB", "rgb"},
        {ShaderGraphNodeKind::TexCoord, "Input > Texture Coordinate", "Texture Coordinate", "texcoord"},
        {ShaderGraphNodeKind::Geometry, "Input > Geometry", "Geometry", "geometry"},
        {ShaderGraphNodeKind::ImageTexture, "Texture > Image Texture", "Image Texture", "image"},
        {ShaderGraphNodeKind::Noise, "Texture > Noise Texture", "Noise Texture", "noise"},
        {ShaderGraphNodeKind::Voronoi, "Texture > Voronoi Texture", "Voronoi Texture", "voronoi"},
        {ShaderGraphNodeKind::Math, "Converter > Math", "Math", "math"},
        {ShaderGraphNodeKind::MixRGB, "Color > Mix", "Mix", "mix"},
        {ShaderGraphNodeKind::ColorRamp, "Color > Color Ramp", "Color Ramp", "ramp"},
        {ShaderGraphNodeKind::Invert, "Color > Invert", "Invert", "invert"},
        {ShaderGraphNodeKind::Gamma, "Color > Gamma", "Gamma", "gamma"},
        {ShaderGraphNodeKind::HueSatVal, "Color > Hue Saturation Value", "Hue Saturation Value", "hsv"},
        {ShaderGraphNodeKind::Luminance, "Color > Luminance", "Luminance", "luma"},
        {ShaderGraphNodeKind::MapRange, "Converter > Map Range", "Map Range", "maprange"},
        {ShaderGraphNodeKind::Clamp, "Converter > Clamp", "Clamp", "clamp"},
        {ShaderGraphNodeKind::Principled, "Shader > Principled BSDF", "Principled BSDF", "principled"},
        {ShaderGraphNodeKind::MixShader, "Shader > Mix Shader", "Mix Shader", "mixshader"},
        {ShaderGraphNodeKind::Fresnel, "Shader > Fresnel", "Fresnel", "fresnel"},
        {ShaderGraphNodeKind::LayerWeight, "Shader > Layer Weight", "Layer Weight", "layerweight"},
        {ShaderGraphNodeKind::Blackbody, "Shader > Blackbody", "Blackbody", "blackbody"},
        {ShaderGraphNodeKind::NormalMap, "Vector > Normal Map", "Normal Map", "normalmap"},
        {ShaderGraphNodeKind::Bump, "Vector > Bump", "Bump", "bump"},
        {ShaderGraphNodeKind::MaterialOutput, "Output > Material Output", "Material Output", "output"},
    };
    return defs;
}

class ShaderGraphNodeItem : public QGraphicsRectItem {
public:
    ShaderNode::ShaderNodeBase* node = nullptr;
    ShaderGraphNodeKind kind = ShaderGraphNodeKind::Value;
    QString title;
    QString summary;
    std::function<void()> positionChanged;

    ShaderGraphNodeItem(ShaderNode::ShaderNodeBase* backendNode,
                        ShaderGraphNodeKind nodeKind,
                        const QString& nodeTitle)
        : node(backendNode), kind(nodeKind), title(nodeTitle) {
        setFlag(ItemIsMovable);
        setFlag(ItemIsSelectable);
        setFlag(ItemSendsGeometryChanges);
        refreshRect();
    }

    int inputCount() const {
        return node ? static_cast<int>(node->inputs.size()) : 0;
    }

    int outputCount() const {
        return node ? static_cast<int>(node->outputs.size()) : 0;
    }

    int rowCount() const {
        return std::max(1, std::max(inputCount(), outputCount()));
    }

    void refreshRect() {
        setRect(0, 0, kNodeWidth, kHeaderHeight + rowCount() * kRowHeight + 4.0f);
    }

    QPointF inputPortPos(int index) const {
        return mapToScene(QPointF(0.0f, kHeaderHeight + index * kRowHeight + kRowHeight * 0.5f));
    }

    QPointF outputPortPos(int index) const {
        return mapToScene(QPointF(kNodeWidth, kHeaderHeight + index * kRowHeight + kRowHeight * 0.5f));
    }

    // Returns {isInput, pinIndex} or {false, -1} when no port hit.
    std::pair<bool, int> portAt(const QPointF& scenePos) const {
        for (int i = 0; i < inputCount(); ++i) {
            const QPointF d = scenePos - inputPortPos(i);
            if (d.x() * d.x() + d.y() * d.y() <= kPortHitRadius * kPortHitRadius) {
                return {true, i};
            }
        }
        for (int i = 0; i < outputCount(); ++i) {
            const QPointF d = scenePos - outputPortPos(i);
            if (d.x() * d.x() + d.y() * d.y() <= kPortHitRadius * kPortHitRadius) {
                return {false, i};
            }
        }
        return {false, -1};
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        const QRectF bounds = rect();
        const bool selected = isSelected();
        painter->setPen(QPen(selected ? QColor(240, 200, 90) : QColor(70, 70, 75), selected ? 2.0f : 1.0f));
        painter->setBrush(QColor(42, 42, 46));
        painter->drawRoundedRect(bounds, 5.0f, 5.0f);
        painter->setBrush(QColor(58, 58, 64));
        painter->drawRoundedRect(QRectF(bounds.left(), bounds.top(), bounds.width(), kHeaderHeight), 5.0f, 5.0f);
        painter->setPen(QColor(230, 230, 230));
        QFont titleFont = painter->font();
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->drawText(QRectF(bounds.left() + 8.0f, bounds.top(), bounds.width() - 16.0f, kHeaderHeight),
                          Qt::AlignLeft | Qt::AlignVCenter, title);
        painter->setFont(QFont());
        QFont smallFont = painter->font();
        smallFont.setPointSize(std::max(7, smallFont.pointSize() - 1));
        painter->setFont(smallFont);
        for (int i = 0; i < inputCount(); ++i) {
            const QPointF local = mapFromScene(inputPortPos(i));
            painter->setPen(QPen(QColor(20, 20, 20)));
            painter->setBrush(shaderPortColor(node->inputs[static_cast<size_t>(i)]->type));
            painter->drawEllipse(local, kPortRadius, kPortRadius);
            painter->setPen(QColor(200, 200, 200));
            painter->drawText(QRectF(12.0f, kHeaderHeight + i * kRowHeight, kNodeWidth - 24.0f, kRowHeight),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QString::fromStdString(node->inputs[static_cast<size_t>(i)]->name));
        }
        for (int i = 0; i < outputCount(); ++i) {
            const QPointF local = mapFromScene(outputPortPos(i));
            painter->setPen(QPen(QColor(20, 20, 20)));
            painter->setBrush(shaderPortColor(node->outputs[static_cast<size_t>(i)]->type));
            painter->drawEllipse(local, kPortRadius, kPortRadius);
            painter->setPen(QColor(200, 200, 200));
            painter->drawText(QRectF(12.0f, kHeaderHeight + (rowCount() - outputCount() + i) * kRowHeight,
                                     kNodeWidth - 24.0f, kRowHeight),
                              Qt::AlignRight | Qt::AlignVCenter,
                              QString::fromStdString(node->outputs[static_cast<size_t>(i)]->name));
        }
        if (!summary.isEmpty()) {
            painter->setPen(QColor(150, 150, 155));
            painter->drawText(QRectF(bounds.left() + 8.0f, bounds.bottom() - 16.0f,
                                     bounds.width() - 16.0f, 14.0f),
                              Qt::AlignLeft | Qt::AlignVCenter, summary);
        }
    }

    QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                        const QVariant& value) override {
        const QVariant result = QGraphicsRectItem::itemChange(change, value);
        if (change == QGraphicsItem::ItemPositionHasChanged && positionChanged) {
            positionChanged();
        }
        return result;
    }
};

class ShaderGraphEdgeItem : public QGraphicsPathItem {
public:
    ShaderNode::Pin* fromPin = nullptr;
    ShaderNode::Pin* toPin = nullptr;
    ShaderGraphNodeItem* fromItem = nullptr;
    ShaderGraphNodeItem* toItem = nullptr;
    int fromIndex = -1;
    int toIndex = -1;

    void refreshPath() {
        if (!fromItem || !toItem) {
            return;
        }
        const QPointF p0 = fromItem->outputPortPos(fromIndex);
        const QPointF p1 = toItem->inputPortPos(toIndex);
        const double dx = std::max(24.0, std::abs(p1.x() - p0.x()) * 0.5);
        QPainterPath path(p0);
        path.cubicTo(p0 + QPointF(dx, 0), p1 - QPointF(dx, 0), p1);
        setPath(path);
    }
};

class ArtifactShaderGraphWidget::Impl {
public:
    ArtifactShaderGraphWidget* owner_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;
    QLabel* status_ = nullptr;
    ShaderNode::NodeGraph graph_;
    std::vector<ShaderGraphNodeItem*> items_;
    std::vector<ShaderGraphEdgeItem*> edges_;
    int nodeCounter_ = 0;

    struct PendingLink {
        bool active = false;
        ShaderGraphNodeItem* fromItem = nullptr;
        int fromIndex = -1;
        QGraphicsPathItem* preview = nullptr;
    } pending_;

    class GraphView : public QGraphicsView {
    public:
        Impl* impl_ = nullptr;
        GraphView(QGraphicsScene* scene, Impl* impl, QWidget* parent = nullptr)
            : QGraphicsView(scene, parent), impl_(impl) {}

        void mousePressEvent(QMouseEvent* event) override {
            if (impl_ && event->button() == Qt::LeftButton) {
                const QPointF scenePos = mapToScene(event->position().toPoint());
                if (impl_->handlePortPress(scenePos)) {
                    return;
                }
            }
            QGraphicsView::mousePressEvent(event);
        }

        void mouseMoveEvent(QMouseEvent* event) override {
            if (impl_) {
                impl_->handleMouseMove(mapToScene(event->position().toPoint()));
            }
            QGraphicsView::mouseMoveEvent(event);
        }

        void contextMenuEvent(QContextMenuEvent* event) override {
            if (impl_) {
                impl_->showContextMenu(event->globalPos(), mapToScene(event->pos()));
                event->accept();
                return;
            }
            QGraphicsView::contextMenuEvent(event);
        }

        void keyPressEvent(QKeyEvent* event) override {
            if (impl_ && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
                !event->isAutoRepeat()) {
                impl_->deleteSelected();
                event->accept();
                return;
            }
            if (impl_ && event->key() == Qt::Key_Escape && !event->isAutoRepeat()) {
                impl_->cancelPending();
                event->accept();
                return;
            }
            QGraphicsView::keyPressEvent(event);
        }
    };

    void setupUi(QWidget* parent) {
        auto* layout = new QVBoxLayout(parent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        scene_ = new QGraphicsScene(parent);
        auto* graphView = new GraphView(scene_, this, parent);
        graphView->setAccessibleName(QStringLiteral("Shader graph view"));
        graphView->setAccessibleDescription(
            QStringLiteral("Edit the Blender-style material node graph."));
        graphView->setRenderHint(QPainter::Antialiasing);
        graphView->setDragMode(QGraphicsView::RubberBandDrag);
        graphView->setBackgroundBrush(QColor(30, 30, 32));
        view_ = graphView;
        layout->addWidget(view_);
        status_ = new QLabel(QStringLiteral("Right-click: Add node / Apply. Drag from an output port to link."), parent);
        status_->setWordWrap(true);
        status_->setMinimumHeight(28);
        QPalette statusPalette = status_->palette();
        statusPalette.setColor(QPalette::Window, QColor(38, 38, 42));
        statusPalette.setColor(QPalette::WindowText, QColor(200, 200, 200));
        status_->setPalette(statusPalette);
        status_->setAutoFillBackground(true);
        layout->addWidget(status_);
        setStatus(QStringLiteral("Empty graph. Right-click to add nodes."));
    }

    void setStatus(const QString& text) {
        if (status_) {
            status_->setText(text);
        }
    }

    ShaderGraphNodeItem* itemForNode(ShaderNode::ShaderNodeBase* node) const {
        for (ShaderGraphNodeItem* item : items_) {
            if (item && item->node == node) {
                return item;
            }
        }
        return nullptr;
    }

    ShaderNode::ShaderNodeBase* createBackendNode(ShaderGraphNodeKind kind, const std::string& id) {        using namespace ShaderNode;
        switch (kind) {
            case ShaderGraphNodeKind::Value: return graph_.addNode(std::make_unique<ValueNode>(id));
            case ShaderGraphNodeKind::RGB: return graph_.addNode(std::make_unique<RGBNode>(id));
            case ShaderGraphNodeKind::TexCoord: return graph_.addNode(std::make_unique<TexCoordNode>(id));
            case ShaderGraphNodeKind::ImageTexture: return graph_.addNode(std::make_unique<ImageTextureNode>(id));
            case ShaderGraphNodeKind::Noise: return graph_.addNode(std::make_unique<NoiseTextureNode>(id));
            case ShaderGraphNodeKind::Voronoi: return graph_.addNode(std::make_unique<VoronoiTextureNode>(id));
            case ShaderGraphNodeKind::Math: return graph_.addNode(std::make_unique<MathNode>(id));
            case ShaderGraphNodeKind::MixRGB: return graph_.addNode(std::make_unique<MixRGBNode>(id));
            case ShaderGraphNodeKind::ColorRamp: return graph_.addNode(std::make_unique<ColorRampNode>(id));
            case ShaderGraphNodeKind::Principled: return graph_.addNode(std::make_unique<PrincipledBSDFNode>(id));
            case ShaderGraphNodeKind::MixShader: return graph_.addNode(std::make_unique<MixShaderNode>(id));
            case ShaderGraphNodeKind::Geometry: return graph_.addNode(std::make_unique<GeometryNode>(id));
            case ShaderGraphNodeKind::Fresnel: return graph_.addNode(std::make_unique<FresnelNode>(id));
            case ShaderGraphNodeKind::LayerWeight: return graph_.addNode(std::make_unique<LayerWeightNode>(id));
            case ShaderGraphNodeKind::Blackbody: return graph_.addNode(std::make_unique<BlackbodyNode>(id));
            case ShaderGraphNodeKind::NormalMap: return graph_.addNode(std::make_unique<NormalMapNode>(id));
            case ShaderGraphNodeKind::Bump: return graph_.addNode(std::make_unique<BumpNode>(id));
            case ShaderGraphNodeKind::Invert: return graph_.addNode(std::make_unique<InvertNode>(id));
            case ShaderGraphNodeKind::Gamma: return graph_.addNode(std::make_unique<GammaNode>(id));
            case ShaderGraphNodeKind::HueSatVal: return graph_.addNode(std::make_unique<HueSatValNode>(id));
            case ShaderGraphNodeKind::Luminance: return graph_.addNode(std::make_unique<LuminanceNode>(id));
            case ShaderGraphNodeKind::MapRange: return graph_.addNode(std::make_unique<MapRangeNode>(id));
            case ShaderGraphNodeKind::Clamp: return graph_.addNode(std::make_unique<ClampNode>(id));
            case ShaderGraphNodeKind::MaterialOutput: return graph_.addNode(std::make_unique<MaterialOutputNode>(id));
        }
        return nullptr;
    }

    static bool kindForBackendNode(ShaderNode::ShaderNodeBase* node, ShaderGraphNodeKind& kind) {
        using namespace ShaderNode;
        if (!node) {
            return false;
        }
        if (dynamic_cast<ValueNode*>(node)) { kind = ShaderGraphNodeKind::Value; return true; }
        if (dynamic_cast<RGBNode*>(node)) { kind = ShaderGraphNodeKind::RGB; return true; }
        if (dynamic_cast<TexCoordNode*>(node)) { kind = ShaderGraphNodeKind::TexCoord; return true; }
        if (dynamic_cast<ImageTextureNode*>(node)) { kind = ShaderGraphNodeKind::ImageTexture; return true; }
        if (dynamic_cast<NoiseTextureNode*>(node)) { kind = ShaderGraphNodeKind::Noise; return true; }
        if (dynamic_cast<VoronoiTextureNode*>(node)) { kind = ShaderGraphNodeKind::Voronoi; return true; }
        if (dynamic_cast<MathNode*>(node)) { kind = ShaderGraphNodeKind::Math; return true; }
        if (dynamic_cast<MixRGBNode*>(node)) { kind = ShaderGraphNodeKind::MixRGB; return true; }
        if (dynamic_cast<ColorRampNode*>(node)) { kind = ShaderGraphNodeKind::ColorRamp; return true; }
        if (dynamic_cast<PrincipledBSDFNode*>(node)) { kind = ShaderGraphNodeKind::Principled; return true; }
        if (dynamic_cast<MixShaderNode*>(node)) { kind = ShaderGraphNodeKind::MixShader; return true; }
        if (dynamic_cast<GeometryNode*>(node)) { kind = ShaderGraphNodeKind::Geometry; return true; }
        if (dynamic_cast<FresnelNode*>(node)) { kind = ShaderGraphNodeKind::Fresnel; return true; }
        if (dynamic_cast<LayerWeightNode*>(node)) { kind = ShaderGraphNodeKind::LayerWeight; return true; }
        if (dynamic_cast<BlackbodyNode*>(node)) { kind = ShaderGraphNodeKind::Blackbody; return true; }
        if (dynamic_cast<NormalMapNode*>(node)) { kind = ShaderGraphNodeKind::NormalMap; return true; }
        if (dynamic_cast<BumpNode*>(node)) { kind = ShaderGraphNodeKind::Bump; return true; }
        if (dynamic_cast<InvertNode*>(node)) { kind = ShaderGraphNodeKind::Invert; return true; }
        if (dynamic_cast<GammaNode*>(node)) { kind = ShaderGraphNodeKind::Gamma; return true; }
        if (dynamic_cast<HueSatValNode*>(node)) { kind = ShaderGraphNodeKind::HueSatVal; return true; }
        if (dynamic_cast<LuminanceNode*>(node)) { kind = ShaderGraphNodeKind::Luminance; return true; }
        if (dynamic_cast<MapRangeNode*>(node)) { kind = ShaderGraphNodeKind::MapRange; return true; }
        if (dynamic_cast<ClampNode*>(node)) { kind = ShaderGraphNodeKind::Clamp; return true; }
        if (dynamic_cast<MaterialOutputNode*>(node)) { kind = ShaderGraphNodeKind::MaterialOutput; return true; }
        return false;
    }

    static QString titleForKind(ShaderGraphNodeKind kind) {
        for (const auto& def : shaderGraphNodeDefs()) {
            if (def.kind == kind) {
                return QString::fromUtf8(def.title);
            }
        }
        return QStringLiteral("Node");
    }

    void addNode(ShaderGraphNodeKind kind, const QPointF& scenePos) {
        const ShaderGraphNodeDef* def = nullptr;
        for (const auto& candidate : shaderGraphNodeDefs()) {
            if (candidate.kind == kind) {
                def = &candidate;
                break;
            }
        }
        if (!def) {
            return;
        }
        ++nodeCounter_;
        const std::string id = std::string(def->nodeId) + "_" + std::to_string(nodeCounter_);
        ShaderNode::ShaderNodeBase* backend = createBackendNode(kind, id);
        if (!backend) {
            return;
        }
        auto* item = new ShaderGraphNodeItem(backend, kind, QString::fromUtf8(def->title));
        item->setPos(scenePos);
        item->positionChanged = [this]() { refreshEdges(); };
        refreshSummary(item);
        scene_->addItem(item);
        items_.push_back(item);
    }

    void refreshEdges() {
        for (ShaderGraphEdgeItem* edge : edges_) {
            if (edge) {
                edge->refreshPath();
            }
        }
        if (pending_.preview && pending_.fromItem) {
            // Preview path is refreshed on mouse move; nothing to do here.
        }
    }

    void removeEdge(ShaderGraphEdgeItem* edge) {
        if (!edge) {
            return;
        }
        graph_.unlink(edge->fromPin, edge->toPin);
        edges_.erase(std::remove(edges_.begin(), edges_.end(), edge), edges_.end());
        scene_->removeItem(edge);
        delete edge;
    }

    void deleteNodeItem(ShaderGraphNodeItem* item) {
        if (!item) {
            return;
        }
        if (pending_.fromItem == item) {
            cancelPending();
        }
        for (size_t i = 0; i < edges_.size();) {
            ShaderGraphEdgeItem* edge = edges_[i];
            if (edge && (edge->fromItem == item || edge->toItem == item)) {
                graph_.unlink(edge->fromPin, edge->toPin);
                scene_->removeItem(edge);
                delete edge;
                edges_.erase(edges_.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        graph_.removeNode(item->node);
        items_.erase(std::remove(items_.begin(), items_.end(), item), items_.end());
        scene_->removeItem(item);
        delete item;
    }

    void deleteSelected() {
        bool changed = false;
        for (size_t i = 0; i < edges_.size();) {
            ShaderGraphEdgeItem* edge = edges_[i];
            if (edge && edge->isSelected()) {
                graph_.unlink(edge->fromPin, edge->toPin);
                scene_->removeItem(edge);
                delete edge;
                edges_.erase(edges_.begin() + static_cast<ptrdiff_t>(i));
                changed = true;
            } else {
                ++i;
            }
        }
        std::vector<ShaderGraphNodeItem*> doomed;
        for (ShaderGraphNodeItem* item : items_) {
            if (item && item->isSelected()) {
                doomed.push_back(item);
            }
        }
        for (ShaderGraphNodeItem* item : doomed) {
            deleteNodeItem(item);
            changed = true;
        }
        if (changed) {
            setStatus(QStringLiteral("Edited. Apply to rebuild the material."));
        }
    }

    void cancelPending() {
        if (pending_.preview) {
            scene_->removeItem(pending_.preview);
            delete pending_.preview;
            pending_.preview = nullptr;
        }
        pending_.active = false;
        pending_.fromItem = nullptr;
        pending_.fromIndex = -1;
    }

    bool handlePortPress(const QPointF& scenePos) {
        QGraphicsItem* root = scene_->itemAt(scenePos, view_->transform());
        ShaderGraphNodeItem* nodeItem = nullptr;
        while (root) {
            nodeItem = dynamic_cast<ShaderGraphNodeItem*>(root);
            if (nodeItem) {
                break;
            }
            root = root->parentItem();
        }
        if (!nodeItem) {
            if (pending_.active) {
                cancelPending();
                return true;
            }
            return false;
        }
        const auto hit = nodeItem->portAt(scenePos);
        if (hit.second < 0) {
            return false;
        }
        if (!hit.first) {
            cancelPending();
            pending_.active = true;
            pending_.fromItem = nodeItem;
            pending_.fromIndex = hit.second;
            pending_.preview = new QGraphicsPathItem();
            pending_.preview->setPen(QPen(QColor(180, 220, 255), 2.0f));
            scene_->addItem(pending_.preview);
            return true;
        }
        if (!pending_.active || !pending_.fromItem) {
            return false;
        }
        tryCompleteLink(nodeItem, hit.second);
        return true;
    }

    void handleMouseMove(const QPointF& scenePos) {
        if (!pending_.active || !pending_.preview || !pending_.fromItem) {
            return;
        }
        const QPointF p0 = pending_.fromItem->outputPortPos(pending_.fromIndex);
        const double dx = std::max(24.0, std::abs(scenePos.x() - p0.x()) * 0.5);
        QPainterPath path(p0);
        path.cubicTo(p0 + QPointF(dx, 0), scenePos - QPointF(dx, 0), scenePos);
        pending_.preview->setPath(path);
    }

    void tryCompleteLink(ShaderGraphNodeItem* toItem, int toIndex) {
        ShaderGraphNodeItem* fromItem = pending_.fromItem;
        const int fromIndex = pending_.fromIndex;
        cancelPending();
        if (!fromItem || !toItem || fromIndex < 0 || toIndex < 0) {
            return;
        }
        if (fromIndex >= fromItem->outputCount() || toIndex >= toItem->inputCount()) {
            return;
        }
        ShaderNode::Pin* fromPin = fromItem->node->outputs[static_cast<size_t>(fromIndex)].get();
        ShaderNode::Pin* toPin = toItem->node->inputs[static_cast<size_t>(toIndex)].get();
        if (!fromPin || !toPin) {
            return;
        }
        if (fromPin->type != toPin->type) {
            setStatus(QStringLiteral("Incompatible sockets: %1 -> %2.")
                          .arg(shaderPinTypeName(fromPin->type), shaderPinTypeName(toPin->type)));
            return;
        }
        if (fromItem == toItem) {
            setStatus(QStringLiteral("Self-links are rejected."));
            return;
        }
        graph_.disconnectInput(toPin);
        for (size_t i = 0; i < edges_.size();) {
            ShaderGraphEdgeItem* edge = edges_[i];
            if (edge && edge->toPin == toPin) {
                scene_->removeItem(edge);
                delete edge;
                edges_.erase(edges_.begin() + static_cast<ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        graph_.link(fromPin, toPin);
        auto* edge = new ShaderGraphEdgeItem();
        edge->fromPin = fromPin;
        edge->toPin = toPin;
        edge->fromItem = fromItem;
        edge->toItem = toItem;
        edge->fromIndex = fromIndex;
        edge->toIndex = toIndex;
        edge->setPen(QPen(QColor(150, 200, 240), 2.0f));
        edge->setFlag(QGraphicsItem::ItemIsSelectable);
        scene_->addItem(edge);
        edge->refreshPath();
        edges_.push_back(edge);
    }

    void showContextMenu(const QPoint& globalPos, const QPointF& scenePos) {
        QGraphicsItem* root = scene_->itemAt(scenePos, view_->transform());
        ShaderGraphNodeItem* nodeItem = nullptr;
        while (root) {
            nodeItem = dynamic_cast<ShaderGraphNodeItem*>(root);
            if (nodeItem) {
                break;
            }
            root = root->parentItem();
        }
        if (nodeItem) {
            showNodeMenu(nodeItem, globalPos);
            return;
        }
        QMenu menu;
        QMenu* addMenu = menu.addMenu(QStringLiteral("Add Node"));
        struct PendingAdd { const ShaderGraphNodeDef* def; QAction* action; };
        std::vector<PendingAdd> adds;
        for (const auto& def : shaderGraphNodeDefs()) {
            QAction* action = addMenu->addAction(QString::fromUtf8(def.menuLabel));
            adds.push_back(PendingAdd{&def, action});
        }
        menu.addSeparator();
        QAction* applyAction = menu.addAction(QStringLiteral("Apply to Selected 3D Layer"));
        QAction* loadAction = menu.addAction(QStringLiteral("Load Selected Layer Graph"));
        QAction* clearMatAction = menu.addAction(QStringLiteral("Clear Selected Layer Graph"));
        QAction* previewAction = menu.addAction(QStringLiteral("Preview HLSL..."));
        menu.addSeparator();
        QAction* copyAction = menu.addAction(QStringLiteral("Copy Graph JSON"));
        QAction* pasteAction = menu.addAction(QStringLiteral("Paste Graph JSON"));
        menu.addSeparator();
        QAction* clearAction = menu.addAction(QStringLiteral("Clear Graph"));
        QAction* chosen = menu.exec(globalPos);
        if (!chosen) {
            return;
        }
        for (const PendingAdd& add : adds) {
            if (chosen == add.action) {
                addNode(add.def->kind, scenePos);
                setStatus(QStringLiteral("Added %1.").arg(QString::fromUtf8(add.def->title)));
                return;
            }
        }
        if (chosen == applyAction) {
            applyGraph();
        } else if (chosen == loadAction) {
            loadSelectedLayerGraph();
        } else if (chosen == clearMatAction) {
            clearMaterial();
        } else if (chosen == previewAction) {
            previewHlsl();
        } else if (chosen == copyAction) {
            copyGraphJson();
        } else if (chosen == pasteAction) {
            pasteGraphJson();
        } else if (chosen == clearAction) {
            clearGraph();
        }
    }

    std::vector<ShaderNode::MaterialGraphLayout> exportLayout() const {
        std::vector<ShaderNode::MaterialGraphLayout> layout;
        for (ShaderGraphNodeItem* item : items_) {
            if (item && item->node) {
                ShaderNode::MaterialGraphLayout placed;
                placed.nodeId = item->node->id;
                placed.x = static_cast<float>(item->pos().x());
                placed.y = static_cast<float>(item->pos().y());
                layout.push_back(placed);
            }
        }
        return layout;
    }

    void copyGraphJson() {
        if (!owner_) {
            return;
        }
        QClipboard* clipboard = QApplication::clipboard();
        if (!clipboard) {
            setStatus(QStringLiteral("No clipboard available."));
            return;
        }
        clipboard->setText(QString::fromStdString(graph_.toJson(exportLayout())));
        setStatus(QStringLiteral("Copied %1 node(s) as JSON.").arg(items_.size()));
    }

    void pasteGraphJson() {
        if (!owner_) {
            return;
        }
        QClipboard* clipboard = QApplication::clipboard();
        if (!clipboard) {
            setStatus(QStringLiteral("No clipboard available."));
            return;
        }
        const std::string json = clipboard->text().toStdString();
        if (json.empty()) {
            setStatus(QStringLiteral("Clipboard is empty."));
            return;
        }
        const ShaderNode::MaterialGraphLoadResult loaded = graph_.fromJson(json);
        if (!loaded.ok) {
            setStatus(QString::fromStdString(loaded.error));
            return;
        }
        rebuildFromBackend(loaded.layout);
        const QString warning = QString::fromStdString(loaded.error);
        setStatus(warning.isEmpty() ? QStringLiteral("Pasted graph JSON.")
                                    : QStringLiteral("Pasted graph JSON. ") + warning);
    }

    void rebuildFromBackend(const std::vector<ShaderNode::MaterialGraphLayout>& layout) {
        cancelPending();
        for (ShaderGraphEdgeItem* edge : edges_) {
            if (edge) {
                scene_->removeItem(edge);
                delete edge;
            }
        }
        edges_.clear();
        for (ShaderGraphNodeItem* item : items_) {
            if (item) {
                scene_->removeItem(item);
                delete item;
            }
        }
        items_.clear();
        int cascade = 0;
        for (const auto& backend : graph_.nodes) {
            if (!backend) {
                continue;
            }
            ShaderGraphNodeKind kind = ShaderGraphNodeKind::Value;
            if (!kindForBackendNode(backend.get(), kind)) {
                continue;
            }
            auto* item = new ShaderGraphNodeItem(backend.get(), kind, titleForKind(kind));
            QPointF pos(cascade * 24.0, cascade * 24.0);
            for (const auto& placed : layout) {
                if (placed.nodeId == backend->id) {
                    pos = QPointF(placed.x, placed.y);
                    break;
                }
            }
            item->setPos(pos);
            item->positionChanged = [this]() { refreshEdges(); };
            refreshSummary(item);
            scene_->addItem(item);
            items_.push_back(item);
            ++cascade;
        }
        for (const auto& link : graph_.links) {
            if (!link || !link->fromPin || !link->toPin) {
                continue;
            }
            ShaderGraphNodeItem* fromItem = itemForNode(link->fromPin->owner);
            ShaderGraphNodeItem* toItem = itemForNode(link->toPin->owner);
            if (!fromItem || !toItem) {
                continue;
            }
            int fromIndex = -1;
            int toIndex = -1;
            for (size_t i = 0; i < fromItem->node->outputs.size(); ++i) {
                if (fromItem->node->outputs[i].get() == link->fromPin) {
                    fromIndex = static_cast<int>(i);
                    break;
                }
            }
            for (size_t i = 0; i < toItem->node->inputs.size(); ++i) {
                if (toItem->node->inputs[i].get() == link->toPin) {
                    toIndex = static_cast<int>(i);
                    break;
                }
            }
            if (fromIndex < 0 || toIndex < 0) {
                continue;
            }
            auto* edge = new ShaderGraphEdgeItem();
            edge->fromPin = link->fromPin;
            edge->toPin = link->toPin;
            edge->fromItem = fromItem;
            edge->toItem = toItem;
            edge->fromIndex = fromIndex;
            edge->toIndex = toIndex;
            edge->setPen(QPen(QColor(150, 200, 240), 2.0f));
            edge->setFlag(QGraphicsItem::ItemIsSelectable);
            scene_->addItem(edge);
            edge->refreshPath();
            edges_.push_back(edge);
        }
    }

    void showNodeMenu(ShaderGraphNodeItem* item, const QPoint& globalPos) {
        if (!item) {
            return;
        }
        QMenu menu;
        QAction* editAction = nullptr;
        if (nodeHasEditableParams(item->kind)) {
            editAction = menu.addAction(QStringLiteral("Edit Parameters..."));
        }
        if (item->kind == ShaderGraphNodeKind::ColorRamp) {
            menu.addSeparator();
        }
        QAction* addStopAction = nullptr;
        QAction* removeStopAction = nullptr;
        if (item->kind == ShaderGraphNodeKind::ColorRamp) {
            addStopAction = menu.addAction(QStringLiteral("Add Ramp Stop..."));
            removeStopAction = menu.addAction(QStringLiteral("Remove Last Ramp Stop"));
        }
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(QStringLiteral("Delete Node"));
        QAction* chosen = menu.exec(globalPos);
        if (!chosen) {
            return;
        }
        if (editAction && chosen == editAction) {
            editNodeParameters(item);
        } else if (addStopAction && chosen == addStopAction) {
            editRampAddStop(item);
        } else if (removeStopAction && chosen == removeStopAction) {
            editRampRemoveStop(item);
        } else if (chosen == deleteAction) {
            deleteNodeItem(item);
            setStatus(QStringLiteral("Deleted. Apply to rebuild the material."));
        }
    }

    static bool nodeHasEditableParams(ShaderGraphNodeKind kind) {
        switch (kind) {
            case ShaderGraphNodeKind::Value:
            case ShaderGraphNodeKind::RGB:
            case ShaderGraphNodeKind::Math:
            case ShaderGraphNodeKind::MixRGB:
            case ShaderGraphNodeKind::ColorRamp:
            case ShaderGraphNodeKind::ImageTexture:
            case ShaderGraphNodeKind::MapRange:
                return true;
            default:
                return false;
        }
    }

    void refreshSummary(ShaderGraphNodeItem* item) {
        if (!item || !item->node) {
            return;
        }
        using namespace ShaderNode;
        QString text;
        switch (item->kind) {
            case ShaderGraphNodeKind::Value: {
                const auto* n = static_cast<const ValueNode*>(item->node);
                text = QStringLiteral("v=%1").arg(n->value, 0, 'f', 3);
                break;
            }
            case ShaderGraphNodeKind::RGB: {
                const auto* n = static_cast<const RGBNode*>(item->node);
                text = QStringLiteral("%1,%2,%3,%4").arg(n->r, 0, 'f', 2).arg(n->g, 0, 'f', 2).arg(n->b, 0, 'f', 2).arg(n->a, 0, 'f', 2);
                break;
            }
            case ShaderGraphNodeKind::Math: {
                const auto* n = static_cast<const MathNode*>(item->node);
                text = mathOpName(n->op) + (n->clampResult ? QStringLiteral(" clamp") : QString());
                break;
            }
            case ShaderGraphNodeKind::MixRGB: {
                const auto* n = static_cast<const MixRGBNode*>(item->node);
                text = mixBlendName(n->blend);
                break;
            }
            case ShaderGraphNodeKind::ColorRamp: {
                const auto* n = static_cast<const ColorRampNode*>(item->node);
                text = QStringLiteral("%1 stops %2").arg(n->stops.size()).arg(rampInterpName(n->interp));
                break;
            }
            case ShaderGraphNodeKind::ImageTexture: {
                const auto* n = static_cast<const ImageTextureNode*>(item->node);
                text = textureSlotName(n->slot);
                break;
            }
            case ShaderGraphNodeKind::MapRange: {
                const auto* n = static_cast<const MapRangeNode*>(item->node);
                text = mapRangeInterpName(n->interp) + (n->clampRange ? QStringLiteral(" clamp") : QString());
                break;
            }
            default:
                break;
        }
        item->summary = text;
        item->update();
    }

    static QString mathOpName(ShaderNode::MathNode::Op op) {
        using Op = ShaderNode::MathNode::Op;
        switch (op) {
            case Op::Add: return QStringLiteral("Add");
            case Op::Subtract: return QStringLiteral("Subtract");
            case Op::Multiply: return QStringLiteral("Multiply");
            case Op::Divide: return QStringLiteral("Divide");
            case Op::Mix: return QStringLiteral("Mix");
            case Op::Sine: return QStringLiteral("Sine");
            case Op::Cosine: return QStringLiteral("Cosine");
            case Op::Power: return QStringLiteral("Power");
            case Op::Minimum: return QStringLiteral("Minimum");
            case Op::Maximum: return QStringLiteral("Maximum");
            case Op::LessThan: return QStringLiteral("Less Than");
            case Op::GreaterThan: return QStringLiteral("Greater Than");
            case Op::Modulo: return QStringLiteral("Modulo");
            case Op::Absolute: return QStringLiteral("Absolute");
            case Op::Floor: return QStringLiteral("Floor");
            case Op::Ceil: return QStringLiteral("Ceil");
            case Op::Fract: return QStringLiteral("Fract");
        }
        return QStringLiteral("Add");
    }

    static QString mixBlendName(ShaderNode::MixRGBNode::Blend blend) {
        using Blend = ShaderNode::MixRGBNode::Blend;
        switch (blend) {
            case Blend::Mix: return QStringLiteral("Mix");
            case Blend::Add: return QStringLiteral("Add");
            case Blend::Multiply: return QStringLiteral("Multiply");
            case Blend::Screen: return QStringLiteral("Screen");
            case Blend::Overlay: return QStringLiteral("Overlay");
            case Blend::Darken: return QStringLiteral("Darken");
            case Blend::Lighten: return QStringLiteral("Lighten");
            case Blend::Difference: return QStringLiteral("Difference");
        }
        return QStringLiteral("Mix");
    }

    static QString rampInterpName(ShaderNode::ColorRampNode::Interp interp) {
        using Interp = ShaderNode::ColorRampNode::Interp;
        switch (interp) {
            case Interp::Linear: return QStringLiteral("Linear");
            case Interp::Constant: return QStringLiteral("Constant");
            case Interp::Ease: return QStringLiteral("Ease");
        }
        return QStringLiteral("Linear");
    }

    static QString mapRangeInterpName(ShaderNode::MapRangeNode::Interp interp) {
        using Interp = ShaderNode::MapRangeNode::Interp;
        switch (interp) {
            case Interp::Linear: return QStringLiteral("Linear");
            case Interp::Stepped: return QStringLiteral("Stepped");
            case Interp::SmoothStep: return QStringLiteral("Smooth Step");
            case Interp::SmootherStep: return QStringLiteral("Smoother Step");
        }
        return QStringLiteral("Linear");
    }

    static QString textureSlotName(ShaderNode::ImageTextureNode::Slot slot) {
        using Slot = ShaderNode::ImageTextureNode::Slot;
        switch (slot) {
            case Slot::BaseColor: return QStringLiteral("BaseColor");
            case Slot::Opacity: return QStringLiteral("Opacity");
            case Slot::Emission: return QStringLiteral("Emission");
            case Slot::MetallicRoughness: return QStringLiteral("MetallicRoughness");
            case Slot::Normal: return QStringLiteral("Normal");
            case Slot::Occlusion: return QStringLiteral("Occlusion");
        }
        return QStringLiteral("BaseColor");
    }

    static bool askYesNo(QWidget* parent, const QString& title, const QString& label, bool current) {
        const QStringList options = {QStringLiteral("No"), QStringLiteral("Yes")};
        bool ok = false;
        const QString chosen = QInputDialog::getItem(parent, title, label, options,
                                                     current ? 1 : 0, false, &ok);
        return ok && chosen == QStringLiteral("Yes");
    }

    void editNodeParameters(ShaderGraphNodeItem* item) {
        if (!item || !item->node || !owner_) {
            return;
        }
        using namespace ShaderNode;
        bool ok = false;
        switch (item->kind) {
            case ShaderGraphNodeKind::Value: {
                auto* n = static_cast<ValueNode*>(item->node);
                const double v = QInputDialog::getDouble(owner_, QStringLiteral("Value"),
                                                         QStringLiteral("Value:"), n->value,
                                                         -100000.0, 100000.0, 3, &ok);
                if (ok) {
                    n->value = static_cast<float>(v);
                }
                break;
            }
            case ShaderGraphNodeKind::RGB: {
                auto* n = static_cast<RGBNode*>(item->node);
                const QString current = QStringLiteral("%1 %2 %3 %4").arg(n->r).arg(n->g).arg(n->b).arg(n->a);
                const QString text = QInputDialog::getText(owner_, QStringLiteral("RGB"),
                                                           QStringLiteral("r g b a (0-1):"), QLineEdit::Normal,
                                                           current, &ok);
                std::vector<float> values;
                if (ok && parseFloatList(text, values, 4)) {
                    n->r = values[0];
                    n->g = values[1];
                    n->b = values[2];
                    n->a = values[3];
                } else if (ok) {
                    setStatus(QStringLiteral("RGB needs 4 numbers."));
                    return;
                }
                break;
            }
            case ShaderGraphNodeKind::Math: {
                auto* n = static_cast<MathNode*>(item->node);
                QStringList ops;
                for (int i = 0; i <= static_cast<int>(MathNode::Op::Fract); ++i) {
                    ops.push_back(mathOpName(static_cast<MathNode::Op>(i)));
                }
                int current = static_cast<int>(n->op);
                const QString chosen = QInputDialog::getItem(owner_, QStringLiteral("Math"),
                                                             QStringLiteral("Operation:"), ops, current,
                                                             false, &ok);
                if (ok) {
                    const int index = ops.indexOf(chosen);
                    if (index >= 0) {
                        n->op = static_cast<MathNode::Op>(index);
                    }
                }
                if (ok) {
                    n->clampResult = askYesNo(owner_, QStringLiteral("Math"),
                                              QStringLiteral("Clamp result?"), n->clampResult);
                }
                break;
            }
            case ShaderGraphNodeKind::MixRGB: {
                auto* n = static_cast<MixRGBNode*>(item->node);
                QStringList blends;
                for (int i = 0; i <= static_cast<int>(MixRGBNode::Blend::Difference); ++i) {
                    blends.push_back(mixBlendName(static_cast<MixRGBNode::Blend>(i)));
                }
                const QString chosen = QInputDialog::getItem(owner_, QStringLiteral("Mix"),
                                                             QStringLiteral("Blend type:"),
                                                             blends, static_cast<int>(n->blend),
                                                             false, &ok);
                if (ok) {
                    const int index = blends.indexOf(chosen);
                    if (index >= 0) {
                        n->blend = static_cast<MixRGBNode::Blend>(index);
                    }
                }
                if (ok) {
                    n->clampFactor = askYesNo(owner_, QStringLiteral("Mix"),
                                              QStringLiteral("Clamp factor?"), n->clampFactor);
                }
                if (ok) {
                    n->clampResult = askYesNo(owner_, QStringLiteral("Mix"),
                                              QStringLiteral("Clamp result?"), n->clampResult);
                }
                break;
            }
            case ShaderGraphNodeKind::ColorRamp: {
                auto* n = static_cast<ColorRampNode*>(item->node);
                const QStringList interps = {QStringLiteral("Linear"), QStringLiteral("Constant"),
                                             QStringLiteral("Ease")};
                const QString chosen = QInputDialog::getItem(owner_, QStringLiteral("Color Ramp"),
                                                             QStringLiteral("Interpolation:"),
                                                             interps, static_cast<int>(n->interp),
                                                             false, &ok);
                if (ok) {
                    const int index = interps.indexOf(chosen);
                    if (index >= 0) {
                        n->interp = static_cast<ColorRampNode::Interp>(index);
                    }
                }
                break;
            }
            case ShaderGraphNodeKind::ImageTexture: {
                auto* n = static_cast<ImageTextureNode*>(item->node);
                const QStringList textureSlots = {QStringLiteral("BaseColor"), QStringLiteral("Opacity"),
                                           QStringLiteral("Emission"),
                                           QStringLiteral("MetallicRoughness"),
                                           QStringLiteral("Normal"), QStringLiteral("Occlusion")};
                const QString chosen = QInputDialog::getItem(owner_, QStringLiteral("Image Texture"),
                                                             QStringLiteral("Texture slot:"),
                                                             textureSlots, static_cast<int>(n->slot),
                                                             false, &ok);
                if (ok) {
                    const int index = textureSlots.indexOf(chosen);
                    if (index >= 0) {
                        n->slot = static_cast<ImageTextureNode::Slot>(index);
                    }
                }
                break;
            }
            case ShaderGraphNodeKind::MapRange: {
                auto* n = static_cast<MapRangeNode*>(item->node);
                const QStringList interps = {QStringLiteral("Linear"), QStringLiteral("Stepped"),
                                             QStringLiteral("Smooth Step"),
                                             QStringLiteral("Smoother Step")};
                const QString chosen = QInputDialog::getItem(owner_, QStringLiteral("Map Range"),
                                                             QStringLiteral("Interpolation:"),
                                                             interps, static_cast<int>(n->interp),
                                                             false, &ok);
                if (ok) {
                    const int index = interps.indexOf(chosen);
                    if (index >= 0) {
                        n->interp = static_cast<MapRangeNode::Interp>(index);
                    }
                }
                if (ok) {
                    n->clampRange = askYesNo(owner_, QStringLiteral("Map Range"),
                                             QStringLiteral("Clamp result?"), n->clampRange);
                }
                break;
            }
            default:
                return;
        }
        refreshSummary(item);
        setStatus(QStringLiteral("Edited. Apply to rebuild the material."));
    }

    void editRampAddStop(ShaderGraphNodeItem* item) {
        auto* n = static_cast<ShaderNode::ColorRampNode*>(item->node);
        if (!n || !owner_) {
            return;
        }
        bool ok = false;
        const double pos = QInputDialog::getDouble(owner_, QStringLiteral("Add Ramp Stop"),
                                                   QStringLiteral("Position (0-1):"), 0.5,
                                                   0.0, 1.0, 3, &ok);
        if (!ok) {
            return;
        }
        const QString text = QInputDialog::getText(owner_, QStringLiteral("Add Ramp Stop"),
                                                   QStringLiteral("r g b a (0-1):"), QLineEdit::Normal,
                                                   QStringLiteral("1 1 1 1"), &ok);
        std::vector<float> values;
        if (!ok || !parseFloatList(text, values, 4)) {
            return;
        }
        ShaderNode::ColorRampNode::Stop stop;
        stop.pos = static_cast<float>(pos);
        stop.r = values[0];
        stop.g = values[1];
        stop.b = values[2];
        stop.a = values[3];
        n->stops.push_back(stop);
        refreshSummary(item);
        setStatus(QStringLiteral("Edited. Apply to rebuild the material."));
    }

    void editRampRemoveStop(ShaderGraphNodeItem* item) {
        auto* n = static_cast<ShaderNode::ColorRampNode*>(item->node);
        if (!n) {
            return;
        }
        if (n->stops.size() <= 1) {
            setStatus(QStringLiteral("A ramp needs at least one stop."));
            return;
        }
        n->stops.pop_back();
        refreshSummary(item);
        setStatus(QStringLiteral("Edited. Apply to rebuild the material."));
    }

    CompositionRenderController* renderController() const {
        if (!owner_) {
            return nullptr;
        }
        ArtifactCompositionEditor* editor = findActiveCompositionEditor(owner_);
        if (!editor) {
            return nullptr;
        }
        return editor->renderController();
    }

    void applyGraph() {
        CompositionRenderController* controller = renderController();
        if (!controller) {
            setStatus(QStringLiteral("No composition editor is open."));
            return;
        }
        const std::string json = graph_.toJson(exportLayout());
        if (!controller->setMaterialGraphJsonOnSelectedLayer(json)) {
            setStatus(QStringLiteral("Select a 3D model layer first."));
            return;
        }
        const ShaderNode::MaterialGraphResult probe = graph_.compileMaterialGraph();
        if (!probe.ok) {
            setStatus(QString::fromStdString(probe.error) +
                      QStringLiteral(" (magenta fallback applies on next frame)"));
            return;
        }
        const QString warning = QString::fromStdString(probe.error);
        setStatus(QStringLiteral("Stored on the 3D layer [%1], applies on next frame%2")
                      .arg(QString::fromStdString(probe.hashHex))
                      .arg(warning.isEmpty() ? QString() : QStringLiteral(" ") + warning));
    }

    void loadSelectedLayerGraph() {
        CompositionRenderController* controller = renderController();
        if (!controller) {
            setStatus(QStringLiteral("No composition editor is open."));
            return;
        }
        const std::string json = controller->materialGraphJsonOfSelectedLayer();
        if (json.empty()) {
            setStatus(QStringLiteral("Selected layer has no stored graph."));
            return;
        }
        const ShaderNode::MaterialGraphLoadResult loaded = graph_.fromJson(json);
        if (!loaded.ok) {
            setStatus(QString::fromStdString(loaded.error));
            return;
        }
        rebuildFromBackend(loaded.layout);
        setStatus(QStringLiteral("Loaded stored graph."));
    }

    void clearMaterial() {        CompositionRenderController* controller = renderController();
        if (!controller) {
            setStatus(QStringLiteral("No composition editor is open."));
            return;
        }
        setStatus(controller->clearMaterialGraphOnSelectedLayer()
                      ? QStringLiteral("Cleared material graph on the selected layer.")
                      : QStringLiteral("No 3D layer selected or no graph stored."));
    }

    void previewHlsl() {
        if (!owner_) {
            return;
        }
        const ShaderNode::MaterialGraphResult result = graph_.compileMaterialGraph();
        QDialog dialog(owner_);
        dialog.setWindowTitle(QStringLiteral("Material Graph HLSL"));
        dialog.resize(720, 520);
        auto* layout = new QVBoxLayout(&dialog);
        auto* view = new QPlainTextEdit(&dialog);
        view->setReadOnly(true);
        QPalette palette = view->palette();
        palette.setColor(QPalette::Base, QColor(30, 30, 32));
        palette.setColor(QPalette::Text, QColor(210, 210, 210));
        view->setPalette(palette);
        QString text = QString::fromStdString(result.helperHlsl) +
                       QString::fromStdString(result.blockHlsl);
        if (!result.ok) {
            text = QStringLiteral("// ") + QString::fromStdString(result.error) + QStringLiteral("\n") + text;
        }
        view->setPlainText(text);
        layout->addWidget(view);
        dialog.exec();
    }

    void clearGraph() {
        cancelPending();
        for (ShaderGraphEdgeItem* edge : edges_) {
            if (edge) {
                scene_->removeItem(edge);
                delete edge;
            }
        }
        edges_.clear();
        for (ShaderGraphNodeItem* item : items_) {
            if (item) {
                scene_->removeItem(item);
                delete item;
            }
        }
        items_.clear();
        graph_ = ShaderNode::NodeGraph();
        nodeCounter_ = 0;
        setStatus(QStringLiteral("Empty graph. Right-click to add nodes."));
    }
};

ArtifactShaderGraphWidget::ArtifactShaderGraphWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    impl_->owner_ = this;
    setAccessibleName(QStringLiteral("Shader graph panel"));
    setAccessibleDescription(QStringLiteral("Edit the Blender-style material node graph."));
    impl_->setupUi(this);
}

ArtifactShaderGraphWidget::~ArtifactShaderGraphWidget() {
    delete impl_;
    impl_ = nullptr;
}

QSize ArtifactShaderGraphWidget::sizeHint() const {
    return QSize(360, 480);
}

} // namespace Artifact
