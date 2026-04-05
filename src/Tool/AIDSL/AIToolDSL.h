#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>
#include <json/json.h>  // Assume nlohmann_json available

namespace AIToolDSL {

using LayerID = std::string;   // "#L1234"
using CompID = std::string;    // "#C9"
using FrameNum = int64_t;

using Value = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    std::vector<double>
>;

struct CommandResult {
    bool success;
    std::string message;
    Json::Object stats;  // e.g., {"layers_affected": 3}
};

class Interpreter {
public:
    Interpreter();
    ~Interpreter();

    // Set host application callbacks
    using LayerFilter = std::function<std::vector<LayerID>(const std::string& filterExpr)>;
    using PropertyGetter = std::function<Value(const std::string& layerId, const std::string& propPath)>;
    using PropertySetter = std::function<bool(const std::string& layerId, const std::string& propPath, const Value& value)>;
    using KeyframeAdder = std::function<bool(const std::string& layerId, const std::string& propPath, FrameNum frame, const Value& value)>;

    void setLayerFilter(LayerFilter filter) { layerFilter_ = filter; }
    void setPropertyGetter(PropertyGetter getter) { propertyGetter_ = getter; }
    void setPropertySetter(PropertySetter setter) { propertySetter_ = setter; }
    void setKeyframeAdder(KeyframeAdder adder) { keyframeAdder_ = adder; }

    // Main API
    CommandResult execute(const std::string& dsl);
    CommandResult dryRun(const std::string& dsl);
    bool undo();
    bool canUndo() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    LayerFilter layerFilter_;
    PropertyGetter propertyGetter_;
    PropertySetter propertySetter_;
    KeyframeAdder keyframeAdder_;
};

} // namespace AIToolDSL
