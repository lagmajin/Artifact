module;
#include <QDebug>
#include <wobjectimpl.h>

module Artifact.LOD.Manager;

namespace Artifact {

W_OBJECT_IMPL(LODManager)

LODManager::LODManager(QObject* parent)
    : QObject(parent)
{
}

LODManager::~LODManager()
{
}

LODManager::DetailLevel LODManager::getDetailLevel(float zoom) const
{
    DetailLevel newLevel;
    
    if (zoom < lowThreshold_) {
        newLevel = DetailLevel::Low;
    } else if (zoom < mediumThreshold_) {
        newLevel = DetailLevel::Medium;
    } else {
        newLevel = DetailLevel::High;
    }
    
    // 状態変化を検出してシグナル発火
    if (newLevel != currentLevel_) {
        const_cast<LODManager*>(this)->currentLevel_ = newLevel;
        emit const_cast<LODManager*>(this)->detailLevelChanged(static_cast<int>(currentLevel_), static_cast<int>(newLevel));
    }
    
    return newLevel;
}

void LODManager::setThresholds(float lowThreshold, float mediumThreshold)
{
    // 検証
    if (lowThreshold <= 0.0f || lowThreshold >= 1.0f) {
        qWarning() << "LODManager: lowThreshold must be between 0 and 1";
        return;
    }
    
    if (mediumThreshold <= lowThreshold || mediumThreshold >= 1.0f) {
        qWarning() << "LODManager: mediumThreshold must be between lowThreshold and 1";
        return;
    }
    
    lowThreshold_ = lowThreshold;
    mediumThreshold_ = mediumThreshold;
    
    emit thresholdsChanged();
}

float LODManager::calculateLODFactor(float zoom) const
{
    if (zoom < lowThreshold_) {
        // Low ゾーン：0.0-0.5
        return std::clamp(zoom / lowThreshold_ * 0.5f, 0.0f, 0.5f);
    } else if (zoom < mediumThreshold_) {
        // Medium ゾーン：0.5-0.75
        float t = (zoom - lowThreshold_) / (mediumThreshold_ - lowThreshold_);
        return 0.5f + t * 0.25f;
    } else {
        // High ゾーン：0.75-1.0
        return std::clamp(0.75f + (zoom - mediumThreshold_) * 1.0f, 0.75f, 1.0f);
    }
}

} // namespace Artifact
