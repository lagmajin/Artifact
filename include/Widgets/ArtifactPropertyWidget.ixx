module;

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
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
export module Artifact.Widgets.ArtifactPropertyWidget;




import Artifact.Layer.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPropertyWidget : public QScrollArea {
 W_OBJECT(ArtifactPropertyWidget)
private:
    class Impl;
    Impl* impl_;

public:
    explicit ArtifactPropertyWidget(QWidget* parent = nullptr);
    ~ArtifactPropertyWidget();

    void setLayer(Artifact::ArtifactAbstractLayerPtr layer);
    void setFocusedEffectId(const QString& effectId);
    void clear();
    void setFilterText(const QString& text);
    QString filterText() const;
    void setValueColumnFirst(bool enabled);
    bool valueColumnFirst() const;

    void updateProperties();
};

} // namespace Artifact
