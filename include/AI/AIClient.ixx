module;
export module AI.Client;

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



import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

class AIClient {
private:
    class Impl;
    Impl* impl_;
public:
    AIClient();
    ~AIClient();

    // singleton
    static AIClient* instance();

    // configuration
    void setApiKey(const UniString& key);
    void setProvider(const UniString& provider);

    // Send a message to AI and get a (dummy) response synchronously for now
    UniString sendMessage(const UniString& message);
};

}
