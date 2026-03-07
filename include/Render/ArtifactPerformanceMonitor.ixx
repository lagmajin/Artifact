module;
#include <QLabel>
#include <QTimer>

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
export module Artifact.Render.PerformanceMonitor;





export namespace Artifact {

  // Simple performance stats holder
  struct PerformanceStats {
    double fps = 0.0;
    double gpuUsagePercent = 0.0;
    uint64_t memoryUsageMB = 0;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    double frameTimeMs = 0.0;
  };

  // Simple performance monitor (simplified version)
  class ArtifactPerformanceMonitor {
  private:
    uint32_t frameCount_ = 0;
    double currentFPS_ = 0.0;
    uint64_t frameCountTime_ = 0;
    double gpuUsagePercent_ = 0.0;
    uint64_t memoryUsageMB_ = 0;

  public:
    ArtifactPerformanceMonitor() = default;
    ~ArtifactPerformanceMonitor() = default;

    void recordFrameTime() {
      frameCount_++;
    }

    void updateGPUUsage(double gpuUsagePercent) {
      gpuUsagePercent_ = gpuUsagePercent;
    }

    void updateMemoryUsage(uint64_t memoryMB) {
      memoryUsageMB_ = memoryMB;
    }

    PerformanceStats getStats() const {
      PerformanceStats stats;
      stats.fps = currentFPS_;
      stats.gpuUsagePercent = gpuUsagePercent_;
      stats.memoryUsageMB = memoryUsageMB_;
      return stats;
    }

    void setFPS(double fps) { currentFPS_ = fps; }
  };

}
