module;

export module Artifact.Project.Roles;

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




export namespace Artifact {

// Data roles stored in QStandardItem::data at Qt::UserRole + offset.
// Use this enum to avoid magic numbers throughout the codebase.
export enum class ProjectItemDataRole : int {
    ProjectItemType = 1, // Qt::UserRole + 1 => optional type identifier
    CompositionId = 2,   // Qt::UserRole + 2 => composition id string
    ProjectItemPtr = 3,  // Qt::UserRole + 3 => raw ProjectItem*
    AssetId = 4,         // Qt::UserRole + 4 => deterministic asset id string
};

};
