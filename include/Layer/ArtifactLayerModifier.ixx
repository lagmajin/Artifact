module;
#include <compare>
#include <algorithm>
#include <memory>
#include <vector>
#include <utility>

#include <QPointF>
#include <QRectF>
#include <QJsonObject>
#include <QString>
#include <QTransform>

export module Artifact.Layer.Modifier;

export namespace Artifact {

enum class LayerModifierKind {
    Transform,
    Warp,
    Noise,
    Wave,
    Custom
};

class ArtifactLayerModifier {
public:
    ArtifactLayerModifier();
    virtual ~ArtifactLayerModifier();

    QString modifierId() const { return modifierId_; }
    void setModifierId(const QString& id) { modifierId_ = id.trimmed(); }

    QString displayName() const { return displayName_; }
    void setDisplayName(const QString& name) { displayName_ = name.trimmed(); }

    LayerModifierKind kind() const { return kind_; }
    void setKind(LayerModifierKind kind) { kind_ = kind; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    virtual QTransform applyToTransform(
        const QTransform& baseTransform,
        const QRectF& localBounds,
        double frameTime) const = 0;

protected:
    QString modifierId_;
    QString displayName_;
    LayerModifierKind kind_ = LayerModifierKind::Custom;
    bool enabled_ = true;
};

class TransformLayerModifier final : public ArtifactLayerModifier {
public:
    TransformLayerModifier();

    QPointF translation() const { return translation_; }
    void setTranslation(const QPointF& translation) { translation_ = translation; }

    double rotationDegrees() const { return rotationDegrees_; }
    void setRotationDegrees(double rotationDegrees) { rotationDegrees_ = rotationDegrees; }

    QPointF scale() const { return scale_; }
    void setScale(const QPointF& scale) { scale_ = scale; }

    QPointF shear() const { return shear_; }
    void setShear(const QPointF& shear) { shear_ = shear; }

    QPointF pivot() const { return pivot_; }
    void setPivot(const QPointF& pivot) { pivot_ = pivot; }

    bool useBoundsCenter() const { return useBoundsCenter_; }
    void setUseBoundsCenter(bool enabled) { useBoundsCenter_ = enabled; }

    QTransform applyToTransform(
        const QTransform& baseTransform,
        const QRectF& localBounds,
        double frameTime) const override;

private:
    QPointF translation_ = QPointF(0.0, 0.0);
    double rotationDegrees_ = 0.0;
    QPointF scale_ = QPointF(1.0, 1.0);
    QPointF shear_ = QPointF(0.0, 0.0);
    QPointF pivot_ = QPointF(0.0, 0.0);
    bool useBoundsCenter_ = true;
};

class LayerModifierStack {
public:
    void add(std::shared_ptr<ArtifactLayerModifier> modifier);
    void remove(const QString& modifierId);
    void clear();

    std::vector<std::shared_ptr<ArtifactLayerModifier>> modifiers() const;
    std::shared_ptr<ArtifactLayerModifier> modifier(const QString& modifierId) const;
    int count() const;
    bool isEmpty() const;

    QTransform apply(
        const QTransform& baseTransform,
        const QRectF& localBounds,
        double frameTime) const;

private:
    std::vector<std::shared_ptr<ArtifactLayerModifier>> modifiers_;
};

export QJsonObject serializeLayerModifier(const ArtifactLayerModifier& modifier);
export std::shared_ptr<ArtifactLayerModifier> deserializeLayerModifier(const QJsonObject& obj);

inline ArtifactLayerModifier::ArtifactLayerModifier() = default;
inline ArtifactLayerModifier::~ArtifactLayerModifier() = default;

inline TransformLayerModifier::TransformLayerModifier() {
    kind_ = LayerModifierKind::Transform;
    modifierId_ = QStringLiteral("transform-modifier");
    displayName_ = QStringLiteral("Transform Modifier");
}

inline QTransform TransformLayerModifier::applyToTransform(
    const QTransform& baseTransform,
    const QRectF& localBounds,
    double frameTime) const
{
    Q_UNUSED(frameTime);

    if (!enabled_) {
        return baseTransform;
    }

    QPointF pivotPoint = pivot_;
    if (useBoundsCenter_ && localBounds.isValid()) {
        pivotPoint = localBounds.center();
    }

    QTransform delta;
    delta.translate(translation_.x(), translation_.y());
    delta.translate(pivotPoint.x(), pivotPoint.y());
    delta.rotate(rotationDegrees_);
    delta.scale(scale_.x(), scale_.y());
    delta.shear(shear_.x(), shear_.y());
    delta.translate(-pivotPoint.x(), -pivotPoint.y());

    return baseTransform * delta;
}

inline void LayerModifierStack::add(std::shared_ptr<ArtifactLayerModifier> modifier) {
    if (!modifier) {
        return;
    }

    const QString currentId = modifier->modifierId().trimmed();
    if (!currentId.isEmpty()) {
        auto existing = std::find_if(
            modifiers_.begin(), modifiers_.end(),
            [&currentId](const std::shared_ptr<ArtifactLayerModifier>& candidate) {
                return candidate && candidate->modifierId() == currentId;
            });
        if (existing != modifiers_.end()) {
            modifiers_.erase(existing);
        }
    }

    modifiers_.push_back(std::move(modifier));
}

inline void LayerModifierStack::remove(const QString& modifierId) {
    const QString normalized = modifierId.trimmed();
    if (normalized.isEmpty()) {
        return;
    }

    modifiers_.erase(
        std::remove_if(modifiers_.begin(), modifiers_.end(),
            [&normalized](const std::shared_ptr<ArtifactLayerModifier>& candidate) {
                return candidate && candidate->modifierId() == normalized;
            }),
        modifiers_.end());
}

inline void LayerModifierStack::clear() {
    modifiers_.clear();
}

inline std::vector<std::shared_ptr<ArtifactLayerModifier>> LayerModifierStack::modifiers() const {
    return modifiers_;
}

inline std::shared_ptr<ArtifactLayerModifier> LayerModifierStack::modifier(const QString& modifierId) const {
    const QString normalized = modifierId.trimmed();
    if (normalized.isEmpty()) {
        return nullptr;
    }

    for (const auto& modifier : modifiers_) {
        if (modifier && modifier->modifierId() == normalized) {
            return modifier;
        }
    }
    return nullptr;
}

inline int LayerModifierStack::count() const {
    return static_cast<int>(modifiers_.size());
}

inline bool LayerModifierStack::isEmpty() const {
    return modifiers_.empty();
}

inline QTransform LayerModifierStack::apply(
    const QTransform& baseTransform,
    const QRectF& localBounds,
    double frameTime) const
{
    QTransform result = baseTransform;
    for (const auto& modifier : modifiers_) {
        if (!modifier || !modifier->enabled()) {
            continue;
        }
        result = modifier->applyToTransform(result, localBounds, frameTime);
    }
    return result;
}

inline QJsonObject serializeLayerModifier(const ArtifactLayerModifier& modifier) {
    const auto kindToString = [](LayerModifierKind kind) {
        switch (kind) {
        case LayerModifierKind::Transform:
            return QStringLiteral("transform");
        case LayerModifierKind::Warp:
            return QStringLiteral("warp");
        case LayerModifierKind::Noise:
            return QStringLiteral("noise");
        case LayerModifierKind::Wave:
            return QStringLiteral("wave");
        case LayerModifierKind::Custom:
        default:
            return QStringLiteral("custom");
        }
    };
    const auto pointToJson = [](const QPointF& point) {
        QJsonObject obj;
        obj[QStringLiteral("x")] = point.x();
        obj[QStringLiteral("y")] = point.y();
        return obj;
    };

    QJsonObject obj;
    obj[QStringLiteral("id")] = modifier.modifierId();
    obj[QStringLiteral("displayName")] = modifier.displayName();
    obj[QStringLiteral("kind")] = dynamic_cast<const TransformLayerModifier*>(&modifier)
        ? kindToString(LayerModifierKind::Transform)
        : kindToString(LayerModifierKind::Custom);
    obj[QStringLiteral("enabled")] = modifier.enabled();

    if (const auto* transform = dynamic_cast<const TransformLayerModifier*>(&modifier)) {
        auto pointToJson = [](const QPointF& point) {
            QJsonObject pointObj;
            pointObj[QStringLiteral("x")] = point.x();
            pointObj[QStringLiteral("y")] = point.y();
            return pointObj;
        };
        obj[QStringLiteral("translation")] = pointToJson(transform->translation());
        obj[QStringLiteral("rotationDegrees")] = transform->rotationDegrees();
        obj[QStringLiteral("scale")] = pointToJson(transform->scale());
        obj[QStringLiteral("shear")] = pointToJson(transform->shear());
        obj[QStringLiteral("pivot")] = pointToJson(transform->pivot());
        obj[QStringLiteral("useBoundsCenter")] = transform->useBoundsCenter();
    }

    return obj;
}

inline std::shared_ptr<ArtifactLayerModifier> deserializeLayerModifier(const QJsonObject& obj) {
    const QString kindText = obj.value(QStringLiteral("kind")).toString();
    auto kindFromString = [](const QString& kind) {
        const QString normalized = kind.trimmed().toLower();
        if (normalized == QStringLiteral("transform")) {
            return LayerModifierKind::Transform;
        }
        if (normalized == QStringLiteral("warp")) {
            return LayerModifierKind::Warp;
        }
        if (normalized == QStringLiteral("noise")) {
            return LayerModifierKind::Noise;
        }
        if (normalized == QStringLiteral("wave")) {
            return LayerModifierKind::Wave;
        }
        return LayerModifierKind::Custom;
    };

    const LayerModifierKind kind = kindText.isEmpty()
        ? LayerModifierKind::Transform
        : kindFromString(kindText);

    if (kind != LayerModifierKind::Transform) {
        return nullptr;
    }

    auto transformModifier = std::make_shared<TransformLayerModifier>();
    auto& modifier = *transformModifier;

    if (obj.contains(QStringLiteral("id"))) {
        modifier.setModifierId(obj.value(QStringLiteral("id")).toString());
    }
    if (obj.contains(QStringLiteral("displayName"))) {
        modifier.setDisplayName(obj.value(QStringLiteral("displayName")).toString());
    }
    if (obj.contains(QStringLiteral("enabled"))) {
        modifier.setEnabled(obj.value(QStringLiteral("enabled")).toBool(true));
    }
    if (obj.contains(QStringLiteral("translation")) &&
        obj.value(QStringLiteral("translation")).isObject()) {
        const auto pointObj = obj.value(QStringLiteral("translation")).toObject();
        modifier.setTranslation(QPointF(
            pointObj.value(QStringLiteral("x")).toDouble(modifier.translation().x()),
            pointObj.value(QStringLiteral("y")).toDouble(modifier.translation().y())));
    }
    if (obj.contains(QStringLiteral("rotationDegrees"))) {
        modifier.setRotationDegrees(
            obj.value(QStringLiteral("rotationDegrees")).toDouble(modifier.rotationDegrees()));
    }
    if (obj.contains(QStringLiteral("scale")) &&
        obj.value(QStringLiteral("scale")).isObject()) {
        const auto pointObj = obj.value(QStringLiteral("scale")).toObject();
        modifier.setScale(QPointF(
            pointObj.value(QStringLiteral("x")).toDouble(modifier.scale().x()),
            pointObj.value(QStringLiteral("y")).toDouble(modifier.scale().y())));
    }
    if (obj.contains(QStringLiteral("shear")) &&
        obj.value(QStringLiteral("shear")).isObject()) {
        const auto pointObj = obj.value(QStringLiteral("shear")).toObject();
        modifier.setShear(QPointF(
            pointObj.value(QStringLiteral("x")).toDouble(modifier.shear().x()),
            pointObj.value(QStringLiteral("y")).toDouble(modifier.shear().y())));
    }
    if (obj.contains(QStringLiteral("pivot")) &&
        obj.value(QStringLiteral("pivot")).isObject()) {
        const auto pointObj = obj.value(QStringLiteral("pivot")).toObject();
        modifier.setPivot(QPointF(
            pointObj.value(QStringLiteral("x")).toDouble(modifier.pivot().x()),
            pointObj.value(QStringLiteral("y")).toDouble(modifier.pivot().y())));
    }
    if (obj.contains(QStringLiteral("useBoundsCenter"))) {
        modifier.setUseBoundsCenter(obj.value(QStringLiteral("useBoundsCenter")).toBool(true));
    }

    return transformModifier;
}

} // namespace Artifact
