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
module Audio.AudioClockProvider;




import Audio.AudioClockProvider;

namespace Artifact {

class AudioClockProvider::Impl {
public:
    AudioClockFn fn_ = nullptr;
};

AudioClockProvider::AudioClockProvider(): impl_(new Impl()) {}
AudioClockProvider::~AudioClockProvider(){ delete impl_; }

void AudioClockProvider::setProvider(const AudioClockFn& fn) { impl_->fn_ = fn; }

double AudioClockProvider::now() const {
    if (impl_->fn_) return impl_->fn_();
    return 0.0;
}

}
