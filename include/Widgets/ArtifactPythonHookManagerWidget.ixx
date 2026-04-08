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
export module Artifact.Widgets.PythonHookManagerWidget;


export namespace Artifact {

class ArtifactPythonHookManagerWidget : public QWidget {
 W_OBJECT(ArtifactPythonHookManagerWidget)
public:
 explicit ArtifactPythonHookManagerWidget(QWidget* parent = nullptr);
 ~ArtifactPythonHookManagerWidget() override;

 void refresh();

private:
 class Impl;
 Impl* impl_;
};

}
