module;

#include <QString>
#include <QVector>
#include <QMap>

export module Artifact.Project.Statistics;

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



import Artifact.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Utils.Id;

export namespace Artifact {

struct CompositionStats {
    QString name;
    int layerCount = 0;
    int effectCount = 0;
    long long totalDurationFrames = 0;
};

struct ProjectStats {
    QString projectName;
    int compositionCount = 0;
    int totalLayerCount = 0;
    int totalEffectCount = 0;
    QMap<QString, int> effectUsageMap; // Effect Type -> Usage Count
    QVector<CompositionStats> compositionDetails;
};

class ArtifactProjectStatistics {
public:
    static ProjectStats collect(ArtifactProject* project);
    static CompositionStats collectForComposition(ArtifactAbstractComposition* comp);
};

} // namespace Artifact
