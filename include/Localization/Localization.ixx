module;
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
export module Localization.Localization;




import Utils.String.UniString;

export namespace ArtifactCore {

using namespace ArtifactCore;

class LocalizationManager {
private:
    class Impl;
    Impl* impl_;
public:
    LocalizationManager();
    ~LocalizationManager();

    // singleton accessor
    static LocalizationManager* instance();

    // Set locale code like "eng" or "jp"
    void setLocale(const UniString& locale);
    UniString locale() const;

    // Translate a key (e.g., "menu.file") into localized string.
    // If not found, returns the key itself.
    UniString translate(const UniString& key) const;
};

}
