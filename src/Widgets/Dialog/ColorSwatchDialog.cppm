module;
#include <utility>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QScrollArea>
#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QSizePolicy>
#include <QGridLayout>
#include <QToolTip>
#include <QShowEvent>
#include <QPalette>
#include <wobjectimpl.h>

module Artifact.Widgets.ColorSwatchDialog;

import std;
import Color.Float;
import Widgets.Utils.CSS;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(ColorSwatchDialog)

// ---------------------------------------------------------------------------
// Swatch data
// ---------------------------------------------------------------------------

struct SwatchEntry {
    QString name;
    FloatColor  color;
    bool    isChecker    = false;
    int     checkerDensity = 1;
};

static const std::vector<SwatchEntry> kBasicColors = {
    { "ホワイト",     FloatColor(1.0f, 1.0f, 1.0f, 1.0f) },
    { "ライトグレー1",FloatColor(0.866f, 0.866f, 0.866f, 1.0f) },
    { "ライトグレー2",FloatColor(0.667f, 0.667f, 0.667f, 1.0f) },
    { "グレー",       FloatColor(0.467f, 0.467f, 0.467f, 1.0f) },
    { "ダークグレー", FloatColor(0.267f, 0.267f, 0.267f, 1.0f) },
    { "ブラック",     FloatColor(0.0f, 0.0f, 0.0f, 1.0f) },
    { "レッド",       FloatColor(1.0f, 0.0f, 0.0f, 1.0f) },
    { "グリーン",     FloatColor(0.0f, 1.0f, 0.0f, 1.0f) },
    { "ブルー",       FloatColor(0.0f, 0.0f, 1.0f, 1.0f) },
    { "シアン",       FloatColor(0.0f, 1.0f, 1.0f, 1.0f) },
    { "マゼンタ",     FloatColor(1.0f, 0.0f, 1.0f, 1.0f) },
    { "イエロー",     FloatColor(1.0f, 1.0f, 0.0f, 1.0f) },
};

static const std::vector<SwatchEntry> kVideoColors = {
    { "ホワイト",       FloatColor(1.0f, 1.0f, 1.0f, 1.0f) },
    { "ブラック",       FloatColor(0.0f, 0.0f, 0.0f, 1.0f) },
    { "レッド",         FloatColor(1.0f, 0.0f, 0.0f, 1.0f) },
    { "ブルー",         FloatColor(0.0f, 0.0f, 1.0f, 1.0f) },
    { "スキン",         FloatColor(1.0f, 0.8f, 0.6f, 1.0f) },
    { "ブラウン",       FloatColor(0.6f, 0.4f, 0.2f, 1.0f) },
    { "ライトブルー",   FloatColor(0.4f, 0.67f, 1.0f, 1.0f) },
    { "グリーン",       FloatColor(0.0f, 0.6f, 0.0f, 1.0f) },
    { "オレンジ",       FloatColor(1.0f, 0.53f, 0.0f, 1.0f) },
    { "シアン",         FloatColor(0.0f, 0.8f, 1.0f, 1.0f) },
};

static const std::vector<SwatchEntry> kTransparentColors = {
    { "透明1", FloatColor(1.0f, 1.0f, 1.0f, 0.0f),   true, 1 },
    { "透明2", FloatColor(1.0f, 1.0f, 1.0f, 0.25f),  true, 2 },
    { "透明3", FloatColor(0.78f, 0.90f, 1.0f, 0.50f), true, 3 },
    { "透明4", FloatColor(1.0f, 0.86f, 0.71f, 0.38f),  true, 4 },
    { "透明5", FloatColor(0.71f, 1.0f, 0.71f, 0.25f),  true, 5 },
    { "透明6", FloatColor(1.0f, 1.0f, 1.0f, 0.13f),  true, 6 },
};

// ---------------------------------------------------------------------------
// SwatchCell – a single clickable 32×32 swatch widget
// ---------------------------------------------------------------------------

class SwatchCell : public QFrame {
public:
    SwatchEntry entry;
    bool selected = false;
    std::function<void(const SwatchEntry&)> onSelected;
    std::function<void(const SwatchEntry&)> onDoubleClicked;

    explicit SwatchCell(const SwatchEntry& e, QWidget* parent = nullptr)
        : QFrame(parent), entry(e)
    {
        setFixedSize(32, 32);
        setToolTip(e.name);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        QRect r = rect().adjusted(1, 1, -1, -1);

        if (entry.isChecker) {
            // Draw checkerboard
            const int cellSz = std::max(4, 10 - entry.checkerDensity);
            for (int y = r.top(); y < r.bottom(); y += cellSz) {
                for (int x = r.left(); x < r.right(); x += cellSz) {
                    bool light = ((x / cellSz + y / cellSz) % 2 == 0);
                    p.fillRect(x, y, cellSz, cellSz,
                               light ? QColor(200,200,200) : QColor(100,100,100));
                }
            }
            // Color overlay
            p.fillRect(r, QColor::fromRgbF(entry.color.r(), entry.color.g(), entry.color.b(), entry.color.a()));
        } else {
            p.fillRect(r, QColor::fromRgbF(entry.color.r(), entry.color.g(), entry.color.b(), entry.color.a()));
        }

        // Border
        if (selected) {
            p.setPen(QPen(QColor("#4af"), 2));
        } else {
            p.setPen(QPen(QColor("#555"), 1));
        }
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }

    void mousePressEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton && onSelected)
            onSelected(entry);
        QFrame::mousePressEvent(ev);
    }

    void mouseDoubleClickEvent(QMouseEvent* ev) override
    {
        if (ev->button() == Qt::LeftButton && onDoubleClicked)
            onDoubleClicked(entry);
        QFrame::mouseDoubleClickEvent(ev);
    }
};

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct Category {
    QString               name;
    std::vector<SwatchEntry> entries;
    QWidget*              contentWidget = nullptr;
    QPushButton*          headerBtn     = nullptr;
    bool                  expanded      = true;
    std::vector<SwatchCell*> cells;
};

class ColorSwatchDialog::Impl {
public:
    std::vector<Category> categories;
    FloatColor            selectedColor;
    FloatColor            initialColor;

    int                   selectedCategory = -1;
    int                   selectedIndex    = -1;

    QFrame*               previewSquare  = nullptr;
    QLabel*               colorNameLabel = nullptr;
    QLabel*               colorInfoLabel = nullptr;

    void updateInfoPanel(const SwatchEntry& e)
    {
        QColor c = QColor::fromRgbF(e.color.r(), e.color.g(), e.color.b(), e.color.a());
        QString hex = c.name().toUpper();
        float a = c.alphaF();
        float r = c.redF();
        float g = c.greenF();
        float b = c.blueF();

        colorNameLabel->setText(e.name);
        colorInfoLabel->setText(
            QString("%1  A:%2  R:%3  G:%4  B:%5")
                .arg(hex)
                .arg(a, 0, 'f', 3)
                .arg(r, 0, 'f', 3)
                .arg(g, 0, 'f', 3)
                .arg(b, 0, 'f', 3));

        previewSquare->setAutoFillBackground(true);
        QPalette pal = previewSquare->palette();
        pal.setColor(QPalette::Window, c);
        pal.setColor(QPalette::Base, c);
        previewSquare->setPalette(pal);

        selectedColor = FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF());
    }

    void clearSelection()
    {
        for (auto& cat : categories)
            for (auto* cell : cat.cells)
                cell->selected = false;
    }
};

// ---------------------------------------------------------------------------
// ColorSwatchDialog
// ---------------------------------------------------------------------------

static QPushButton* makeToolBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(24, 24);
    btn->setFlat(true);
    return btn;
}

static QWidget* buildSection(Category& cat,
                              std::function<void(int catIdx, int idx, const SwatchEntry&)> onSelect,
                              std::function<void(int catIdx, int idx, const SwatchEntry&)> onDbl,
                              int catIdx)
{
    auto* wrapper = new QWidget();
    auto* wl = new QVBoxLayout(wrapper);
    wl->setContentsMargins(0, 0, 0, 0);
    wl->setSpacing(0);

    // Header button
    cat.headerBtn = new QPushButton(cat.name + " ▼");
    cat.headerBtn->setFlat(true);
    cat.headerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cat.headerBtn->setFixedHeight(26);
    wl->addWidget(cat.headerBtn);

    // Content widget
    cat.contentWidget = new QWidget();
    cat.contentWidget->setAutoFillBackground(true);
    auto* grid = new QGridLayout(cat.contentWidget);
    grid->setSpacing(4);
    grid->setContentsMargins(8, 6, 8, 6);

    int col = 0, row = 0;
    int idx = 0;
    for (auto& entry : cat.entries) {
        auto* cell = new SwatchCell(entry, cat.contentWidget);
        int capturedCat = catIdx;
        int capturedIdx = idx;
        cell->onSelected      = [=](const SwatchEntry& e){ onSelect(capturedCat, capturedIdx, e); };
        cell->onDoubleClicked = [=](const SwatchEntry& e){ onDbl(capturedCat, capturedIdx, e); };
        grid->addWidget(cell, row, col++);
        cat.cells.push_back(cell);
        if (col >= 8) { col = 0; row++; }
        ++idx;
    }

    // [+] add button
    auto* addBtn = new QPushButton("+");
    addBtn->setFixedSize(32, 32);
    addBtn->setToolTip("カラーを追加");
    grid->addWidget(addBtn, row, col);

    wl->addWidget(cat.contentWidget);

    // Toggle collapse
    QObject::connect(cat.headerBtn, &QPushButton::clicked, [&cat]() {
        cat.expanded = !cat.expanded;
        cat.contentWidget->setVisible(cat.expanded);
        QString arrow = cat.expanded ? " ▼" : " ▶";
        // Strip old arrow and append new
        QString t = cat.headerBtn->text();
        if (t.endsWith(" ▼") || t.endsWith(" ▶"))
            t.chop(2);
        cat.headerBtn->setText(t + arrow);
    });

    return wrapper;
}

ColorSwatchDialog::ColorSwatchDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle("カラースウォッチ");
    setFixedSize(520, 560);
    setAutoFillBackground(true);

    impl_->categories = {
        { "基本カラー",   kBasicColors,       nullptr, nullptr, true, {} },
        { "映像制作",     kVideoColors,       nullptr, nullptr, true, {} },
        { "透明・グロー", kTransparentColors, nullptr, nullptr, true, {} },
    };

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(2);
    toolbar->addWidget(makeToolBtn("⠿", this));
    toolbar->addWidget(makeToolBtn("•", this));
    toolbar->addWidget(makeToolBtn("+", this));
    toolbar->addWidget(makeToolBtn("×", this));
    toolbar->addWidget(makeToolBtn("↑", this));
    toolbar->addWidget(makeToolBtn("↓", this));
    toolbar->addStretch();
    auto* libLabel = new QLabel("AE Default Library");
    {
        QPalette pal = libLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        libLabel->setPalette(pal);
    }
    toolbar->addWidget(libLabel);
    mainLayout->addLayout(toolbar);

    // ── Scroll area with sections ─────────────────────────────────────────
    auto* scrollArea  = new QScrollArea(this);
    auto* scrollWidget = new QWidget();
    scrollWidget->setAutoFillBackground(true);
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(0);

    for (int i = 0; i < (int)impl_->categories.size(); ++i) {
        auto& cat = impl_->categories[i];

        auto onSelect = [this, &cat](int catIdx, int idx, const SwatchEntry& e) {
            impl_->clearSelection();
            if (catIdx >= 0 && catIdx < (int)impl_->categories.size()) {
                auto& c = impl_->categories[catIdx];
                if (idx >= 0 && idx < (int)c.cells.size())
                    c.cells[idx]->selected = true;
            }
            impl_->selectedCategory = catIdx;
            impl_->selectedIndex    = idx;
            impl_->updateInfoPanel(e);
            // Repaint all cells
            for (auto& c2 : impl_->categories)
                for (auto* cell : c2.cells)
                    cell->update();
        };

        auto onDbl = [this](int /*catIdx*/, int /*idx*/, const SwatchEntry& e) {
            impl_->updateInfoPanel(e);
            Q_EMIT colorApplied(impl_->selectedColor);
            accept();
        };

        auto* sectionWidget = buildSection(cat, onSelect, onDbl, i);
        scrollLayout->addWidget(sectionWidget);
    }
    scrollLayout->addStretch();

    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(scrollArea, 1);

    // ── Separator ────────────────────────────────────────────────────────
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    {
      QPalette pal = sep->palette();
      pal.setColor(QPalette::WindowText, QColor("#444"));
      sep->setPalette(pal);
    }
    mainLayout->addWidget(sep);

    // ── Info panel ───────────────────────────────────────────────────────
    auto* infoPanel = new QWidget();
    infoPanel->setFixedHeight(72);
    auto* infoLayout = new QHBoxLayout(infoPanel);
    infoLayout->setContentsMargins(4, 4, 4, 4);
    infoLayout->setSpacing(10);

    impl_->previewSquare = new QFrame();
    impl_->previewSquare->setFixedSize(48, 48);
    impl_->previewSquare->setAutoFillBackground(true);
    infoLayout->addWidget(impl_->previewSquare);

    auto* textCol = new QVBoxLayout();
    impl_->colorNameLabel = new QLabel("—");
    {
        QPalette pal = impl_->colorNameLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
        impl_->colorNameLabel->setPalette(pal);
    }
    impl_->colorInfoLabel = new QLabel("");
    {
        QPalette pal = impl_->colorInfoLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        impl_->colorInfoLabel->setPalette(pal);
    }
    textCol->addWidget(impl_->colorNameLabel);
    textCol->addWidget(impl_->colorInfoLabel);
    textCol->addStretch();
    infoLayout->addLayout(textCol, 1);

    mainLayout->addWidget(infoPanel);

    // ── Button row ────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto* closeBtn = new QPushButton("閉じる");
    closeBtn->setFixedSize(80, 28);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(closeBtn);

    auto* applyBtn = new QPushButton("適用 ↗");
    applyBtn->setFixedSize(80, 28);
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT colorApplied(impl_->selectedColor);
        accept();
    });
    btnRow->addWidget(applyBtn);

    mainLayout->addLayout(btnRow);

    // Pre-select first swatch
    if (!impl_->categories.empty() && !impl_->categories[0].entries.empty()) {
        impl_->updateInfoPanel(impl_->categories[0].entries[0]);
        impl_->selectedCategory = 0;
        impl_->selectedIndex    = 0;
        if (!impl_->categories[0].cells.empty())
            impl_->categories[0].cells[0]->selected = true;
    }
}

ColorSwatchDialog::~ColorSwatchDialog()
{
    delete impl_;
}

void ColorSwatchDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    } else {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect sg = screen->availableGeometry();
            move(sg.center().x() - width() / 2, sg.center().y() - height() / 2);
        }
    }
}

FloatColor ColorSwatchDialog::selectedColor() const
{
    return impl_->selectedColor;
}

void ColorSwatchDialog::setCurrentColor(const FloatColor& color)
{
    impl_->initialColor  = color;
    impl_->selectedColor = color;
}

} // namespace Artifact
