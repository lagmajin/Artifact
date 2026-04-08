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
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <wobjectdefs.h>
export module Artifact.Widgets.UndoHistoryWidget;



import Undo.UndoManager;

export namespace Artifact {

class ArtifactUndoHistoryWidget : public QWidget {
 W_OBJECT(ArtifactUndoHistoryWidget)
public:
 explicit ArtifactUndoHistoryWidget(QWidget* parent = nullptr);
 ~ArtifactUndoHistoryWidget() override;

 void refreshHistory();

private:
 class Impl;
 Impl* impl_;
};

}
