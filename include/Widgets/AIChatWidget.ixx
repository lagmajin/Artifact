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
#include <wobjectdefs.h>
#include <QWidget>
export module Widgets.AIChatWidget;





import Utils.String.UniString;

export namespace Artifact {
using ArtifactCore::UniString;

class AIChatWidget : public QWidget {
 W_OBJECT(AIChatWidget)
private:
    class Impl;
    Impl* impl_;
public:
    AIChatWidget(QWidget* parent = nullptr);
    ~AIChatWidget();

    void sendUserMessage(const UniString& msg);

    void setProvider(const UniString& provider);
};

}
