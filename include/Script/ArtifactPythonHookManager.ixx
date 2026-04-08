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
#include <QString>
#include <QStringList>
export module Artifact.Script.Hooks;

import std;

export namespace Artifact {

class ArtifactPythonHookManager {
public:
 static QStringList knownHooks();
 static QString hookScriptPath(const QString& hookName);
 static bool hookScriptExists(const QString& hookName);

 static bool isHookEnabled(const QString& hookName);
 static void setHookEnabled(const QString& hookName, bool enabled);

 static bool runHook(const QString& hookName, const QStringList& args = {});
};

}
