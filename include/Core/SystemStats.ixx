module;
#include <QtGlobal>
export module Core.SystemStats;

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




export namespace ArtifactCore {

struct SystemMemoryInfo {
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
};

struct ProcessCpuInfo {
    double processPercent = 0.0; // percent of one CPU (0-100)
};

class CpuMonitor {
public:
    CpuMonitor();
    ~CpuMonitor();

    SystemMemoryInfo queryMemory() const;
    ProcessCpuInfo queryProcessCpu();
};

}
