module;
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <wobjectimpl.h>

module Artifact.Widgets.RenderQueuePresetSelector;

import std;

import Artifact.Render.Queue.Presets;

namespace Artifact {

class ArtifactRenderQueuePresetSelector::Impl {
public:
    QComboBox* categoryCombo = nullptr;
    QListWidget* presetListWidget = nullptr;
    QLabel* descriptionLabel = nullptr;
    ArtifactRenderFormatCategory currentCategory = ArtifactRenderFormatCategory::Video;
    bool multiSelectMode = false;
    
    void loadPresets() {
        if (!presetListWidget) return;
        
        presetListWidget->clear();
        const auto presets = ArtifactRenderFormatPresetManager::instance().presetsByCategory(currentCategory);
        
        for (const auto& preset : presets) {
            auto* item = new QListWidgetItem();
            item->setText(preset.name);
            item->setData(Qt::UserRole, preset.id);
            item->setToolTip(preset.description);
            
            // アイコン（簡易）
            if (preset.isImageSequence) {
                item->setIcon(QIcon::fromTheme("image-x-generic"));
            } else {
                item->setIcon(QIcon::fromTheme("video-x-generic"));
            }
            
            presetListWidget->addItem(item);
        }
        
        if (presetListWidget->count() > 0) {
            presetListWidget->setCurrentRow(0);
            updateDescription();
        }
    }
    
    void updateDescription() {
        if (!presetListWidget || !descriptionLabel) return;
        
        const auto* currentItem = presetListWidget->currentItem();
        if (!currentItem) {
            descriptionLabel->clear();
            return;
        }
        
        const QString presetId = currentItem->data(Qt::UserRole).toString();
        const auto* preset = ArtifactRenderFormatPresetManager::instance().findPresetById(presetId);
        
        if (preset) {
            descriptionLabel->setText(
                QStringLiteral("<b>%1</b><br/>%2<br/>"
                              "<i>Container: %3 | Codec: %4</i>")
                    .arg(preset->name)
                    .arg(preset->description)
                    .arg(preset->container)
                    .arg(preset->codec));
        } else {
            descriptionLabel->clear();
        }
    }
};

class ArtifactRenderQueuePresetDialog::Impl {
public:
    ArtifactRenderQueuePresetSelector* presetSelector = nullptr;
    QDialogButtonBox* buttonBox = nullptr;
    QVector<QString> selectedPresetIds;
    
    void onConfirmed() {
        if (presetSelector) {
            selectedPresetIds = presetSelector->selectedPresetIds();
        }
    }
};

W_OBJECT_IMPL(ArtifactRenderQueuePresetSelector)
W_OBJECT_IMPL(ArtifactRenderQueuePresetDialog)

ArtifactRenderQueuePresetSelector::ArtifactRenderQueuePresetSelector(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // カテゴリ選択
    auto* categoryRow = new QHBoxLayout();
    categoryRow->addWidget(new QLabel(QStringLiteral("カテゴリ:"), this));
    
    impl_->categoryCombo = new QComboBox(this);
    impl_->categoryCombo->addItem(QStringLiteral("ビデオ形式"), static_cast<int>(ArtifactRenderFormatCategory::Video));
    impl_->categoryCombo->addItem(QStringLiteral("連番画像"), static_cast<int>(ArtifactRenderFormatCategory::ImageSequence));
    categoryRow->addWidget(impl_->categoryCombo, 1);
    
    layout->addLayout(categoryRow);
    
    // プリセット一覧
    impl_->presetListWidget = new QListWidget(this);
    impl_->presetListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    impl_->presetListWidget->setMinimumSize(280, 200);
    layout->addWidget(impl_->presetListWidget, 1);
    
    // 説明
    impl_->descriptionLabel = new QLabel(this);
    impl_->descriptionLabel->setFrameShape(QFrame::StyledPanel);
    impl_->descriptionLabel->setWordWrap(true);
    impl_->descriptionLabel->setMinimumHeight(60);
    layout->addWidget(impl_->descriptionLabel);
    
    // 接続
    connect(impl_->categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        impl_->currentCategory = static_cast<ArtifactRenderFormatCategory>(
            impl_->categoryCombo->itemData(index).toInt());
        impl_->loadPresets();
    });
    
    connect(impl_->presetListWidget, &QListWidget::currentItemChanged, this, [this]() {
        impl_->updateDescription();
        const auto* currentItem = impl_->presetListWidget->currentItem();
        if (currentItem) {
            const QString presetId = currentItem->data(Qt::UserRole).toString();
            Q_EMIT presetSelected(presetId);
        }
    });
    
    connect(impl_->presetListWidget, &QListWidget::itemDoubleClicked, this, [this]() {
        if (!impl_->multiSelectMode) {
            Q_EMIT presetsConfirmed({selectedPresetId()});
        }
    });
    
    impl_->loadPresets();
}

ArtifactRenderQueuePresetSelector::~ArtifactRenderQueuePresetSelector() {
    delete impl_;
}

QString ArtifactRenderQueuePresetSelector::selectedPresetId() const {
    if (!impl_->presetListWidget) return {};
    
    const auto* currentItem = impl_->presetListWidget->currentItem();
    if (!currentItem) return {};
    
    return currentItem->data(Qt::UserRole).toString();
}

const ArtifactRenderFormatPreset* ArtifactRenderQueuePresetSelector::selectedPreset() const {
    const QString id = selectedPresetId();
    if (id.isEmpty()) return nullptr;
    
    return ArtifactRenderFormatPresetManager::instance().findPresetById(id);
}

void ArtifactRenderQueuePresetSelector::setSelectedPresetId(const QString& presetId) {
    if (!impl_->presetListWidget) return;
    
    for (int i = 0; i < impl_->presetListWidget->count(); ++i) {
        auto* item = impl_->presetListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == presetId) {
            impl_->presetListWidget->setCurrentItem(item);
            impl_->updateDescription();
            return;
        }
    }
}

void ArtifactRenderQueuePresetSelector::setCategoryFilter(ArtifactRenderFormatCategory category) {
    impl_->currentCategory = category;
    impl_->categoryCombo->setCurrentIndex(
        impl_->categoryCombo->findData(static_cast<int>(category)));
    impl_->loadPresets();
}

void ArtifactRenderQueuePresetSelector::setMultiSelectMode(bool enabled) {
    impl_->multiSelectMode = enabled;
    impl_->presetListWidget->setSelectionMode(
        enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
}

bool ArtifactRenderQueuePresetSelector::isMultiSelectMode() const {
    return impl_->multiSelectMode;
}

QVector<QString> ArtifactRenderQueuePresetSelector::selectedPresetIds() const {
    QVector<QString> result;
    
    if (!impl_->presetListWidget) return result;
    
    const auto items = impl_->presetListWidget->selectedItems();
    for (const auto* item : items) {
        result.push_back(item->data(Qt::UserRole).toString());
    }
    
    return result;
}

ArtifactRenderQueuePresetDialog::ArtifactRenderQueuePresetDialog(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    
    // ダイアログ風レイアウト
    auto* layout = new QVBoxLayout(this);
    
    // タイトル
    auto* titleLabel = new QLabel(QStringLiteral("出力フォーマットを選択"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(12);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);
    
    // プリセットセレクター
    impl_->presetSelector = new ArtifactRenderQueuePresetSelector(this);
    impl_->presetSelector->setMultiSelectMode(true);
    layout->addWidget(impl_->presetSelector, 1);
    
    // ボタン
    impl_->buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    
    connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        impl_->onConfirmed();
        Q_EMIT presetsConfirmed(impl_->selectedPresetIds);
    });
    connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        Q_EMIT canceled();
    });
    
    layout->addWidget(impl_->buttonBox);
}

ArtifactRenderQueuePresetDialog::~ArtifactRenderQueuePresetDialog() {
    delete impl_;
}

QVector<QString> ArtifactRenderQueuePresetDialog::selectedPresetIds() const {
    return impl_->selectedPresetIds;
}

void ArtifactRenderQueuePresetDialog::setInitialSelection(const QVector<QString>& presetIds) {
    impl_->selectedPresetIds = presetIds;
}

} // namespace Artifact
