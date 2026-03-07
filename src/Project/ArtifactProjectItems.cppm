module;
#include <QString>
#include <QVector>
#include <QColor>
module Artifact.Project.Items;

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



import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

FolderItem* FolderItem::addChildFolder(const UniString& name) {
    auto* folder = new FolderItem();
    folder->name = name;
    folder->parent = this;
    this->children.append(folder);
    return folder;
}

} // namespace Artifact
