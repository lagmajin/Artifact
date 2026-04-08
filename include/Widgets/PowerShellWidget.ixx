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
#include <QWidget>
#include <wobjectdefs.h>
export module Widgets.PowerShellWidget;




import Utils.String.UniString;

export namespace Artifact {

class PowerShellWidget : public QWidget {
 W_OBJECT(PowerShellWidget)
private:
    class Impl;
    Impl* impl_;
public:
    PowerShellWidget(QWidget* parent = nullptr);
    ~PowerShellWidget();

    ArtifactCore::UniString runCommand(const ArtifactCore::UniString& command);
    void runCommandAsync(const ArtifactCore::UniString& command);
    void appendLog(const ArtifactCore::UniString& text);

    void commandFinished(const ArtifactCore::UniString& output) W_SIGNAL(commandFinished, output);
};

} // namespace Artifact

