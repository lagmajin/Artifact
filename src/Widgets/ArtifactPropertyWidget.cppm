module;

#include <QWidget>
#include <QVBoxLayout>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QCursor>
#include <QMenu>
#include <QTimer>
#include <wobjectimpl.h>

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
module Artifact.Widgets.ArtifactPropertyWidget;




import Artifact.Layer.Abstract;
import Property.Abstract;
import Property.Group;
import Undo.UndoManager;
import Artifact.Effect.Abstract;
import Utils.String.UniString;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Widgets.PropertyEditor;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Time.Rational;

namespace Artifact {

namespace {
void clearLayoutRecursive(QLayout* layout) {
    if (!layout) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QLayout* childLayout = item->layout()) {
            clearLayoutRecursive(childLayout);
        }
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}
}

 W_OBJECT_IMPL(ArtifactPropertyWidget)

class ArtifactPropertyWidget::Impl {
public:
    ArtifactPropertyWidget* owner = nullptr;
    QWidget* containerWidget = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    ArtifactAbstractLayerPtr currentLayer;
    QMetaObject::Connection currentLayerChangedConnection;
    QTimer* rebuildTimer = nullptr;
    QTimer* updateValuesTimer = nullptr;
    int rebuildDebounceMs = 80;
    int updateValuesDebounceMs = 16;
    QString filterText;
    QString focusedEffectId;
    bool rebuilding = false;
    bool needsRebuildWhenVisible = false;
    bool valueColumnFirst = false;
    bool sliderBeforeValue = false;
    int localPropertyEditDepth = 0;
    QHash<QString, ArtifactPropertyEditorRowWidget*> propertyEditors;

    void scheduleRebuild(int delayMs = -1)
    {
        if (!rebuildTimer) {
            return;
        }
        if (updateValuesTimer) {
            updateValuesTimer->stop();
        }
        const int delay = (delayMs < 0) ? rebuildDebounceMs : delayMs;
        rebuildTimer->start(std::max(0, delay));
    }

    void scheduleUpdateValues()
    {
        if (!updateValuesTimer || rebuilding) {
            return;
        }
        // If a full rebuild is already scheduled, don't bother with partial update
        if (rebuildTimer && rebuildTimer->isActive()) {
            return;
        }
        updateValuesTimer->start(updateValuesDebounceMs);
    }

    void rebuildUI();
    void updatePropertyValues();
    void applyLockState();
};

void ArtifactPropertyWidget::showEvent(QShowEvent* event) {
    QScrollArea::showEvent(event);
    if (impl_->needsRebuildWhenVisible) {
        impl_->needsRebuildWhenVisible = false;
        QTimer::singleShot(0, this, [this]() {
            if (impl_) {
                impl_->rebuildUI();
            }
        });
    } else {
        impl_->updatePropertyValues();
    }
}

namespace {

class ScopedPropertyEditGuard final
{
public:
    explicit ScopedPropertyEditGuard(int& depth) : depth_(depth) { ++depth_; }
    ~ScopedPropertyEditGuard() { --depth_; }
private:
    int& depth_;
};

QString humanizePropertyLabel(QString name)
{
    static const QHash<QString, QString> explicitLabels = {
        { QStringLiteral("transform.position.x"), QStringLiteral("Position X") },
        { QStringLiteral("transform.position.y"), QStringLiteral("Position Y") },
        { QStringLiteral("transform.scale.x"),    QStringLiteral("Scale X") },
        { QStringLiteral("transform.scale.y"),    QStringLiteral("Scale Y") },
        { QStringLiteral("transform.rotation"),   QStringLiteral("Rotation") },
        { QStringLiteral("transform.anchor.x"),   QStringLiteral("Anchor X") },
        { QStringLiteral("transform.anchor.y"),   QStringLiteral("Anchor Y") },
        { QStringLiteral("layer.opacity"),        QStringLiteral("Opacity") },
        { QStringLiteral("time.inPoint"),         QStringLiteral("In Point") },
        { QStringLiteral("time.outPoint"),        QStringLiteral("Out Point") },
        { QStringLiteral("time.startTime"),       QStringLiteral("Start Time") }
    };
    if (const auto it = explicitLabels.constFind(name); it != explicitLabels.constEnd()) {
        return it.value();
    }

    const int dot = name.lastIndexOf('.');
    if (dot >= 0 && dot + 1 < name.size()) {
        name = name.mid(dot + 1);
    }

    QString out;
    out.reserve(name.size() * 2);
    for (int i = 0; i < name.size(); ++i) {
        const QChar ch = name.at(i);
        if (ch == '_' || ch == '-') {
            out += ' ';
            continue;
        }
        if (i > 0 && ch.isUpper() && name.at(i - 1).isLetterOrNumber()) {
            out += ' ';
        }
        out += ch;
    }

    bool cap = true;
    for (int i = 0; i < out.size(); ++i) {
        if (out.at(i).isSpace()) {
            cap = true;
            continue;
        }
        if (cap) {
            out[i] = out.at(i).toUpper();
            cap = false;
        }
    }
    return out;
}

bool propertyMatchesFilter(const ArtifactCore::AbstractProperty& property, const QString& filterText)
{
    const QString query = filterText.trimmed();
    if (query.isEmpty()) {
        return true;
    }

    const QString key = property.getName();
    const QString friendly = humanizePropertyLabel(key);
    return key.contains(query, Qt::CaseInsensitive) || friendly.contains(query, Qt::CaseInsensitive);
}

void notifyProjectIfLayerNameChanged(const QString& propertyName)
{
    if (propertyName.compare(QStringLiteral("layer.name"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (auto* service = ArtifactProjectService::instance()) {
        service->projectChanged();
    }
}

void notifyProjectIfTimelinePropertyChanged(const QString& propertyName)
{
    const bool isTimelineProperty =
        propertyName.compare(QStringLiteral("time.inPoint"), Qt::CaseInsensitive) == 0 ||
        propertyName.compare(QStringLiteral("time.outPoint"), Qt::CaseInsensitive) == 0 ||
        propertyName.compare(QStringLiteral("time.startTime"), Qt::CaseInsensitive) == 0;
    if (!isTimelineProperty) {
        return;
    }
    if (auto* service = ArtifactProjectService::instance()) {
        service->projectChanged();
    }
}

ArtifactPropertyEditorRowWidget* createPropertyRow(
    QWidget* parent,
    const std::shared_ptr<ArtifactCore::AbstractProperty>& propertyPtr,
    const std::function<void(const QString&, const QVariant&)>& applyValue)
{
    if (!propertyPtr) return nullptr;
    const auto& property = *propertyPtr;

    auto* editor = createPropertyEditorWidget(property, parent);
    if (!editor) {
        return nullptr;
    }

    const auto meta = property.metadata();
    const QString labelText = meta.displayLabel.isEmpty() ? humanizePropertyLabel(property.getName()) : meta.displayLabel;
    auto* row = new ArtifactPropertyEditorRowWidget(labelText, editor, property.getName(), parent);

    const auto applyPropertyValue = [applyValue, propertyPtr, propertyName = property.getName()](const QVariant& value) {
        if (propertyPtr) {
            propertyPtr->setValue(value);
        }
        applyValue(propertyName, value);
    };
    editor->setPreviewHandler(applyPropertyValue);
    editor->setCommitHandler(applyPropertyValue);

    if (!meta.tooltip.isEmpty()) {
        row->setEditorToolTip(meta.tooltip);
    }

    const QVariant defaultValue = property.getDefaultValue();
    row->setShowResetButton(defaultValue.isValid());
    if (defaultValue.isValid()) {
        row->setResetHandler([editor, propertyPtr, applyValue, propertyName = property.getName(), defaultValue]() {
            editor->setValueFromVariant(defaultValue);
            if (propertyPtr) {
                propertyPtr->setValue(defaultValue);
            }
            applyValue(propertyName, defaultValue);
        });
    }

    const bool animatable = property.isAnimatable();
    row->setShowKeyframeButton(animatable);
    row->setNavigationEnabled(false);
    if (animatable) {
        const QString propertyName = property.getName();
        auto* playback = ArtifactPlaybackService::instance();
        const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
        const int64_t fps_val = static_cast<int64_t>(std::round(frameRate.framerate()));
        const auto track = propertyPtr->getKeyFrames();
        row->setNavigationEnabled(!track.empty());
        
        // 現時点でのキーフレーム状態を反映
        if (playback) {
            const auto now = RationalTime(playback->currentFrame().framePosition(), fps_val);
            row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
            const QVariant animatedValue = propertyPtr->interpolateValue(now);
            if (animatedValue.isValid()) {
                editor->setValueFromVariant(animatedValue);
            }
        }

        // キーフレームトグル (◆ボタン)
        row->setKeyframeHandler([propertyPtr, playback, row, editor, fps_val](bool checked) {
            if (!playback || !propertyPtr) return;
            const auto nowPos = playback->currentFrame();
            const auto nowTime = RationalTime(nowPos.framePosition(), fps_val);

            if (checked) {
                propertyPtr->addKeyFrame(nowTime, editor->value());
            } else {
                propertyPtr->removeKeyFrame(nowTime);
            }
            // 状態を再反映
            row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(nowTime));
            row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
        });

        // ナビゲーション (◀ ▶ボタン)
        row->setNavigationHandler([propertyPtr, playback, fps_val](int direction) {
            if (!playback || !propertyPtr) return;
            const auto track = propertyPtr->getKeyFrames();
            if (track.empty()) return;

            const auto nowTime = RationalTime(playback->currentFrame().framePosition(), fps_val);
            if (direction > 0) {
                // 次のキーフレームへ
                for (const auto& kf : track) {
                    if (kf.time > nowTime) {
                        playback->goToFrame(FramePosition(static_cast<int>(kf.time.rescaledTo(fps_val))));
                        break;
                    }
                }
            } else {
                // 前のキーフレームへ
                for (auto it = track.rbegin(); it != track.rend(); ++it) {
                    if (it->time < nowTime) {
                        playback->goToFrame(FramePosition(static_cast<int>(it->time.rescaledTo(fps_val))));
                        break;
                    }
                }
            }
        });
    }

    const bool showExpressionButton = false;
    row->setShowExpressionButton(showExpressionButton);
    if (showExpressionButton) {
        row->setExpressionHandler([propertyName = property.getName()]() {
            auto* copilot = new ArtifactExpressionCopilotWidget();
            copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
            copilot->setWindowTitle(QStringLiteral("Expression Copilot: %1").arg(propertyName));
            copilot->setAttribute(Qt::WA_DeleteOnClose);
            copilot->move(QCursor::pos() - QPoint(150, 200));
            copilot->show();
        });
    }

    return row;
}

void addRowsFromProperties(
    QWidget* parent,
    QVBoxLayout* layout,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>& properties,
    const QString& filterText,
    const std::function<void(const QString&, const QVariant&)>& applyValue,
    bool* addedAny,
    QHash<QString, ArtifactPropertyEditorRowWidget*>* registry = nullptr)
{
    for (const auto& ptr : properties) {
        if (!ptr || !propertyMatchesFilter(*ptr, filterText)) {
            continue;
        }
        if (auto* row = createPropertyRow(parent, ptr, applyValue)) {
            layout->addWidget(row);
            if (registry) {
                registry->insert(ptr->getName(), row);
            }
            if (addedAny) {
                *addedAny = true;
            }
        }
    }
}

std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> prioritizedSummaryProperties(
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>& properties,
    const std::unordered_set<std::string>& preferredKeys,
    const std::size_t maxCount)
{
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> preferred;
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> fallback;
    preferred.reserve(properties.size());
    fallback.reserve(properties.size());

    for (const auto& property : properties) {
        if (!property) {
            continue;
        }
        const auto key = property->getName().toStdString();
        if (preferredKeys.contains(key)) {
            preferred.push_back(property);
        } else {
            fallback.push_back(property);
        }
    }

    preferred.insert(preferred.end(), fallback.begin(), fallback.end());
    if (preferred.size() > maxCount) {
        preferred.resize(maxCount);
    }
    return preferred;
}

} // namespace

ArtifactPropertyWidget::ArtifactPropertyWidget(QWidget* parent)
    : QScrollArea(parent), impl_(new Impl()) 
{
    impl_->owner = this;
    ArtifactPropertyEditorRowWidget::setGlobalLayoutMode(
        impl_->valueColumnFirst ? ArtifactPropertyRowLayoutMode::EditorThenLabel
                                : ArtifactPropertyRowLayoutMode::LabelThenEditor);
    setGlobalNumericEditorLayoutMode(
        impl_->sliderBeforeValue ? ArtifactNumericEditorLayoutMode::SliderThenValue
                                 : ArtifactNumericEditorLayoutMode::ValueThenSlider);
    setObjectName(QStringLiteral("artifactPropertyWidget"));
    setMinimumWidth(360);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setContextMenuPolicy(Qt::CustomContextMenu);
    impl_->containerWidget = new QWidget(this);
    impl_->containerWidget->setObjectName(QStringLiteral("artifactPropertyContainer"));
    impl_->mainLayout = new QVBoxLayout(impl_->containerWidget);
    impl_->mainLayout->setAlignment(Qt::AlignTop);
    impl_->mainLayout->setContentsMargins(8, 8, 8, 10);
    impl_->mainLayout->setSpacing(6);

    setWidget(impl_->containerWidget);
    impl_->rebuildTimer = new QTimer(this);
    impl_->rebuildTimer->setSingleShot(true);
    QObject::connect(impl_->rebuildTimer, &QTimer::timeout, this, [this]() {
        impl_->rebuildUI();
    });

    impl_->updateValuesTimer = new QTimer(this);
    impl_->updateValuesTimer->setSingleShot(true);
    QObject::connect(impl_->updateValuesTimer, &QTimer::timeout, this, [this]() {
        impl_->updatePropertyValues();
    });
    QObject::connect(this, &QWidget::customContextMenuRequested, this,
                     [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* sliderBeforeValueAct = menu.addAction(QStringLiteral("Slider before value"));
        sliderBeforeValueAct->setCheckable(true);
        sliderBeforeValueAct->setChecked(impl_->sliderBeforeValue);
        QObject::connect(sliderBeforeValueAct, &QAction::toggled, this,
                         [this](bool checked) {
            setSliderBeforeValue(checked);
        });
        menu.exec(mapToGlobal(pos));
    });
    if (auto* playback = ArtifactPlaybackService::instance()) {
        QObject::connect(playback, &ArtifactPlaybackService::frameChanged, this, [this, playback]() {
            if (isVisible()) {
                // [Optimization] If playing, only update if it's the first frame of playback or not playing.
                // High-frequency UI updates during playback can cause significant lag.
                if (!playback->isPlaying()) {
                    impl_->updatePropertyValues();
                }
            } else {
                impl_->needsRebuildWhenVisible = true;
            }
        });
        QObject::connect(playback, &ArtifactPlaybackService::playbackStateChanged, this, [this](PlaybackState state) {
            if (state != PlaybackState::Playing) {
                impl_->updatePropertyValues();
            }
        });
    }
    if (auto* projectService = ArtifactProjectService::instance()) {
        QObject::connect(projectService, &ArtifactProjectService::projectChanged, this, [this]() {
            if (impl_->currentLayer) {
                impl_->scheduleRebuild();
            }
        });
    }
}

ArtifactPropertyWidget::~ArtifactPropertyWidget() {
   if (impl_->currentLayerChangedConnection) {
       QObject::disconnect(impl_->currentLayerChangedConnection);
   }
   delete impl_;
}

QSize ArtifactPropertyWidget::sizeHint() const {
   return QSize(300, 600);
}

void ArtifactPropertyWidget::setLayer(ArtifactAbstractLayerPtr layer) {    if (impl_->currentLayer == layer) return;
    
    if (impl_->currentLayerChangedConnection) {
        QObject::disconnect(impl_->currentLayerChangedConnection);
    }

    impl_->currentLayer = layer;
    
    // Connect to new layer
    if (impl_->currentLayer) {
        impl_->currentLayerChangedConnection =
            connect(impl_->currentLayer.get(), &ArtifactAbstractLayer::changed, this, [this]() {
                if (impl_->localPropertyEditDepth > 0) {
                    return;
                }
                impl_->scheduleUpdateValues();
            });
    }

    updateProperties();
}

void ArtifactPropertyWidget::setFocusedEffectId(const QString& effectId) {
    if (impl_->focusedEffectId == effectId) return;
    impl_->focusedEffectId = effectId;
    updateProperties();
}

void ArtifactPropertyWidget::clear() {
    impl_->currentLayer = nullptr;
    impl_->focusedEffectId.clear();
    updateProperties();
}

void ArtifactPropertyWidget::setFilterText(const QString& text) {
    if (impl_->filterText == text) return;
    impl_->filterText = text;
    updateProperties();
}

QString ArtifactPropertyWidget::filterText() const {
    return impl_->filterText;
}

void ArtifactPropertyWidget::setValueColumnFirst(const bool enabled)
{
    if (impl_->valueColumnFirst == enabled) {
        return;
    }
    impl_->valueColumnFirst = enabled;
    ArtifactPropertyEditorRowWidget::setGlobalLayoutMode(
        enabled ? ArtifactPropertyRowLayoutMode::EditorThenLabel
                : ArtifactPropertyRowLayoutMode::LabelThenEditor);
    updateProperties();
}

bool ArtifactPropertyWidget::valueColumnFirst() const
{
    return impl_->valueColumnFirst;
}

void ArtifactPropertyWidget::setSliderBeforeValue(const bool enabled)
{
    if (impl_->sliderBeforeValue == enabled) {
        return;
    }
    impl_->sliderBeforeValue = enabled;
    setGlobalNumericEditorLayoutMode(
        enabled ? ArtifactNumericEditorLayoutMode::SliderThenValue
                : ArtifactNumericEditorLayoutMode::ValueThenSlider);
    updateProperties();
}

bool ArtifactPropertyWidget::sliderBeforeValue() const
{
    return impl_->sliderBeforeValue;
}

void ArtifactPropertyWidget::updateProperties() {
    impl_->scheduleRebuild(80);
}

void ArtifactPropertyWidget::Impl::updatePropertyValues() {
    if (!currentLayer || rebuilding) return;
    
    auto* playback = ArtifactPlaybackService::instance();
    const auto frameRate = playback ? playback->frameRate() : FrameRate(30.0f);
    const int64_t fps_val = static_cast<int64_t>(std::round(frameRate.framerate()));
    const auto now = playback ? RationalTime(playback->currentFrame().framePosition(), fps_val) : RationalTime(0, 30);

    for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
        const QString& propName = it.key();
        auto* row = it.value();
        if (!row) continue;

        auto propertyPtr = currentLayer->getProperty(propName);
        if (!propertyPtr) continue;

        auto* editor = row->editor();
        if (!editor) continue;

        // アニメーション値を計算して反映
        const QVariant animatedValue = propertyPtr->interpolateValue(now);
        if (animatedValue.isValid() && !editor->hasFocus()) {
            editor->setValueFromVariant(animatedValue);
        }

        // キーフレーム状態（◆ボタン）の更新
        row->setKeyframeChecked(propertyPtr->hasKeyFrameAt(now));
        row->setNavigationEnabled(!propertyPtr->getKeyFrames().empty());
    }

    applyLockState();
}

void ArtifactPropertyWidget::Impl::applyLockState() {
    if (!currentLayer) {
        return;
    }

    const bool locked = currentLayer->isLocked();
    for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
        auto* row = it.value();
        if (!row) {
            continue;
        }
        const bool isLockRow =
            it.key().compare(QStringLiteral("layer.locked"), Qt::CaseInsensitive) == 0;
        row->setEnabled(!locked || isLockRow);
    }
}

void ArtifactPropertyWidget::Impl::rebuildUI() {
    if (rebuilding) return;
    
    // 1. 可視性チェック: 非表示なら後回しにする
    if (!owner->isVisible()) {
        needsRebuildWhenVisible = true;
        return;
    }

    rebuilding = true;

    // 既存ウィジェットを全て非表示にする（破棄しない）
    QSet<QString> reusedKeys;
    for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
            // レイアウトから一時的に取り除く
            mainLayout->removeWidget(it.value());
        }
    }

    clearLayoutRecursive(mainLayout);

    if (!currentLayer) {
        QLabel* emptyLabel = new QLabel("No layer selected");
        emptyLabel->setObjectName(QStringLiteral("propertyEmptyLabel"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(emptyLabel);
        rebuilding = false;
        return;
    }

    // ウィジェット再利用ヘルパー
    auto getOrCreateRow = [this, &reusedKeys](const QString& key, auto createFn) -> ArtifactPropertyEditorRowWidget* {
        auto it = propertyEditors.find(key);
        if (it != propertyEditors.end() && it.value()) {
            reusedKeys.insert(key);
            it.value()->show();
            return it.value();
        }
        auto* row = createFn();
        if (row) {
            propertyEditors.insert(key, row);
            reusedKeys.insert(key);
        }
        return row;
    };

    auto* searchEdit = new QLineEdit();
    searchEdit->setObjectName(QStringLiteral("propertyFilterEdit"));
    searchEdit->setPlaceholderText("Search properties...");
    searchEdit->setText(filterText);
    QObject::connect(searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        filterText = text;
        rebuildUI();
    });
    mainLayout->addWidget(searchEdit);

    bool hasAnyProperties = false;

    auto* summaryGroup = new QGroupBox(QStringLiteral("Summary"));
    auto* summaryLayout = new QVBoxLayout(summaryGroup);
    summaryLayout->setContentsMargins(8, 8, 8, 8);
    summaryLayout->setSpacing(4);

    const std::unordered_set<std::string> keyLayerProperties = {
        "transform.position.x",
        "transform.position.y",
        "transform.scale.x",
        "transform.scale.y",
        "transform.rotation",
        "transform.anchor.x",
        "transform.anchor.y",
        "layer.opacity",
        "layer.name",
        "layer.visible",
        "layer.locked",
        "layer.solo",
        "layer.shy",
        "time.inPoint",
        "time.outPoint",
        "time.startTime"
    };

    const auto layerGroups = currentLayer->getLayerPropertyGroups();
    std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>> layerSummaryProperties;
    for (const auto& groupDef : layerGroups) {
        auto sortedProps = groupDef.sortedProperties();
        // Keep the quick-edit strip large enough to include anchor and opacity.
        auto picked = prioritizedSummaryProperties(sortedProps, keyLayerProperties, 8);
        layerSummaryProperties.insert(layerSummaryProperties.end(), picked.begin(), picked.end());
    }

    auto removePropertyByName = [&layerSummaryProperties](const QString& propertyName) {
        auto it = std::find_if(layerSummaryProperties.begin(), layerSummaryProperties.end(),
            [&propertyName](const std::shared_ptr<ArtifactCore::AbstractProperty>& property) {
                return property && property->getName().compare(propertyName, Qt::CaseInsensitive) == 0;
            });
        if (it == layerSummaryProperties.end()) {
            return std::shared_ptr<ArtifactCore::AbstractProperty>();
        }
        auto property = *it;
        layerSummaryProperties.erase(it);
        return property;
    };

    bool hasSummaryProperties = false;
    if (auto layerNameProperty = removePropertyByName(QStringLiteral("layer.name"))) {
        if (auto* row = createPropertyRow(
                summaryGroup,
                layerNameProperty,
                [this](const QString& name, const QVariant& value) {
                    if (currentLayer) {
                        ScopedPropertyEditGuard guard(localPropertyEditDepth);
                        currentLayer->setLayerPropertyValue(name, value);
                        notifyProjectIfLayerNameChanged(name);
                        notifyProjectIfTimelinePropertyChanged(name);
                    }
                })) {
            summaryLayout->addWidget(row);
            propertyEditors.insert(layerNameProperty->getName(), row);
            hasSummaryProperties = true;
        }
    }

    addRowsFromProperties(
        summaryGroup,
        summaryLayout,
        layerSummaryProperties,
        filterText,
        [this](const QString& name, const QVariant& value) {
            if (currentLayer) {
                ScopedPropertyEditGuard guard(localPropertyEditDepth);
                currentLayer->setLayerPropertyValue(name, value);
                notifyProjectIfLayerNameChanged(name);
                notifyProjectIfTimelinePropertyChanged(name);
            }
        },
        &hasSummaryProperties,
        &propertyEditors);

    const auto effects = currentLayer->getEffects();
    const bool hasFocusedEffect = !focusedEffectId.trimmed().isEmpty();
    for (const auto& effect : effects) {
        if (!effect) {
            continue;
        }
        if (hasFocusedEffect && effect->effectID().toQString() != focusedEffectId) {
            continue;
        }

        ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
        for (const auto& property : effect->getProperties()) {
            propGroup.addProperty(std::make_shared<ArtifactCore::AbstractProperty>(property));
        }

        auto effectSummary = prioritizedSummaryProperties(propGroup.sortedProperties(), {}, 3);
        if (effectSummary.empty()) {
            continue;
        }

        auto* effectLabel = new QLabel(QStringLiteral("Effect: %1").arg(effect->displayName().toQString()), summaryGroup);
        effectLabel->setStyleSheet(QStringLiteral("QLabel { color: #E8E8E8; font-weight: bold; }"));
        summaryLayout->addWidget(effectLabel);

        addRowsFromProperties(
            summaryGroup,
            summaryLayout,
            effectSummary,
            filterText,
            [this, effect](const QString& name, const QVariant& value) {
                ScopedPropertyEditGuard guard(localPropertyEditDepth);
                effect->setPropertyValue(name, value);
            },
            &hasSummaryProperties,
            &propertyEditors);
    }

    if (hasSummaryProperties) {
        mainLayout->addWidget(summaryGroup);
        hasAnyProperties = true;
    } else {
        delete summaryGroup;
    }

    for (const auto& groupDef : layerGroups) {
        QGroupBox* group = new QGroupBox(groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name());
        auto* groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(8, 8, 8, 8);
        groupLayout->setSpacing(4);
    
    group->setStyleSheet(R"(
        QGroupBox {
            background: transparent;
            border: none;
            border-top: 1px solid #333;
            margin-top: 24px;
            padding-top: 2px;
            font-weight: 700;
            font-size: 10px;
            color: #AAA;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 4px 10px;
            background: #252525;
            color: #DDD;
            letter-spacing: 1px;
            width: 100%;
        }
    )");

        auto sortedProps = groupDef.sortedProperties();
        bool addedGroupProperties = false;
        addRowsFromProperties(
            group,
            groupLayout,
            sortedProps,
            filterText,
            [this](const QString& name, const QVariant& value) {
                if (currentLayer) {
                    ScopedPropertyEditGuard guard(localPropertyEditDepth);
                    currentLayer->setLayerPropertyValue(name, value);
                    notifyProjectIfLayerNameChanged(name);
                    notifyProjectIfTimelinePropertyChanged(name);
                }
            },
            &addedGroupProperties,
            &propertyEditors);
        if (addedGroupProperties) {
            mainLayout->addWidget(group);
            hasAnyProperties = true;
        } else {
            delete group;
        }
    }

    if (hasFocusedEffect) {
        auto* focusedLabel = new QLabel(QStringLiteral("Focused Effect ID: %1").arg(focusedEffectId));
        focusedLabel->setStyleSheet(QStringLiteral("QLabel { color: #D0D0D0; font-size: 11px; }"));
        mainLayout->addWidget(focusedLabel);
    }

    for (const auto& effect : effects) {
        if (!effect) continue;
        if (hasFocusedEffect && effect->effectID().toQString() != focusedEffectId) {
            continue;
        }

        QString stageName = "Unknown";
        switch (effect->pipelineStage()) {
            case EffectPipelineStage::Generator: stageName = "[Generator]"; break;
            case EffectPipelineStage::GeometryTransform: stageName = "[Geo Transform]"; break;
            case EffectPipelineStage::MaterialRender: stageName = "[Material]"; break;
            case EffectPipelineStage::Rasterizer: stageName = "[Rasterizer]"; break;
            case EffectPipelineStage::LayerTransform: stageName = "[Layer Transform]"; break;
        }

        QGroupBox* group = new QGroupBox(QString("%1 %2").arg(stageName).arg(effect->displayName().toQString()));
        auto* groupLayout = new QVBoxLayout(group);
        groupLayout->setContentsMargins(8, 8, 8, 8);
        groupLayout->setSpacing(4);
    
    group->setStyleSheet(R"(
        QGroupBox {
            background: transparent;
            border: none;
            border-top: 1px solid #333;
            margin-top: 24px;
            padding-top: 2px;
            font-weight: 700;
            font-size: 10px;
            color: #AAA;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 4px 10px;
            background: #252525;
            color: #DDD;
            letter-spacing: 1px;
            width: 100%;
        }
    )");

        ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
        for (const auto& p : effect->getProperties()) {
            propGroup.addProperty(std::make_shared<ArtifactCore::AbstractProperty>(p));
        }

        auto sortedProps = propGroup.sortedProperties();
        bool addedGroupProperties = false;
        addRowsFromProperties(
            group,
            groupLayout,
            sortedProps,
            filterText,
            [this, effect](const QString& name, const QVariant& value) {
                ScopedPropertyEditGuard guard(localPropertyEditDepth);
                effect->setPropertyValue(name, value);
            },
            &addedGroupProperties,
            &propertyEditors);

        if (addedGroupProperties) {
            mainLayout->addWidget(group);
            hasAnyProperties = true;
        } else {
            delete group;
        }
    }

    if (!hasAnyProperties) {
        QLabel* emptyProps = new QLabel("No editable properties");
        emptyProps->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(emptyProps);
    }

    mainLayout->addStretch();
    applyLockState();

    // 再利用されなかったウィジェットを削除
    QStringList toRemove;
    for (auto it = propertyEditors.begin(); it != propertyEditors.end(); ++it) {
        if (!reusedKeys.contains(it.key())) {
            if (it.value()) {
                it.value()->deleteLater();
            }
            toRemove.append(it.key());
        }
    }
    for (const auto& key : toRemove) {
        propertyEditors.remove(key);
    }

    rebuilding = false;
}

} // namespace Artifact
