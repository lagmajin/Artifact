module;
#include <vector>
#include <QString>
#include <QVariant>
#include <array>

module Artifact.Effect.CornerPin;

import Artifact.Effect.Abstract;
import Property.Abstract;
import Property.Types;
import Tracking.MotionTracker;
import Image.ImageF32x4RGBAWithCache;

namespace Artifact {

class ArtifactCornerPinEffect::Impl {
public:
    ArtifactCore::Point2DValue upperLeft = {0, 0};
    ArtifactCore::Point2DValue upperRight = {100, 0};
    ArtifactCore::Point2DValue lowerLeft = {0, 100};
    ArtifactCore::Point2DValue lowerRight = {100, 100};
};

ArtifactCornerPinEffect::ArtifactCornerPinEffect() 
    : impl_(new Impl()) {
    setDisplayName("Corner Pin");
    setEffectID("builtin.corner_pin");
}

ArtifactCornerPinEffect::~ArtifactCornerPinEffect() {
    delete impl_;
}

std::vector<ArtifactCore::AbstractProperty> ArtifactCornerPinEffect::getProperties() const {
    std::vector<ArtifactCore::AbstractProperty> props;

    auto makePointComponent = [&](const char* name, double value, int priority) {
        ArtifactCore::AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(ArtifactCore::PropertyType::Float);
        prop.setValue(value);
        prop.setDefaultValue(value);
        prop.setMinValue(-100000.0);
        prop.setMaxValue(100000.0);
        prop.setStep(1.0);
        prop.setAnimatable(true);
        prop.setDisplayPriority(priority);
        return prop;
    };

    props.push_back(makePointComponent("Upper Left X", impl_->upperLeft.x, -40));
    props.push_back(makePointComponent("Upper Left Y", impl_->upperLeft.y, -39));
    props.push_back(makePointComponent("Upper Right X", impl_->upperRight.x, -38));
    props.push_back(makePointComponent("Upper Right Y", impl_->upperRight.y, -37));
    props.push_back(makePointComponent("Lower Left X", impl_->lowerLeft.x, -36));
    props.push_back(makePointComponent("Lower Left Y", impl_->lowerLeft.y, -35));
    props.push_back(makePointComponent("Lower Right X", impl_->lowerRight.x, -34));
    props.push_back(makePointComponent("Lower Right Y", impl_->lowerRight.y, -33));

    return props;
}

void ArtifactCornerPinEffect::setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    const double v = value.toDouble();

    if (key == QStringLiteral("Upper Left X")) {
        impl_->upperLeft.x = v;
    } else if (key == QStringLiteral("Upper Left Y")) {
        impl_->upperLeft.y = v;
    } else if (key == QStringLiteral("Upper Right X")) {
        impl_->upperRight.x = v;
    } else if (key == QStringLiteral("Upper Right Y")) {
        impl_->upperRight.y = v;
    } else if (key == QStringLiteral("Lower Left X")) {
        impl_->lowerLeft.x = v;
    } else if (key == QStringLiteral("Lower Left Y")) {
        impl_->lowerLeft.y = v;
    } else if (key == QStringLiteral("Lower Right X")) {
        impl_->lowerRight.x = v;
    } else if (key == QStringLiteral("Lower Right Y")) {
        impl_->lowerRight.y = v;
    }
}

void ArtifactCornerPinEffect::apply(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) {
    // Current implementation: CPU based bilinear warping using Homography
    int w = src.width();
    int h = src.height();
    
    std::vector<QPointF> srcPoints = {
        {0, 0}, {static_cast<double>(w), 0},
        {0, static_cast<double>(h)}, {static_cast<double>(w), static_cast<double>(h)}
    };
    
    std::vector<QPointF> dstPoints = {
        {impl_->upperLeft.x, impl_->upperLeft.y},
        {impl_->upperRight.x, impl_->upperRight.y},
        {impl_->lowerLeft.x, impl_->lowerLeft.y},
        {impl_->lowerRight.x, impl_->lowerRight.y}
    };
    
    auto H = ArtifactCore::MotionTracker::computeHomography(srcPoints, dstPoints);
    
    // Homography inversion for backward mapping
    // (Simplification: In regular effect impl, we'd use a shader or optimized CPU loop)
    // For now, this is a placeholder for the logic.
}

} // namespace Artifact
