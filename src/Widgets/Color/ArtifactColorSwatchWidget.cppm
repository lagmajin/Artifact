module;
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QModelIndex>
#include <QToolTip>
#include <wobjectimpl.h>

module Artifact.Widgets.ColorSwatchWidget;

import Color.Swatch;
import Color.Float;

namespace Artifact {

W_OBJECT_IMPL(ArtifactColorSwatchWidget)

class ArtifactColorSwatchWidget::Impl {
public:
    ArtifactCore::ColorSwatch swatch;
    QListWidget* listWidget = nullptr;
    QPushButton* btnLoad = nullptr;
    QPushButton* btnSave = nullptr;
    QPushButton* btnClear = nullptr;

    Impl() : swatch("New Palette") {}
};

ArtifactColorSwatchWidget::ArtifactColorSwatchWidget(QWidget* parent)
    : QWidget(parent), impl_(std::unique_ptr<Impl>(new Impl())) {
    
    // UI Setup
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Grid View
    impl_->listWidget = new QListWidget(this);
    impl_->listWidget->setViewMode(QListView::IconMode);
    impl_->listWidget->setResizeMode(QListView::Adjust);
    impl_->listWidget->setSpacing(4);
    impl_->listWidget->setWrapping(true);
    impl_->listWidget->setIconSize(QSize(32, 32));
    impl_->listWidget->setMovement(QListView::Static);
    impl_->listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->listWidget->setStyleSheet("QListWidget { background-color: #2b2b2b; border: 1px solid #333; outline: none; }"
                                    "QListWidget::item { border: 1px solid transparent; }"
                                    "QListWidget::item:selected { border: 2px solid #0078d7; background: transparent; }");
    
    mainLayout->addWidget(impl_->listWidget);

    // Toolbar
    auto* toolLayout = new QHBoxLayout();
    impl_->btnLoad = new QPushButton("Import .gpl", this);
    impl_->btnSave = new QPushButton("Export .gpl", this);
    impl_->btnClear = new QPushButton("Clear", this);

    toolLayout->addWidget(impl_->btnLoad);
    toolLayout->addWidget(impl_->btnSave);
    toolLayout->addWidget(impl_->btnClear);
    mainLayout->addLayout(toolLayout);

    // Signals
    connect(impl_->btnLoad, &QPushButton::clicked, this, &ArtifactColorSwatchWidget::onLoadGPL);
    connect(impl_->btnSave, &QPushButton::clicked, this, &ArtifactColorSwatchWidget::onSaveGPL);
    connect(impl_->btnClear, &QPushButton::clicked, this, &ArtifactColorSwatchWidget::onClear);
    
    connect(impl_->listWidget, &QListWidget::itemClicked, [this](QListWidgetItem* item) {
        if (!item) return;
        int idx = impl_->listWidget->row(item);
        if (idx >= 0 && idx < (int)impl_->swatch.count()) {
            Q_EMIT colorSelected(impl_->swatch.at(idx).color);
        }
    });

}

ArtifactColorSwatchWidget::~ArtifactColorSwatchWidget() = default;

void ArtifactColorSwatchWidget::setSwatch(const ArtifactCore::ColorSwatch& sw) {
    impl_->swatch = sw;
    updateListView();
    Q_EMIT swatchChanged();
}

const ArtifactCore::ColorSwatch& ArtifactColorSwatchWidget::getSwatch() const {
    return impl_->swatch;
}

void ArtifactColorSwatchWidget::updateListView() {
    impl_->listWidget->clear();
    for (size_t i = 0; i < impl_->swatch.count(); ++i) {
        const auto& entry = impl_->swatch.at(i);
        
        QPixmap pix(32, 32);
        pix.fill(QColor::fromRgbF(entry.color.r(), entry.color.g(), entry.color.b(), entry.color.a()));
        
        QPainter p(&pix);
        p.setPen(QColor(40, 40, 40));
        p.drawRect(0, 0, 31, 31);
        p.end();

        auto* item = new QListWidgetItem(QIcon(pix), "", impl_->listWidget);
        item->setSizeHint(QSize(36, 36));
        if (!entry.name.empty()) {
            item->setToolTip(QString::fromStdString(entry.name));
        } else {
            item->setToolTip(QString("R: %1, G: %2, B: %3").arg(entry.color.r()).arg(entry.color.g()).arg(entry.color.b()));
        }
    }
}

void ArtifactColorSwatchWidget::onLoadGPL() {
    QString path = QFileDialog::getOpenFileName(this, "Import GIMP Palette", "", "Palettes (*.gpl);;All Files (*.*)");
    if (!path.isEmpty()) {
        if (impl_->swatch.importGPL(path.toStdString())) {
            updateListView();
            Q_EMIT swatchChanged();
        }
    }
}

void ArtifactColorSwatchWidget::onSaveGPL() {
     QString path = QFileDialog::getSaveFileName(this, "Export GIMP Palette", "", "Palettes (*.gpl);;All Files (*.*)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".gpl", Qt::CaseInsensitive)) path += ".gpl";
        impl_->swatch.exportGPL(path.toStdString());
    }
}

void ArtifactColorSwatchWidget::onClear() {
    impl_->swatch.clear();
    updateListView();
    Q_EMIT swatchChanged();
}

void ArtifactColorSwatchWidget::onColorDoubleClicked(const QModelIndex& index) {
    // 将来的にはここで色編集ダイアログを開くなどの処理
}

} // namespace Artifact

//#include <moc_ArtifactColorSwatchWidget.cpp>
