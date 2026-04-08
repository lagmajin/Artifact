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
export module Audio.AudioClockProvider;





export namespace Artifact {

using AudioClockFn = std::function<double()>;

export class AudioClockProvider {
private:
    class Impl;
    Impl* impl_;
public:
    AudioClockProvider();
    ~AudioClockProvider();

    void setProvider(const AudioClockFn& fn);
    double now() const; // seconds
};

}
