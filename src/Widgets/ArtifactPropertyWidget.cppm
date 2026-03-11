module;

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QCursor>
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

namespace Artifact {

 W_OBJECT_IMPL(ArtifactPropertyWidget)

class ArtifactPropertyWidget::Impl {
public:
    QWidget* containerWidget = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    ArtifactAbstractLayerPtr currentLayer;
    QString filterText;
    QString focusedEffectId;
    bool rebuilding = false;

    void rebuildUI();
};

namespace {

QString humanizePropertyLabel(QString name)
{
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

ArtifactPropertyEditorRowWidget* createPropertyRow(
    QWidget* parent,
    const ArtifactCore::AbstractProperty& property,
    const std::function<void(const QString&, const QVariant&)>& applyValue)
{
    auto* editor = createPropertyEditorWidget(property, parent);
    if (!editor) {
        return nullptr;
    }

    const auto meta = property.metadata();
    const QString labelText = meta.displayLabel.isEmpty() ? humanizePropertyLabel(property.getName()) : meta.displayLabel;
    auto* row = new ArtifactPropertyEditorRowWidget(labelText, editor, property.getName(), parent);
    editor->setCommitHandler([applyValue, propertyName = property.getName()](const QVariant& value) {
        applyValue(propertyName, value);
    });

    if (!meta.tooltip.isEmpty()) {
        row->setEditorToolTip(meta.tooltip);
    }

    const QVariant defaultValue = property.getDefaultValue();
    row->setShowResetButton(defaultValue.isValid());
    if (defaultValue.isValid()) {
        row->setResetHandler([editor, applyValue, propertyName = property.getName(), defaultValue]() {
            editor->setValueFromVariant(defaultValue);
            applyValue(propertyName, defaultValue);
        });
    }

    row->setShowKeyframeButton(property.isAnimatable());
    row->setExpressionHandler([propertyName = property.getName()]() {
        auto* copilot = new ArtifactExpressionCopilotWidget();
        copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
        copilot->setWindowTitle(QStringLiteral("Expression Copilot: %1").arg(propertyName));
        copilot->setAttribute(Qt::WA_DeleteOnClose);
        copilot->move(QCursor::pos() - QPoint(150, 200));
        copilot->show();
    });

    return row;
}

void addRowsFromProperties(
    QWidget* parent,
    QVBoxLayout* layout,
    const std::vector<std::shared_ptr<ArtifactCore::AbstractProperty>>& properties,
    const QString& filterText,
    const std::function<void(const QString&, const QVariant&)>& applyValue,
    bool* addedAny)
{
    for (const auto& ptr : properties) {
        if (!ptr || !propertyMatchesFilter(*ptr, filterText)) {
            continue;
        }
        if (auto* row = createPropertyRow(parent, *ptr, applyValue)) {
            layout->addWidget(row);
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
    setWidgetResizable(true);
    impl_->containerWidget = new QWidget(this);
    impl_->mainLayout = new QVBoxLayout(impl_->containerWidget);
    impl_->mainLayout->setAlignment(Qt::AlignTop);
    
    setWidget(impl_->containerWidget);
}

ArtifactPropertyWidget::~ArtifactPropertyWidget() {
    delete impl_;
}

void ArtifactPropertyWidget::setLayer(ArtifactAbstractLayerPtr layer) {
    if (impl_->currentLayer == layer) return;
    
    // Disconnect from old layer
    if (impl_->currentLayer) {
        disconnect(impl_->currentLayer.get(), &ArtifactAbstractLayer::changed, this, &ArtifactPropertyWidget::updateProperties);
    }

    impl_->currentLayer = layer;
    
    // Connect to new layer
    if (impl_->currentLayer) {
        connect(impl_->currentLayer.get(), &ArtifactAbstractLayer::changed, this, &ArtifactPropertyWidget::updateProperties);
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

void ArtifactPropertyWidget::updateProperties() {
    impl_->rebuildUI();
}

void ArtifactPropertyWidget::Impl::rebuildUI() {
    if (rebuilding) return;
    rebuilding = true;

    // 古いウィジェットをクリア
    QLayoutItem* child;
    while ((child = mainLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    auto* searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search properties...");
    searchEdit->setText(filterText);
    QObject::connect(searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        filterText = text;
        rebuildUI();
    });
    mainLayout->addWidget(searchEdit);

    if (!currentLayer) {
        QLabel* emptyLabel = new QLabel("No layer selected");
        emptyLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(emptyLabel);
        rebuilding = false;
        return;
    }

    QLabel* layerNameLabel = new QLabel(QString("<b>Layer: %1</b>").arg(currentLayer->layerName()));
    mainLayout->addWidget(layerNameLabel);

    bool hasAnyProperties = false;

    auto* summaryGroup = new QGroupBox(QStringLiteral("Summary"));
    auto* summaryLayout = new QVBoxLayout(summaryGroup);
    summaryLayout->setContentsMargins(8, 8, 8, 8);
    summaryLayout->setSpacing(4);

    const std::unordered_set<std::string> keyLayerProperties = {
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
        auto picked = prioritizedSummaryProperties(sortedProps, keyLayerProperties, 4);
        layerSummaryProperties.insert(layerSummaryProperties.end(), picked.begin(), picked.end());
    }

    bool hasSummaryProperties = false;
    addRowsFromProperties(
        summaryGroup,
        summaryLayout,
        layerSummaryProperties,
        filterText,
        [this](const QString& name, const QVariant& value) {
            if (currentLayer) {
                currentLayer->setLayerPropertyValue(name, value);
            }
        },
        &hasSummaryProperties);

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
        effectLabel->setStyleSheet(QStringLiteral("QLabel { color: #9aa7b5; font-weight: bold; }"));
        summaryLayout->addWidget(effectLabel);

        addRowsFromProperties(
            summaryGroup,
            summaryLayout,
            effectSummary,
            filterText,
            [effect](const QString& name, const QVariant& value) {
                effect->setPropertyValue(name, value);
            },
            &hasSummaryProperties);
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

        auto sortedProps = groupDef.sortedProperties();
        bool addedGroupProperties = false;
        addRowsFromProperties(
            group,
            groupLayout,
            sortedProps,
            filterText,
            [this](const QString& name, const QVariant& value) {
                if (currentLayer) {
                    currentLayer->setLayerPropertyValue(name, value);
                }
            },
            &addedGroupProperties);
        if (addedGroupProperties) {
            mainLayout->addWidget(group);
            hasAnyProperties = true;
        } else {
            delete group;
        }
    }

    if (hasFocusedEffect) {
        auto* focusedLabel = new QLabel(QStringLiteral("Focused Effect ID: %1").arg(focusedEffectId));
        focusedLabel->setStyleSheet(QStringLiteral("QLabel { color: #9aa7b5; font-size: 11px; }"));
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
            [effect](const QString& name, const QVariant& value) {
                effect->setPropertyValue(name, value);
            },
            &addedGroupProperties);

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
    rebuilding = false;
}

} // namespace Artifact
