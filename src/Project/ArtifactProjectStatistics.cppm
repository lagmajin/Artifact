module;

#pragma warning(push)
#pragma warning(disable: 2382)  // redefinition; different exception specifications (std::ranges)

#include <QString>
#include <QVector>
#include <QMap>
#include <QDebug>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

#pragma warning(pop)

module Artifact.Project.Statistics;




import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Property.Abstract;

namespace Artifact {

// We need a way to access the private container_ or use existing public methods.
// ArtifactProject has projectItems(), we can use that to find compositions.

CompositionStats ArtifactProjectStatistics::collectForComposition(ArtifactAbstractComposition* comp) {
    if (!comp) return {};

    CompositionStats stats;
    stats.name = comp->objectName(); // Or get from project items if needed
    stats.layerCount = comp->layerCount();
    
    auto layers = comp->allLayer();
    for (const auto& layer : layers) {
        if (!layer) continue;
        auto effects = layer->getEffects();
        stats.effectCount += static_cast<int>(effects.size());
    }
    
    // Duration
    auto range = comp->frameRange();
    stats.totalDurationFrames = range.duration();
    
    return stats;
}

ProjectStats ArtifactProjectStatistics::collect(ArtifactProject* project) {
    if (!project) return {};

    ProjectStats stats;
    // stats.projectName = project->projectName(); // Need to check if this exists

    auto items = project->projectItems();
    
    // Simple recursive traversal of project items to find all compositions
    std::function<void(ProjectItem*)> traverse = [&](ProjectItem* item) {
        if (!item) return;
        
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto findRes = project->findComposition(compItem->compositionId);
            if (findRes.success) {
                auto comp = findRes.ptr.lock();
                if (comp) {
                    stats.compositionCount++;
                    auto cStats = collectForComposition(comp.get());
                    cStats.name = compItem->name.toQString(); // Use names from tree
                    stats.totalLayerCount += cStats.layerCount;
                    stats.totalEffectCount += cStats.effectCount;
                    stats.compositionDetails.push_back(cStats);
                    
                    // Count effect usage
                    auto layers = comp->allLayer();
                    for (const auto& layer : layers) {
                        for (const auto& effect : layer->getEffects()) {
                           QString typeName = effect->displayName().toQString();
                           stats.effectUsageMap[typeName]++;
                        }
                    }
                }
            }
        }
        
        for (auto child : item->children) {
            traverse(child);
        }
    };

    for (auto root : items) {
        traverse(root);
    }

    return stats;
}

} // namespace Artifact
