module;
#include <utility>
#include <algorithm>
#include <QDebug>
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

namespace Artifact {

class ArtifactGroupLayer::GroupImpl {
public:
    std::vector<ArtifactAbstractLayerPtr> children;
    bool collapsed = false;

    // Cached temporary render target for offscreen composition
    Diligent::RefCntAutoPtr<Diligent::ITexture> cachedTexture;
    Diligent::Uint32 cachedWidth = 0;
    Diligent::Uint32 cachedHeight = 0;
    Diligent::TEXTURE_FORMAT cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    // Optional per-group texture manager for pooled allocations
    std::unique_ptr<ArtifactCore::TextureManager> textureManager;
};

ArtifactGroupLayer::ArtifactGroupLayer()
    : groupImpl_(std::make_unique<GroupImpl>()) {
    setLayerName("Layer Group");
    // setLayerType(LayerType::Group); // Implicitly via constructor initialization
}

ArtifactGroupLayer::~ArtifactGroupLayer() = default;

bool ArtifactGroupLayer::isGroupLayer() const {
    return true;
}

void ArtifactGroupLayer::setComposition(void *comp) {
    ArtifactAbstractLayer::setComposition(comp);
    for (auto& child : groupImpl_->children) {
        if (child) {
            child->setComposition(comp);
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
        for (auto& child : groupImpl_->children) {
            if (child && child->isVisible()) child->draw(renderer);
        }
        return;
    }

    auto device = renderer->device();
    if (!device) {
        for (auto& child : groupImpl_->children) {
            if (child && child->isVisible()) child->draw(renderer);
        }
        return;
    }

    // Derive size/format from the layer RT texture
    auto* origTex = origLayerRTV->GetTexture();
    if (!origTex) {
        for (auto& child : groupImpl_->children) {
            if (child && child->isVisible()) child->draw(renderer);
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
            for (auto& child : groupImpl_->children) {
                if (child && child->isVisible()) child->draw(renderer);
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
        for (auto& child : groupImpl_->children) {
            if (child && child->isVisible()) child->draw(renderer);
        }
        return;
    }

    // Draw children into temporary RT
    renderer->setOverrideRTV(tempRTV);
    // Use the renderer's viewport clear color but ensure opaque alpha to avoid
    // bilinear edge-bleeding when the temp RT is later sampled and blended.
    const FloatColor oldClear = renderer->getClearColor();
    FloatColor fillColor = oldClear;
    fillColor.setAlpha(1.0f);
    renderer->setClearColor(fillColor);
    renderer->clear();
    for (auto& child : groupImpl_->children) {
        if (child && child->isVisible()) {
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
        cv::Mat maskMat(maskH, maskW, CV_32FC4, cv::Scalar(1.0f, 1.0f, 1.0f, 1.0f));
        const int mCount = maskCount();
        for (int mi = 0; mi < mCount; ++mi) {
            LayerMask m = mask(mi);
            if (!m.isEnabled()) continue;
            m.applyToImage(maskW, maskH, &maskMat);
        }
        QImage maskImage = ArtifactCore::CvUtils::cvMatToQImage(maskMat);
        if (!maskImage.isNull()) {
            renderer->drawMaskedTextureLocal(0.0f, 0.0f, static_cast<float>(desc.Width),
                                             static_cast<float>(desc.Height), tempSRV,
                                             maskImage, 1.0f);
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
    layer->setComposition(composition());
    
    groupImpl_->children.push_back(layer);
    // Invalidate cached RT
    groupImpl_->cachedTexture = nullptr;
    groupImpl_->cachedWidth = 0;
    groupImpl_->cachedHeight = 0;
    groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Q_EMIT changed();
}

void ArtifactGroupLayer::removeChild(const LayerID& id) {
    auto it = std::remove_if(groupImpl_->children.begin(), groupImpl_->children.end(), 
        [&](const auto& l) {
            if (!l || l->id() != id) {
                return false;
            }
            l->clearParent();
            l->setComposition(nullptr);
            return true;
        });
    
    if (it != groupImpl_->children.end()) {
        groupImpl_->children.erase(it, groupImpl_->children.end());
        // Invalidate cached RT
        groupImpl_->cachedTexture = nullptr;
        groupImpl_->cachedWidth = 0;
        groupImpl_->cachedHeight = 0;
        groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
        Q_EMIT changed();
    }
}

void ArtifactGroupLayer::clearChildren() {
    for (auto& child : groupImpl_->children) {
        if (!child) {
            continue;
        }
        child->clearParent();
        child->setComposition(nullptr);
    }
    groupImpl_->children.clear();
    // Invalidate cached RT
    groupImpl_->cachedTexture = nullptr;
    groupImpl_->cachedWidth = 0;
    groupImpl_->cachedHeight = 0;
    groupImpl_->cachedFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Q_EMIT changed();
}

const std::vector<ArtifactAbstractLayerPtr>& ArtifactGroupLayer::children() const {
    return groupImpl_->children;
}

void ArtifactGroupLayer::insertChildAt(int index, ArtifactAbstractLayerPtr layer) {
    if (!layer) return;
    if (layer->id() == this->id()) return;

    layer->setParentById(this->id());
    layer->setComposition(composition());

    if (index < 0) index = 0;
    if (static_cast<size_t>(index) >= groupImpl_->children.size()) {
        groupImpl_->children.push_back(layer);
    } else {
        groupImpl_->children.insert(groupImpl_->children.begin() + index, layer);
    }
    Q_EMIT changed();
}

int ArtifactGroupLayer::childIndex(const LayerID& id) const {
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
    for (const auto& child : groupImpl_->children) {
        if (child) {
            bounds = bounds.united(child->transformedBoundingBox());
        }
    }
    return bounds.isValid() ? bounds : QRectF(0, 0, 100, 100);
}

QJsonObject ArtifactGroupLayer::toJson() const {
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["type"] = static_cast<int>(LayerType::Group);
    obj["childCount"] = static_cast<int>(groupImpl_->children.size());
    obj["collapsed"] = static_cast<bool>(groupImpl_->collapsed);
    
    QJsonArray childrenArr;
    for (const auto& child : groupImpl_->children) {
        if (child) {
            childrenArr.append(child->toJson());
        }
    }
    obj["children"] = childrenArr;
    
    return obj;
}

void ArtifactGroupLayer::fromJsonProperties(const QJsonObject& obj) {
    ArtifactAbstractLayer::fromJsonProperties(obj);
    clearChildren();

    if (obj.contains("collapsed")) {
        setCollapsed(obj["collapsed"].toBool());
    }
    
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

} // namespace Artifact
