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
#include <QVBoxLayout>
#include <QScrollArea>

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

    QSize sizeHint() const override;

    void setLayer(Artifact::ArtifactAbstractLayerPtr layer);
    void setFocusedEffectId(const QString& effectId);
    void clear();
    void setFilterText(const QString& text);
    QString filterText() const;
    void setValueColumnFirst(bool enabled);
    bool valueColumnFirst() const;
    void setSliderBeforeValue(bool enabled);
    bool sliderBeforeValue() const;

    void updateProperties();

protected:
    void showEvent(QShowEvent* event) override;
};

} // namespace Artifact
