module;
#include <utility>
#include <algorithm>
#include <QDebug>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QSize>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/TextureView.h>
#include <opencv2/core.hpp>

module Artifact.Layer.Group;

import std;

import Artifact.Layer.Abstract;
import Utils.Id;
import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Color.Float;
import Graphics.Texture.Manager;
import CvUtils;
import Property.Abstract;
import Property.Group;

namespace Artifact {

class ArtifactGroupLayer::GroupImpl {
public:
    std::vector<ArtifactAbstractLayerPtr> children;
    mutable std::vector<ArtifactAbstractLayerPtr> compositionChildrenCache;
    bool collapsed = false;
    GroupOutputMode outputMode = GroupOutputMode::All;
    LayerID activeChildId;

    // Cached temporary render target for offscreen composition
    Diligent::RefCntAutoPtr<Diligent::ITexture> cachedTexture;
    Diligent::RefCntAutoPtr<Diligent::ITexture> cachedShareScratchTexture;
    Diligent::Uint32 cachedWidth = 0;
    Diligent::Uint32 cachedHeight = 0;
    Diligent::TEXTURE_FORMAT cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    qint64 cachedMaskSignature = -1;
    int cachedMaskWidth = 0;
    int cachedMaskHeight = 0;
    QImage cachedMaskImage;
    // Optional per-group texture manager for pooled allocations
    std::unique_ptr<ArtifactCore::TextureManager> textureManager;
};

static qint64 maskSignatureForLayer(const ArtifactAbstractLayer *layer, int width, int height)
{
    if (!layer) {
        return -1;
    }

    qint64 signature = 1469598103934665603LL;
    auto mix = [&signature](qint64 value) {
        signature ^= value;
        signature *= 1099511628211LL;
    };

    mix(width);
    mix(height);
    const int count = layer->maskCount();
    mix(count);
    for (int i = 0; i < count; ++i) {
        const LayerMask mask = layer->mask(i);
        mix(mask.isEnabled() ? 1 : 0);
        const int pathCount = mask.maskPathCount();
        mix(pathCount);
        for (int p = 0; p < pathCount; ++p) {
            const MaskPath path = mask.maskPath(p);
            mix(path.isClosed() ? 1 : 0);
            mix(static_cast<int>(path.mode()));
            mix(path.isInverted() ? 1 : 0);
            mix(static_cast<int>(std::lround(path.opacity() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.feather() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.featherHorizontal() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.featherVertical() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.featherInner() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.featherOuter() * 1000.0f)));
            mix(static_cast<int>(std::lround(path.expansion() * 1000.0f)));
            mix(path.vertexCount());
            mix(qHash(path.name().toQString()));
        }
    }
    return signature;
}

ArtifactGroupLayer::ArtifactGroupLayer()
    : groupImpl_(std::make_unique<GroupImpl>()) {
    setLayerName("Layer Group");
    // setLayerType(LayerType::Group); // Implicitly via constructor initialization
}

ArtifactGroupLayer::~ArtifactGroupLayer() = default;

bool ArtifactGroupLayer::isGroupLayer() const {
    return true;
}

bool ArtifactGroupLayer::hasExclusiveChildSelection() const {
    return groupImpl_->outputMode == GroupOutputMode::Single;
}

LayerID ArtifactGroupLayer::selectedChildIdForEvaluation() const {
    if (groupImpl_->outputMode != GroupOutputMode::Single) {
        return LayerID();
    }
    const auto active = activeChild();
    if (active && active->isVisible()) {
        return active->id();
    }
    for (const auto& child : children()) {
        if (child && child->isVisible()) {
            return child->id();
        }
    }
    return LayerID();
}

float ArtifactGroupLayer::childEvaluationGain(const LayerID& childId) const {
    if (groupImpl_->outputMode != GroupOutputMode::Share || childId.isNil()) {
        return 1.0f;
    }

    const auto renderChildren = childrenForRender();
    const auto child = std::find_if(renderChildren.begin(), renderChildren.end(),
        [&childId](const auto& candidate) {
            return candidate && candidate->id() == childId;
        });
    if (child == renderChildren.end() || renderChildren.empty()) {
        return 0.0f;
    }
    return 1.0f / static_cast<float>(renderChildren.size());
}

std::vector<ArtifactAbstractLayerPtr> ArtifactGroupLayer::childrenForRender() const {
    std::vector<ArtifactAbstractLayerPtr> result;
    std::vector<ArtifactAbstractLayerPtr> compositionChildren;
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        const auto children = composition->childLayersOf(id());
        compositionChildren.assign(children.begin(), children.end());
    }
    const auto& children = compositionChildren.empty()
        ? groupImpl_->children : compositionChildren;
    if (groupImpl_->outputMode != GroupOutputMode::Single) {
        for (const auto& child : children) {
            if (child && child->isVisible()) {
                result.push_back(child);
            }
        }
        return result;
    }

    const auto active = activeChild();
    if (active && active->isVisible()) {
        result.push_back(active);
        return result;
    }
    for (const auto& child : children) {
        if (child && child->isVisible()) {
            result.push_back(child);
            break;
        }
    }
    return result;
}

void ArtifactGroupLayer::setComposition(QObject *comp) {
    ArtifactAbstractLayer::setComposition(comp);
    promoteEmbeddedChildrenToComposition();
}

void ArtifactGroupLayer::setComposition(void *comp) {
    ArtifactAbstractLayer::setComposition(comp);
    promoteEmbeddedChildrenToComposition();
}

void ArtifactGroupLayer::promoteEmbeddedChildrenToComposition() {
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition || groupImpl_->children.empty()) {
        return;
    }

    auto embeddedChildren = std::move(groupImpl_->children);
    groupImpl_->children.clear();
    for (const auto& child : embeddedChildren) {
        if (!child) {
            continue;
        }
        child->setParentById(id());
        if (!composition->containsLayerById(child->id())) {
            composition->appendLayerTop(child);
        }
    }
}

void ArtifactGroupLayer::draw(ArtifactIRenderer* renderer) {
    if (!isVisible() || opacity() <= 0.0f) return;
    if (!renderer) return;

    // Prefer offscreen composite: render children into a temporary RT and blit
    // the result into the provided layer render target. This lets the composition
    // controller treat the group as a single layer (blend mode / opacity / mask).

    auto* origLayerRTV = renderer->layerRenderTargetView();
    if (!origLayerRTV) {
        // Fallback: draw children directly into current RT
        for (const auto& child : childrenForRender()) {
            child->draw(renderer);
        }
        return;
    }

    auto device = renderer->device();
    if (!device) {
        for (const auto& child : childrenForRender()) {
            child->draw(renderer);
        }
        return;
    }

    // Derive size/format from the layer RT texture
    auto* origTex = origLayerRTV->GetTexture();
    if (!origTex) {
        for (const auto& child : childrenForRender()) {
            child->draw(renderer);
        }
        return;
    }

    const auto desc = origTex->GetDesc();

    Diligent::TextureDesc texDesc;
    texDesc.Name        = "GroupLayer.Temp";
    texDesc.Type        = Diligent::RESOURCE_DIM_TEX_2D;
    texDesc.Width       = desc.Width;
    texDesc.Height      = desc.Height;
    texDesc.Format      = desc.Format;
    texDesc.MipLevels   = 1;
    texDesc.ArraySize   = 1;
    texDesc.SampleCount = 1;
    texDesc.Usage       = Diligent::USAGE_DEFAULT;
    texDesc.BindFlags   = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

    // Try to reuse cached texture if it matches size/format
    Diligent::RefCntAutoPtr<Diligent::ITexture> tempTex = groupImpl_->cachedTexture;
    const bool cachedMatch = tempTex &&
        groupImpl_->cachedWidth == desc.Width &&
        groupImpl_->cachedHeight == desc.Height &&
        groupImpl_->cachedFormat == desc.Format;
    if (!cachedMatch) {
        // Try pooled allocation via TextureManager (ArtifactCore). Fall back to device allocation.
        Diligent::RefCntAutoPtr<Diligent::ITexture> newTex;
        if (!groupImpl_->textureManager && device) {
            groupImpl_->textureManager = std::make_unique<ArtifactCore::TextureManager>(device.RawPtr());
        }
        if (groupImpl_->textureManager) {
            QSize size(static_cast<int>(desc.Width), static_cast<int>(desc.Height));
            Diligent::ITexture* raw = groupImpl_->textureManager->createTexture(size, desc.Format, QStringLiteral("GroupLayer.Temp"));
            if (raw) {
                newTex = raw;
            }
        }
        if (!newTex && device) {
            device->CreateTexture(texDesc, nullptr, &newTex);
        }
        if (!newTex) {
            // Allocation failed: fallback to direct draws
            for (const auto& child : childrenForRender()) {
                child->draw(renderer);
            }
            return;
        }
        groupImpl_->cachedTexture = newTex;
        groupImpl_->cachedWidth = desc.Width;
        groupImpl_->cachedHeight = desc.Height;
        groupImpl_->cachedFormat = desc.Format;
        tempTex = newTex;
    }

    auto tempRTV = tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    auto tempSRV = tempTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!tempRTV || !tempSRV) {
        for (const auto& child : childrenForRender()) {
            child->draw(renderer);
        }
        return;
    }

    // Draw children into the group composite. Share mode isolates each child in
    // a scratch RT before adding it at 1/N opacity, keeping the whole group at
    // 100% regardless of child count.
    renderer->setOverrideRTV(tempRTV);
    // Keep the renderer's clear color intact here; forcing alpha to 1.0 breaks
    // transparent group compositing and can leak opaque fill into the temp RT.
    const FloatColor oldClear = renderer->getClearColor();
    renderer->setClearColor(oldClear);
    renderer->clear();
    const auto renderChildren = childrenForRender();
    if (groupImpl_->outputMode == GroupOutputMode::Share && !renderChildren.empty()) {
        auto scratchTex = groupImpl_->cachedShareScratchTexture;
        if (!scratchTex || !cachedMatch) {
            Diligent::RefCntAutoPtr<Diligent::ITexture> newScratch;
            if (device) {
                auto scratchDesc = texDesc;
                scratchDesc.Name = "GroupLayer.ShareScratch";
                device->CreateTexture(scratchDesc, nullptr, &newScratch);
            }
            if (!newScratch) {
                // Preserve compositing correctness if the extra target cannot
                // be allocated; do not silently attenuate partially rendered output.
                for (const auto& child : renderChildren) {
                    child->draw(renderer);
                }
            } else {
                groupImpl_->cachedShareScratchTexture = newScratch;
                scratchTex = newScratch;
            }
        }
        if (scratchTex) {
            auto scratchRTV = scratchTex->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
            auto scratchSRV = scratchTex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            if (scratchRTV && scratchSRV) {
                QMatrix4x4 screenIdentity;
                screenIdentity.setToIdentity();
                const float shareOpacity = 1.0f / static_cast<float>(renderChildren.size());
                for (const auto& child : renderChildren) {
                    renderer->setOverrideRTV(scratchRTV);
                    renderer->clear();
                    child->draw(renderer);
                    renderer->setOverrideRTV(tempRTV);
                    renderer->drawSpriteTransformed(0.0f, 0.0f,
                                                    static_cast<float>(desc.Width),
                                                    static_cast<float>(desc.Height),
                                                    screenIdentity, scratchSRV, shareOpacity);
                }
            } else {
                for (const auto& child : renderChildren) {
                    child->draw(renderer);
                }
            }
        }
    } else {
        for (const auto& child : renderChildren) {
            child->draw(renderer);
        }
    }
    // Restore previous clear color and RTV state
    renderer->setClearColor(oldClear);
    renderer->setOverrideRTV(nullptr);

    // Blit the temporary SRV into the original layer RT so the composition
    // controller's blend step treats the whole group as a single layer.
    renderer->setOverrideRTV(origLayerRTV);
    QMatrix4x4 screenIdentity;
    screenIdentity.setToIdentity();
    // If this group has masks, rasterize them and use the masked blit path.
    if (hasMasks()) {
        const int maskW = static_cast<int>(desc.Width);
        const int maskH = static_cast<int>(desc.Height);
        const qint64 signature = maskSignatureForLayer(this, maskW, maskH);
        if (groupImpl_->cachedMaskImage.isNull() ||
            groupImpl_->cachedMaskWidth != maskW ||
            groupImpl_->cachedMaskHeight != maskH ||
            groupImpl_->cachedMaskSignature != signature) {
            cv::Mat maskMat(maskH, maskW, CV_32FC4, cv::Scalar(1.0f, 1.0f, 1.0f, 1.0f));
            const int mCount = maskCount();
            for (int mi = 0; mi < mCount; ++mi) {
                LayerMask m = mask(mi);
                if (!m.isEnabled()) continue;
                m.applyToImage(maskW, maskH, &maskMat);
            }
            groupImpl_->cachedMaskImage = ArtifactCore::CvUtils::cvMatToQImage(maskMat);
            groupImpl_->cachedMaskWidth = maskW;
            groupImpl_->cachedMaskHeight = maskH;
            groupImpl_->cachedMaskSignature = signature;
        }
        if (!groupImpl_->cachedMaskImage.isNull()) {
            renderer->drawMaskedTextureLocal(0.0f, 0.0f, static_cast<float>(desc.Width),
                                             static_cast<float>(desc.Height), tempSRV,
                                             groupImpl_->cachedMaskImage, 1.0f);
        } else {
            renderer->drawSpriteTransformed(0.0f, 0.0f, static_cast<float>(desc.Width),
                                           static_cast<float>(desc.Height), screenIdentity, tempSRV, 1.0f);
        }
    } else {
        renderer->drawSpriteTransformed(0.0f, 0.0f, static_cast<float>(desc.Width),
                                       static_cast<float>(desc.Height), screenIdentity, tempSRV, 1.0f);
    }
    renderer->setOverrideRTV(nullptr);
}

void ArtifactGroupLayer::addChild(ArtifactAbstractLayerPtr layer) {
    if (!layer) return;
    
    // Prevent self-addition or cyclic
    if (layer->id() == this->id()) return;

    // Set parenting in the core system
    layer->setParentById(this->id());
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        if (!composition->containsLayerById(layer->id())) {
            composition->appendLayerTop(layer);
        }
    } else {
        layer->setComposition(compositionObject());
        groupImpl_->children.push_back(layer);
    }
    // Invalidate cached RT
    groupImpl_->cachedTexture = nullptr;
    groupImpl_->cachedWidth = 0;
    groupImpl_->cachedHeight = 0;
    groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    groupImpl_->cachedMaskImage = QImage{};
    groupImpl_->cachedMaskSignature = -1;
    Q_EMIT changed();
}

void ArtifactGroupLayer::removeChild(const LayerID& id) {
    bool removedCompositionChild = false;
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        const auto child = composition->layerById(id);
        if (child && child->parentLayerId() == this->id()) {
            child->clearParent();
            removedCompositionChild = true;
        }
    }
    auto it = std::remove_if(groupImpl_->children.begin(), groupImpl_->children.end(), 
        [&](const auto& l) {
            if (!l || l->id() != id) {
                return false;
            }
            l->clearParent();
            l->setComposition(static_cast<QObject *>(nullptr));
            return true;
        });
    
    if (it != groupImpl_->children.end()) {
        groupImpl_->children.erase(it, groupImpl_->children.end());
        removedCompositionChild = true;
    }
    if (removedCompositionChild) {
        // Invalidate cached RT
        groupImpl_->cachedTexture = nullptr;
        groupImpl_->cachedWidth = 0;
        groupImpl_->cachedHeight = 0;
        groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
        groupImpl_->cachedMaskImage = QImage{};
        groupImpl_->cachedMaskSignature = -1;
        Q_EMIT changed();
    }
}

void ArtifactGroupLayer::clearChildren() {
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        for (const auto& child : composition->childLayersOf(id())) {
            if (child) {
                child->clearParent();
            }
        }
    }
    for (auto& child : groupImpl_->children) {
        if (!child) {
            continue;
        }
        child->clearParent();
        child->setComposition(static_cast<QObject *>(nullptr));
    }
    groupImpl_->children.clear();
    // Invalidate cached RT
    groupImpl_->cachedTexture = nullptr;
    groupImpl_->cachedWidth = 0;
    groupImpl_->cachedHeight = 0;
    groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    groupImpl_->cachedMaskImage = QImage{};
    groupImpl_->cachedMaskSignature = -1;
    Q_EMIT changed();
}

const std::vector<ArtifactAbstractLayerPtr>& ArtifactGroupLayer::children() const {
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        const auto compositionChildren = composition->childLayersOf(id());
        if (!compositionChildren.empty()) {
            groupImpl_->compositionChildrenCache.assign(
                compositionChildren.begin(), compositionChildren.end());
            return groupImpl_->compositionChildrenCache;
        }
    }
    return groupImpl_->children;
}

void ArtifactGroupLayer::insertChildAt(int index, ArtifactAbstractLayerPtr layer) {
    if (!layer) return;
    if (layer->id() == this->id()) return;

    layer->setParentById(this->id());
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        if (!composition->containsLayerById(layer->id())) {
            composition->appendLayerTop(layer);
        }
    } else {
        layer->setComposition(compositionObject());
        if (index < 0) index = 0;
        if (static_cast<size_t>(index) >= groupImpl_->children.size()) {
            groupImpl_->children.push_back(layer);
        } else {
            groupImpl_->children.insert(groupImpl_->children.begin() + index, layer);
        }
    }
    Q_EMIT changed();
}

int ArtifactGroupLayer::childIndex(const LayerID& id) const {
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        const auto children = composition->childLayersOf(id());
        for (int index = 0; index < children.size(); ++index) {
            const auto& layer = children.at(index);
            if (layer->id() == id) {
                return index;
            }
        }
    }
    for (int i = 0; i < static_cast<int>(groupImpl_->children.size()); ++i) {
        auto& c = groupImpl_->children[static_cast<size_t>(i)];
        if (c && c->id() == id) return i;
    }
    return -1;
}

bool ArtifactGroupLayer::containsChild(const LayerID& id) const {
    return childIndex(id) >= 0;
}

bool ArtifactGroupLayer::isCollapsed() const {
    return groupImpl_->collapsed;
}

bool ArtifactGroupLayer::isMultiplexer() const {
    return groupImpl_->outputMode != GroupOutputMode::All;
}

void ArtifactGroupLayer::setMultiplexer(const bool enabled) {
    setOutputMode(enabled ? GroupOutputMode::Single : GroupOutputMode::All);
}

GroupOutputMode ArtifactGroupLayer::outputMode() const {
    return groupImpl_->outputMode;
}

void ArtifactGroupLayer::setOutputMode(const GroupOutputMode mode) {
    if (groupImpl_->outputMode == mode) return;
    groupImpl_->outputMode = mode;
    groupImpl_->cachedTexture = nullptr;
    Q_EMIT changed();
}

LayerID ArtifactGroupLayer::activeChildId() const {
    return groupImpl_->activeChildId;
}

void ArtifactGroupLayer::setActiveChildId(const LayerID& id) {
    if (!id.isNil() && !containsChild(id)) {
        return;
    }
    if (groupImpl_->activeChildId == id) {
        return;
    }
    groupImpl_->activeChildId = id;
    groupImpl_->cachedTexture = nullptr;
    Q_EMIT changed();
}

ArtifactAbstractLayerPtr ArtifactGroupLayer::activeChild() const {
    if (groupImpl_->activeChildId.isNil()) {
        return nullptr;
    }
    if (auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
        const auto child = composition->layerById(groupImpl_->activeChildId);
        if (child && child->parentLayerId() == id()) {
            return child;
        }
    }
    const auto it = std::find_if(
        groupImpl_->children.begin(), groupImpl_->children.end(),
        [this](const ArtifactAbstractLayerPtr& child) {
            return child && child->id() == groupImpl_->activeChildId;
        });
    return it != groupImpl_->children.end() ? *it : nullptr;
}

void ArtifactGroupLayer::setCollapsed(bool collapsed) {
    if (groupImpl_->collapsed == collapsed) return;
    groupImpl_->collapsed = collapsed;
    // Invalidate cached RT because visual representation changed
    groupImpl_->cachedTexture = nullptr;
    groupImpl_->cachedWidth = 0;
    groupImpl_->cachedHeight = 0;
    groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Q_EMIT changed();
}

QRectF ArtifactGroupLayer::localBounds() const {
    // Group bounds depends on its children's transformed bounds
    QRectF bounds;
    for (const auto& child : children()) {
        if (child) {
            bounds = bounds.united(child->transformedBoundingBox());
        }
    }
    return bounds.isValid() ? bounds : QRectF(0, 0, 100, 100);
}

QJsonObject ArtifactGroupLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Group);
    obj["childCount"] = static_cast<int>(children().size());
    obj["collapsed"] = static_cast<bool>(groupImpl_->collapsed);
    obj["group.outputMode"] = static_cast<int>(groupImpl_->outputMode);
    obj["group.renderPolicy"] = groupImpl_->outputMode == GroupOutputMode::Single
        ? QStringLiteral("multiplex") : QStringLiteral("composite");
    obj["group.activeChildId"] = groupImpl_->activeChildId.toString();
    
    // Composition-owned children are serialized by the composition itself.
    // Keep embedded children only for detached, legacy group payloads.
    if (dynamic_cast<ArtifactAbstractComposition*>(compositionObject()) == nullptr) {
        QJsonArray childrenArr;
        for (const auto& child : groupImpl_->children) {
            if (child) {
                childrenArr.append(child->toJson());
            }
        }
        obj["children"] = childrenArr;
    }
    
    return obj;
}

void ArtifactGroupLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    clearChildren();

    if (obj.contains("collapsed")) {
        setCollapsed(obj["collapsed"].toBool());
    }
    groupImpl_->outputMode = obj.contains("group.outputMode")
        ? static_cast<GroupOutputMode>(std::clamp(obj.value("group.outputMode").toInt(), 0, 2))
        : (obj.value("group.renderPolicy").toString() == QStringLiteral("multiplex")
               ? GroupOutputMode::Single : GroupOutputMode::All);
    groupImpl_->activeChildId = LayerID(obj.value("group.activeChildId").toString());
    
    if (obj.contains("children") && obj["children"].isArray()) {
        QJsonArray childrenArr = obj["children"].toArray();
        for (const auto& v : childrenArr) {
            QJsonObject childObj = v.toObject();
            auto child = ArtifactAbstractLayer::fromJson(childObj);
            if (child) {
                addChild(child);
            }
        }
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactGroupLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup group(QStringLiteral("Group Output"));
    auto multiplex = persistentLayerProperty(
        QStringLiteral("group.multiplex"), ArtifactCore::PropertyType::Boolean,
        isMultiplexer(), -110);
    multiplex->setDisplayLabel(QStringLiteral("Multiplexer"));
    group.addProperty(multiplex);
    auto outputMode = persistentLayerProperty(
        QStringLiteral("group.outputMode"), ArtifactCore::PropertyType::Integer,
        static_cast<int>(groupImpl_->outputMode), -105);
    outputMode->setDisplayLabel(QStringLiteral("Output Mode"));
    outputMode->setTooltip(
        QStringLiteral("0=All, 1=Single, 2=Share"));
    group.addProperty(outputMode);
    auto activeIndex = persistentLayerProperty(
        QStringLiteral("group.activeChildIndex"), ArtifactCore::PropertyType::Integer,
        std::max(0, childIndex(groupImpl_->activeChildId)), -100);
    activeIndex->setDisplayLabel(QStringLiteral("Active Child"));
    activeIndex->setTooltip(
        QStringLiteral("Zero-based child index used by Single output mode."));
    group.addProperty(activeIndex);
    groups.push_back(std::move(group));
    return groups;
}

bool ArtifactGroupLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value) {
    if (propertyPath == QStringLiteral("group.multiplex")) {
        setMultiplexer(value.toBool());
        return true;
    }
    if (propertyPath == QStringLiteral("group.outputMode")) {
        setOutputMode(static_cast<GroupOutputMode>(std::clamp(value.toInt(), 0, 2)));
        return true;
    }
    if (propertyPath == QStringLiteral("group.activeChildIndex")) {
        const int index = value.toInt();
        if (index < 0) {
            return false;
        }
        if (auto* composition =
                dynamic_cast<ArtifactAbstractComposition*>(compositionObject())) {
            const auto children = composition->childLayersOf(id());
            if (index < children.size() && children.at(index)) {
                setActiveChildId(children.at(index)->id());
                return true;
            }
        }
        if (index >= static_cast<int>(groupImpl_->children.size()) ||
            !groupImpl_->children[static_cast<size_t>(index)]) {
            return false;
        }
        setActiveChildId(groupImpl_->children[static_cast<size_t>(index)]->id());
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
