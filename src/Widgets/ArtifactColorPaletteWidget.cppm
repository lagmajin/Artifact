module;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QColorDialog>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>

// Q_MOC_RUN skip workaround
#ifndef Q_MOC_RUN
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
module Artifact.Widgets.ColorPaletteWidget;




import Artifact.Color.Palette;
import Color.Harmonizer;
import Artifact.Project.PresetManager;
import Color.Float;
import Analyze.SmartPalette;
#endif

namespace Artifact {

using namespace ArtifactCore;

// Custom delegate to draw colored palettes in the list
class PaletteItemDelegate : public QStyledItemDelegate {
public:
    PaletteItemDelegate(std::shared_ptr<ColorPaletteManager> manager, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), manager_(manager) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);

        if (!manager_) return;

        QString name = index.data(Qt::DisplayRole).toString();
        ColorPalette* p = manager_->getPalette(name);
        if (!p || p->colors.isEmpty()) return;

        painter->save();
        
        // Draw the colors as a small horizontal bar sequence right of the text
        QRect r = option.rect;
        int colorBoxWidth = 20;
        int colorBoxHeight = r.height() - 4;
        
        int startX = r.right() - (colorBoxWidth * p->colors.size()) - 5;
        int startY = r.top() + 2;

        for (int i = 0; i < p->colors.size(); ++i) {
            QRect boxRect(startX + i * colorBoxWidth, startY, colorBoxWidth, colorBoxHeight);
            painter->fillRect(boxRect, p->colors[i].color);
            painter->setPen(Qt::black);
            painter->drawRect(boxRect);
        }

        painter->restore();
    }
private:
    std::shared_ptr<ColorPaletteManager> manager_;
};


class ArtifactColorPaletteWidget::Impl {
public:
    std::shared_ptr<ColorPaletteManager> manager;
    
    QListWidget* listWidget = nullptr;
    QPushButton* btnGenerate = nullptr;
    QPushButton* btnExtract = nullptr;
    QPushButton* btnDelete = nullptr;
    QPushButton* btnLoad = nullptr;
    QPushButton* btnSave = nullptr;
    QComboBox* comboHarmony = nullptr;
};

ArtifactColorPaletteWidget::ArtifactColorPaletteWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) 
{
    // Default manager fallback if none is provided
    impl_->manager = std::make_shared<ColorPaletteManager>();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // List View
    impl_->listWidget = new QListWidget(this);
    impl_->listWidget->setItemDelegate(new PaletteItemDelegate(impl_->manager, impl_->listWidget));
    mainLayout->addWidget(impl_->listWidget);

    // Toolbar for Harmony Generation
    QHBoxLayout* genLayout = new QHBoxLayout();
    impl_->comboHarmony = new QComboBox(this);
    impl_->comboHarmony->addItems({
        "Complementary", "Analogous", "Triadic", "Split-Complementary", "Tetradic", "Monochromatic"
    });
    impl_->btnGenerate = new QPushButton("Generate Harmony...", this);
    impl_->btnExtract = new QPushButton("Smart Extract...", this);
    
    connect(impl_->btnGenerate, &QPushButton::clicked, this, &ArtifactColorPaletteWidget::onGenerateHarmonicPalette);
    connect(impl_->btnExtract, &QPushButton::clicked, this, &ArtifactColorPaletteWidget::onSmartExtractPalette);

    genLayout->addWidget(impl_->comboHarmony);
    genLayout->addWidget(impl_->btnGenerate);
    genLayout->addWidget(impl_->btnExtract);
    mainLayout->addLayout(genLayout);

    // Actions
    QHBoxLayout* actionLayout = new QHBoxLayout();
    impl_->btnDelete = new QPushButton("Delete", this);
    impl_->btnLoad = new QPushButton("Load Preset", this);
    impl_->btnSave = new QPushButton("Save Preset", this);

    actionLayout->addWidget(impl_->btnDelete);
    actionLayout->addWidget(impl_->btnLoad);
    actionLayout->addWidget(impl_->btnSave);
    mainLayout->addLayout(actionLayout);

    connect(impl_->btnDelete, &QPushButton::clicked, [this]() {
        QListWidgetItem* item = impl_->listWidget->currentItem();
        if (item) {
            impl_->manager->removePalette(item->text());
            updatePaletteList();
        }
    });

    connect(impl_->btnLoad, &QPushButton::clicked, this, &ArtifactColorPaletteWidget::onLoadPalettes);
    connect(impl_->btnSave, &QPushButton::clicked, this, &ArtifactColorPaletteWidget::onSavePalettes);
}

ArtifactColorPaletteWidget::~ArtifactColorPaletteWidget() {
    delete impl_;
}

void ArtifactColorPaletteWidget::setPaletteManager(std::shared_ptr<ColorPaletteManager> manager) {
    if (manager) {
        impl_->manager = manager;
        // Delegate also needs the new manager reference, we simply recreate it
        impl_->listWidget->setItemDelegate(new PaletteItemDelegate(impl_->manager, impl_->listWidget));
        updatePaletteList();
    }
}

void ArtifactColorPaletteWidget::updatePaletteList() {
    impl_->listWidget->clear();
    QStringList names = impl_->manager->paletteNames();
    for (const QString& n : names) {
        impl_->listWidget->addItem(n);
    }
}

void ArtifactColorPaletteWidget::onGenerateHarmonicPalette() {
    QColor base = QColorDialog::getColor(Qt::red, this, "Select Base Color");
    if (!base.isValid()) return;

    // Convert to FloatColor for ColorHarmonizer
    FloatColor floatBase(base.redF(), base.greenF(), base.blueF(), base.alphaF());
    QList<FloatColor> harmonyColors;

    int idx = impl_->comboHarmony->currentIndex();
    QString prefix;

    switch (idx) {
        case 0:
            harmonyColors.append(floatBase);
            harmonyColors.append(ColorHarmonizer::getComplementary(floatBase));
            prefix = "Complementary";
            break;
        case 1:
            harmonyColors = ColorHarmonizer::getAnalogous(floatBase);
            harmonyColors.prepend(floatBase);
            prefix = "Analogous";
            break;
        case 2:
            harmonyColors = ColorHarmonizer::getTriadic(floatBase);
            harmonyColors.prepend(floatBase);
            prefix = "Triadic";
            break;
        case 3:
            harmonyColors = ColorHarmonizer::getSplitComplementary(floatBase);
            harmonyColors.prepend(floatBase);
            prefix = "SplitComplementary";
            break;
        case 4:
            harmonyColors = ColorHarmonizer::getTetradic(floatBase);
            harmonyColors.prepend(floatBase);
            prefix = "Tetradic";
            break;
        case 5:
            harmonyColors = ColorHarmonizer::getMonochromatic(floatBase, 4);
            prefix = "Monochromatic";
            // getMonochromatic returns only varied lightness colors, we may want to include base if it's not in the list, but it's generated dynamically.
            break;
    }

    bool ok;
    QString presetName = QInputDialog::getText(this, "Palette Name", "Enter a name for the new palette:", QLineEdit::Normal, prefix + " Palette", &ok);
    
    if (ok && !presetName.isEmpty()) {
        ColorPalette palette;
        palette.name = presetName;

        for (int i = 0; i < harmonyColors.size(); ++i) {
            NamedColor nc;
            nc.name = QString("Color %1").arg(i + 1);
            nc.color = QColor::fromRgbF(harmonyColors[i].r(), harmonyColors[i].g(), harmonyColors[i].b(), harmonyColors[i].a());
            palette.colors.append(nc);
        }

        impl_->manager->addPalette(palette);
        updatePaletteList();
    }
}

void ArtifactColorPaletteWidget::onSmartExtractPalette() {
    QString path = QFileDialog::getOpenFileName(this, "Extract Palette from Image", "", "Images (*.png *.jpg *.bmp *.jpeg)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) return;

    // Use the core analyzer (OpenCV based)
    auto floatPalette = SmartPaletteAnalyzer::extractPalette(img, 5);
    
    if (floatPalette.empty()) {
        QMessageBox::warning(this, "Error", "Failed to extract colors from image.");
        return;
    }

    bool ok;
    QString presetName = QInputDialog::getText(this, "Palette Name", "Enter a name for the extracted palette:", QLineEdit::Normal, "Smart Palette", &ok);
    
    if (ok && !presetName.isEmpty()) {
        ColorPalette palette;
        palette.name = presetName;

        for (size_t i = 0; i < floatPalette.size(); ++i) {
            NamedColor nc;
            nc.name = QString("Dominant %1").arg(i + 1);
            nc.color = QColor::fromRgbF(floatPalette[i].r(), floatPalette[i].g(), floatPalette[i].b(), floatPalette[i].a());
            palette.colors.append(nc);
        }

        impl_->manager->addPalette(palette);
        updatePaletteList();
    }
}

void ArtifactColorPaletteWidget::onLoadPalettes() {
    QString path = QFileDialog::getOpenFileName(this, "Load Color Palettes", "", "JSON Files (*.json)");
    if (!path.isEmpty()) {
        if (ArtifactPresetManager::loadColorPaletteMapping(*impl_->manager, path)) {
            updatePaletteList();
        } else {
            QMessageBox::warning(this, "Error", "Failed to load color palettes.");
        }
    }
}

void ArtifactColorPaletteWidget::onSavePalettes() {
    QString path = QFileDialog::getSaveFileName(this, "Save Color Palettes", "", "JSON Files (*.json)");
    if (!path.isEmpty()) {
        if (!ArtifactPresetManager::saveColorPaletteMapping(*impl_->manager, path)) {
            QMessageBox::warning(this, "Error", "Failed to save color palettes.");
        }
    }
}

} // namespace Artifact
