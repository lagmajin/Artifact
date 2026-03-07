module;
#include <QWidget>
#include <wobjectdefs.h>
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
export module Widgets.AIChatWidget;




import Utils.String.UniString;
import AI.Client;

export namespace Artifact {

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
