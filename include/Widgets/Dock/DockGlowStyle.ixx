module;
#include <QProxyStyle>
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
export module Widgets.Dock.GlowStyle;





export namespace Artifact {

class DockGlowStyle : public QProxyStyle {
    W_OBJECT(DockGlowStyle)
private:
    class Impl;
    Impl* impl_;
    
public:
    explicit DockGlowStyle(QStyle* baseStyle = nullptr);
    ~DockGlowStyle();
    
    void setGlowEnabled(bool enabled);
    void setGlowColor(const QColor& color);
    void setGlowWidth(int width);
    void setGlowIntensity(float intensity);
    
    // QStyle overrides
    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                      QPainter* painter, const QWidget* widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                           QPainter* painter, const QWidget* widget = nullptr) const override;
    
private:
    void drawDockWidgetGlow(const QStyleOption* option, QPainter* painter,
                           const QWidget* widget) const;
    void drawDockTabGlow(const QStyleOption* option, QPainter* painter,
                        const QWidget* widget) const;
    bool isDockWidgetActive(const QWidget* widget) const;
    bool isDockTabActive(const QWidget* widget) const;
};

}
