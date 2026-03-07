module;
export module ArtifactAbstractAudioEffects;

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

// I[fBIGtFNg̒ۃx[XNX
class ArtifactAbstractAudioEffects {
public:
    ArtifactAbstractAudioEffects() = default;
    virtual ~ArtifactAbstractAudioEffects() = default;

    // GtFNg
    virtual std::string name() const = 0;

    // L/
    virtual void setEnabled(bool enabled) { enabled_ = enabled; }
    virtual bool isEnabled() const { return enabled_; }

    // p[^i: key-valuej
    virtual void setParameter(const std::string& key, float value) = 0;
    virtual float getParameter(const std::string& key) const = 0;

    // I[fBIiin-placej
    // buffer: floatz, samples: Tv, channels: `l
    virtual void process(float* buffer, int samples, int channels) = 0;

protected:
    bool enabled_ = true;
};

}
