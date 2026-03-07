module;
#include <QObject>
#include <wobjectdefs.h>
#include "DockManager.h"

export module Widgets.Dock.StyleManager;

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

class DockStyleManager : public QObject {
    W_OBJECT(DockStyleManager)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit DockStyleManager(ads::CDockManager* dockManager, QObject* parent = nullptr);
    ~DockStyleManager();
    
    void setGlowEnabled(bool enabled);
    void setGlowColor(const QColor& color);
    void setGlowWidth(int width);
    void setGlowIntensity(float intensity);
    void applyStyle();
};

}
