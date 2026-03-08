module;

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QColorDialog>
#include <QGroupBox>
#include <QHBoxLayout>
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

namespace Artifact {

 W_OBJECT_IMPL(ArtifactPropertyWidget)

class ArtifactPropertyWidget::Impl {
public:
    QWidget* containerWidget = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    ArtifactAbstractLayerPtr currentLayer;
    QString filterText;
    bool rebuilding = false;

    void rebuildUI();
};

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

void ArtifactPropertyWidget::clear() {
    impl_->currentLayer = nullptr;
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

    const auto humanizePropertyLabel = [](QString name) {
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
    };

    const auto addPropertyRow = [this, &humanizePropertyLabel](QFormLayout* form,
                                       const ArtifactCore::AbstractProperty& p,
                                       const std::function<void(const QString&, const QVariant&)>& applyValue) {
        const QString query = filterText.trimmed();
        if (!query.isEmpty()) {
            const QString key = p.getName();
            const QString friendly = humanizePropertyLabel(key);
            if (!key.contains(query, Qt::CaseInsensitive) &&
                !friendly.contains(query, Qt::CaseInsensitive)) {
                return;
            }
        }

        const auto meta = p.metadata();
        const QString labelText = meta.displayLabel.isEmpty() ? humanizePropertyLabel(p.getName()) : meta.displayLabel;
        QLabel* label = new QLabel(labelText);
        QWidget* editor = nullptr;

        const QString propName = p.getName();
        const QVariant curVal = p.getValue();

        switch (p.getType()) {
            case ArtifactCore::PropertyType::Float: {
                auto* spin = new QDoubleSpinBox();
                const double minValue = meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6;
                const double maxValue = meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6;
                spin->setRange(minValue, maxValue);
                spin->setValue(curVal.toDouble());
                if (meta.step.isValid()) spin->setSingleStep(meta.step.toDouble());
                if (!meta.unit.isEmpty()) spin->setSuffix(" " + meta.unit);
                editor = spin;
                QObject::connect(spin, &QDoubleSpinBox::editingFinished, [applyValue, propName, spin]() {
                    applyValue(propName, spin->value());
                });
                break;
            }
            case ArtifactCore::PropertyType::Integer: {
                auto* spin = new QSpinBox();
                const int minValue = meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000;
                const int maxValue = meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000;
                spin->setRange(minValue, maxValue);
                spin->setValue(curVal.toInt());
                if (meta.step.isValid()) spin->setSingleStep(meta.step.toInt());
                if (!meta.unit.isEmpty()) spin->setSuffix(" " + meta.unit);
                editor = spin;
                QObject::connect(spin, &QSpinBox::editingFinished, [applyValue, propName, spin]() {
                    applyValue(propName, spin->value());
                });
                break;
            }
            case ArtifactCore::PropertyType::Boolean: {
                auto* cb = new QCheckBox();
                cb->setChecked(curVal.toBool());
                editor = cb;
                QObject::connect(cb, &QCheckBox::toggled, [applyValue, propName](bool checked) {
                    applyValue(propName, checked);
                });
                break;
            }
            case ArtifactCore::PropertyType::Color: {
                auto* btn = new QPushButton(QStringLiteral(" "));
                QColor c = p.getColorValue();
                if (!c.isValid() && curVal.canConvert<QColor>()) {
                    c = curVal.value<QColor>();
                }
                btn->setStyleSheet(QString("background-color: %1").arg(c.isValid() ? c.name() : QStringLiteral("#000000")));
                editor = btn;
                QObject::connect(btn, &QPushButton::clicked, [applyValue, propName, btn, c]() {
                    const QColor newColor = QColorDialog::getColor(c, btn, QStringLiteral("Select Color"));
                    if (!newColor.isValid()) return;
                    btn->setStyleSheet(QString("background-color: %1").arg(newColor.name()));
                    applyValue(propName, newColor);
                });
                break;
            }
            case ArtifactCore::PropertyType::String: {
                auto* line = new QLineEdit(curVal.toString());
                editor = line;
                QObject::connect(line, &QLineEdit::editingFinished, [applyValue, propName, line]() {
                    applyValue(propName, line->text());
                });
                break;
            }
            default:
                editor = new QLabel(curVal.toString());
                break;
        }

        if (!editor) return;
        if (!meta.tooltip.isEmpty()) {
            label->setToolTip(meta.tooltip);
            editor->setToolTip(meta.tooltip);
        }

        QWidget* rowWidget = new QWidget();
        QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        rowLayout->addWidget(editor);

        QPushButton* copilotBtn = new QPushButton(QString::fromUtf8("✨"));
        copilotBtn->setToolTip("Expression Copilot");
        copilotBtn->setFixedSize(24, 24);
        QObject::connect(copilotBtn, &QPushButton::clicked, [propName]() {
            auto copilot = new ArtifactExpressionCopilotWidget();
            copilot->setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::Tool);
            copilot->setWindowTitle("Expression Copilot: " + propName);
            copilot->setAttribute(Qt::WA_DeleteOnClose);
            copilot->move(QCursor::pos() - QPoint(150, 200));
            copilot->show();
        });
        rowLayout->addWidget(copilotBtn);
        form->addRow(label, rowWidget);
    };

    bool hasAnyProperties = false;

    const auto layerGroups = currentLayer->getLayerPropertyGroups();
    for (const auto& groupDef : layerGroups) {
        QGroupBox* group = new QGroupBox(groupDef.name().isEmpty() ? QStringLiteral("Layer") : groupDef.name());
        QFormLayout* form = new QFormLayout(group);
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto sortedProps = groupDef.sortedProperties();
        for (const auto& ptr : sortedProps) {
            if (!ptr) continue;
            addPropertyRow(form, *ptr, [this](const QString& name, const QVariant& value) {
                if (currentLayer) {
                    currentLayer->setLayerPropertyValue(name, value);
                }
            });
            hasAnyProperties = true;
        }
        mainLayout->addWidget(group);
    }

    const auto effects = currentLayer->getEffects();
    for (const auto& effect : effects) {
        if (!effect) continue;

        QString stageName = "Unknown";
        switch (effect->pipelineStage()) {
            case EffectPipelineStage::Generator: stageName = "[Generator]"; break;
            case EffectPipelineStage::GeometryTransform: stageName = "[Geo Transform]"; break;
            case EffectPipelineStage::MaterialRender: stageName = "[Material]"; break;
            case EffectPipelineStage::Rasterizer: stageName = "[Rasterizer]"; break;
            case EffectPipelineStage::LayerTransform: stageName = "[Layer Transform]"; break;
        }

        QGroupBox* group = new QGroupBox(QString("%1 %2").arg(stageName).arg(effect->displayName().toQString()));
        QFormLayout* form = new QFormLayout(group);
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
        for (const auto& p : effect->getProperties()) {
            propGroup.addProperty(std::make_shared<ArtifactCore::AbstractProperty>(p));
        }

        auto sortedProps = propGroup.sortedProperties();
        for (const auto& ptr : sortedProps) {
            if (!ptr) continue;
            addPropertyRow(form, *ptr, [effect](const QString& name, const QVariant& value) {
                effect->setPropertyValue(name, value);
            });
            hasAnyProperties = true;
        }

        mainLayout->addWidget(group);
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
