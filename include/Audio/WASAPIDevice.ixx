module;
#include <cstdint>
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Audio.WASAPIDevice;




import Audio.IAudioDevice;

export namespace Artifact {

// WASAPI device uses Pimpl to keep implementation private
export class WASAPIDevice : public IAudioDevice {
private:
    class Impl;
    Impl* impl_;
public:
    WASAPIDevice();
    ~WASAPIDevice();

    bool open(int sampleRate, int channels, int framesPerBuffer) override;
    void close() override;
    bool start() override;
    void stop() override;
    void write(const float* interleaved, size_t frames) override;
    std::uint64_t position() const override;
    AudioDeviceState state() const override;
};

}
