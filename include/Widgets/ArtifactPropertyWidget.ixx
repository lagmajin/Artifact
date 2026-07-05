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
#include <QGroupBox>
#include <QLabel>
#include <QSet>

export module Artifact.Widgets.ArtifactPropertyWidget;



import Artifact.Layer.Abstract;
import Artifact.Effect.Abstract;
import Time.Rational;

export namespace Artifact {

class ArtifactPlaybackService;

namespace detail {
enum class LayerPresentationBadgeTone {
  Neutral,
  Container,
  Media,
  Motion,
  Special
};
ArtifactCore::RationalTime currentPlaybackTime(ArtifactPlaybackService* playback);
ArtifactCore::RationalTime currentPlaybackTime(ArtifactPlaybackService* playback, const ArtifactAbstractLayerPtr& layer);
void applyThemeTextPalette(QWidget* widget, int shade);
void applyPropertySectionBox(QGroupBox* box);
void applyPresentationToneLabel(QLabel* label, LayerPresentationBadgeTone tone, bool emphasized = true);
void applyPropertyPanelPalette(QWidget* widget, bool elevated = false);
void notifyLayerPropertyAnimationChanged(const ArtifactAbstractLayerPtr& layer);
std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>
prioritizedSummaryProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>& properties,
    const std::unordered_set<std::string>& preferredKeys,
    std::size_t maxCount);
}

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
    void setCompositionEffects(
        const std::vector<std::shared_ptr<Artifact::ArtifactAbstractEffect>>& effects);
    void setLayers(const QSet<Artifact::ArtifactAbstractLayerPtr>& layers);
    int targetLayersCount() const;
    void setFocusedEffectId(const QString& effectId);
    void clear();
    void setFilterText(const QString& text);
    QString filterText() const;
    void setValueColumnFirst(bool enabled);
    bool valueColumnFirst() const;
    void setSliderBeforeValue(bool enabled);
    bool sliderBeforeValue() const;
    void setFavoriteOnly(bool enabled);
    bool favoriteOnly() const;

    void updateProperties();
    bool openActiveExpressionCopilot();
    bool clearActiveExpression();
    bool convertActiveExpressionToKeyframes();
    bool bakeActivePropertyToKeyframes();
    bool saveActiveExpressionPreset();
    bool loadActiveExpressionPreset();
    bool hasActiveExpressionTarget() const;
    QString activePropertyPath() const;
    Artifact::ArtifactAbstractLayerPtr activePropertyLayer() const;

protected:
    void showEvent(QShowEvent* event) override;
};

} // namespace Artifact
