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

module Artifact.Widgets.ArtifactPropertyWidget;

import std;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Property.Group;
import Undo.UndoManager;
import Artifact.Effect.Abstract;
import Utils.String.UniString;

namespace Artifact {

class ArtifactPropertyWidget::Impl {
public:
    QWidget* containerWidget = nullptr;
    QVBoxLayout* mainLayout = nullptr;
    ArtifactAbstractLayerPtr currentLayer;

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
    impl_->currentLayer = layer;
    updateProperties();
}

void ArtifactPropertyWidget::clear() {
    impl_->currentLayer = nullptr;
    updateProperties();
}

void ArtifactPropertyWidget::updateProperties() {
    impl_->rebuildUI();
}

void ArtifactPropertyWidget::Impl::rebuildUI() {
    // 古いウィジェットをクリア
    QLayoutItem* child;
    while ((child = mainLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    if (!currentLayer) {
        QLabel* emptyLabel = new QLabel("No layer selected");
        emptyLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(emptyLabel);
        return;
    }

    // レイヤー名を最上部に表示
    QLabel* layerNameLabel = new QLabel(QString("<b>Layer: %1</b>").arg(currentLayer->layerName()));
    mainLayout->addWidget(layerNameLabel);

    // エフェクトからプロパティを取得
    auto effects = currentLayer->getEffects();
    
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

        auto props = effect->getProperties();

        // PropertyGroupを使って表示優先度（displayPriority）のテストも兼ねる
        ArtifactCore::PropertyGroup propGroup(effect->displayName().toQString());
        
        for (const auto& p : props) {
            auto ptr = std::make_shared<ArtifactCore::AbstractProperty>(p);
            propGroup.addProperty(ptr);
        }

        // 表示優先度順にソートしてUI生成
        auto sortedProps = propGroup.sortedProperties();

        for (const auto& ptr : sortedProps) {
            const auto& p = *ptr;
            QLabel* label = new QLabel(p.getName().toQString());
            QWidget* editor = nullptr;
            
            auto type = p.getType();
            QVariant curVal = p.getValue();

            // Store copies for capturing in lambdas
            auto effectRef = effect;
            ArtifactCore::UniString propName = p.getName();

            switch (type) {
                case ArtifactCore::PropertyType::Float: {
                    auto* spin = new QDoubleSpinBox();
                    spin->setRange(-1e6, 1e6);
                    spin->setValue(curVal.toDouble());
                    editor = spin;
                    
                    QObject::connect(spin, &QDoubleSpinBox::editingFinished, [effectRef, propName, spin]() {
                        effectRef->setPropertyValue(propName, spin->value());
                    });
                    break;
                }
                case ArtifactCore::PropertyType::Integer: {
                    auto* spin = new QSpinBox();
                    spin->setRange(-1e6, 1e6);
                    spin->setValue(curVal.toInt());
                    editor = spin;
                    
                    QObject::connect(spin, &QSpinBox::editingFinished, [effectRef, propName, spin]() {
                        effectRef->setPropertyValue(propName, spin->value());
                    });
                    break;
                }
                case ArtifactCore::PropertyType::Boolean: {
                    auto* cb = new QCheckBox();
                    cb->setChecked(curVal.toBool());
                    editor = cb;
                    
                    QObject::connect(cb, &QCheckBox::clicked, [effectRef, propName, cb](bool checked) {
                        effectRef->setPropertyValue(propName, checked);
                    });
                    break;
                }
                case ArtifactCore::PropertyType::Color: {
                    auto* btn = new QPushButton("");
                    QColor c = p.getColorValue();
                    btn->setStyleSheet(QString("background-color: %1").arg(c.name()));
                    editor = btn;
                    
                    QObject::connect(btn, &QPushButton::clicked, [effectRef, propName, btn]() {
                        // In a real Qt setup we usually exec QColorDialog
                        // Placeholder just to show the binding structure
                        // QColor newColor = QColorDialog::getColor(...);
                        // effectRef->setPropertyValue(propName, newColor);
                    });
                    break;
                }
                default:
                    editor = new QLabel(curVal.toString());
                    break;
            }
            if (editor) {
                form->addRow(label, editor);
            }
        }
        mainLayout->addWidget(group);
    }
    
    mainLayout->addStretch();
}

} // namespace Artifact
