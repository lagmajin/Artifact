module;
export module Audio.IAudioDevice;

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



import <cstdint>;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

enum class AudioDeviceState {
    Closed,
    Opened,
    Started,
    Stopped
};

// Minimal audio device abstraction. Implementations must inherit and implement virtual methods.
export class IAudioDevice {
private:
    class Impl;
    Impl* impl_;
public:
    IAudioDevice() {}
    virtual ~IAudioDevice() {}

    // Open device with sample rate, channel count and frames per callback buffer
    virtual bool open(int sampleRate, int channels, int framesPerBuffer) = 0;
    virtual void close() = 0;

    // Start/stop streaming
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Write interleaved float samples to device (blocking or buffered depending on implementation)
    // frames = number of frames (samples per channel)
    virtual void write(const float* interleaved, size_t frames) = 0;

    // Query playback position in frames
    virtual std::uint64_t position() const = 0;

    // Query device state
    virtual AudioDeviceState state() const = 0;
};

}
