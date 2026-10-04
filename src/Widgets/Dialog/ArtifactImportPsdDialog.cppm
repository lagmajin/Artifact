module;
#include <algorithm>
#include <wobjectimpl.h>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

module Artifact.Widgets.ImportPsdDialog;

import Core.ArtifactMath;
import Translation.Manager;

namespace Artifact {
namespace {

QString dialogText(const QString& key, const QString& fallback)
{
    return TranslationManager::instance().tr(key, fallback);
}

} // namespace

class ArtifactImportPsdDialog::Impl {
public:
    ArtifactCore::PsdDocument document;
    bool opened = false;
    QListWidget* layerList = nullptr;
    QLabel* summaryLabel = nullptr;
    QPushButton* selectVisibleButton = nullptr;
    QPushButton* deselectButton = nullptr;

    ~Impl()
    {
        document.close();
    }

    void refreshSummary()
    {
        if (!summaryLabel) {
            return;
        }
        int selected = 0;
        int total = 0;
        if (layerList) {
            total = layerList->count();
            for (int i = 0; i < total; ++i) {
                if (const auto* item = layerList->item(i);
                    item && item->checkState() == Qt::Checked) {
                    ++selected;
                    continue;
                }
            }
        }
        summaryLabel->setText(dialogText(
            QStringLiteral("dialog.import_psd.summary"),
            QStringLiteral("選択中: %1 / %2 層")).arg(selected).arg(total));
    }

    void setAllCheckState(Qt::CheckState state)
    {
        if (!layerList) {
            return;
        }
        const int total = layerList->count();
        for (int i = 0; i < total; ++i) {
            if (auto* item = layerList->item(i)) {
                item->setCheckState(state);
            }
        }
        refreshSummary();
    }

    void selectVisibleLayers()
    {
        if (!layerList) {
            return;
        }
        const auto& layers = document.layers();
        const int total = layerList->count();
        for (int i = 0; i < total; ++i) {
            auto* item = layerList->item(i);
            if (!item) {
                continue;
            }
            const int layerIndex = item->data(Qt::UserRole).toInt();
            const bool visible = layerIndex >= 0 &&
                layerIndex < layers.size() && layers.at(layerIndex).visible;
            item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        }
        refreshSummary();
    }
};

ArtifactImportPsdDialog::ArtifactImportPsdDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
    , impl_(new Impl())
{
    setWindowTitle(dialogText(QStringLiteral("dialog.import_psd.title"),
                              QStringLiteral("PSD レイヤーを読み込み")));
    setAccessibleName(QStringLiteral("Import PSD Layers Dialog"));
    setMinimumSize(460, 420);

    auto* layout = new QVBoxLayout(this);
    const QString displayName = QFileInfo(filePath).fileName();
    auto* header = new QLabel(
        dialogText(QStringLiteral("dialog.import_psd.header"),
                   QStringLiteral("%1 .layerNameArg")).arg(displayName),
        this);
    header->setWordWrap(true);
    layout->addWidget(header);

    impl_->layerList = new QListWidget(this);
    impl_->layerList->setSelectionMode(QAbstractItemView::NoSelection);
    impl_->layerList->setUniformItemSizes(true);
    layout->addWidget(impl_->layerList, 1);

    auto* buttonRow = new QHBoxLayout;
    impl_->selectVisibleButton = new QPushButton(
        dialogText(QStringLiteral("dialog.import_psd.select_visible"),
                   QStringLiteral("可視層を選ぶ")), this);
    impl_->deselectButton = new QPushButton(
        dialogText(QStringLiteral("dialog.import_psd.deselect"),
                   QStringLiteral("すべて解除")), this);
    buttonRow->addWidget(impl_->selectVisibleButton);
    buttonRow->addWidget(impl_->deselectButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    impl_->summaryLabel = new QLabel(this);
    layout->addWidget(impl_->summaryLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(
        dialogText(QStringLiteral("dialog.import_psd.accept"),
                   QStringLiteral("選択した層を読み込み")));
    buttons->button(QDialogButtonBox::Cancel)->setText(
        dialogText(QStringLiteral("dialog.import_psd.cancel"),
                   QStringLiteral("キャンセル")));
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(impl_->selectVisibleButton, &QPushButton::clicked, this,
                     [this]() { impl_->selectVisibleLayers(); });
    QObject::connect(impl_->deselectButton, &QPushButton::clicked, this,
                     [this]() { impl_->setAllCheckState(Qt::Unchecked); });

    impl_->opened = impl_->document.open(filePath);
    if (!impl_->opened || impl_->document.layers().isEmpty()) {
        // A PSD without parsed layer metadata cannot be expanded, so offer only
        // the flattened preview path by refusing the layer dialog outright.
        impl_->layerList->setEnabled(false);
        impl_->selectVisibleButton->setEnabled(false);
        impl_->deselectButton->setEnabled(false);
        header->setText(dialogText(
            QStringLiteral("dialog.import_psd.no_layers"),
            QStringLiteral("%1 は PSD レイヤー情報を解析できませんでした。")).arg(displayName));
        impl_->summaryLabel->setText(dialogText(
            QStringLiteral("dialog.import_psd.flattened_only"),
            QStringLiteral("合成済み画像としてのみ読み込めます。")));
        return;
    }

    const auto& layers = impl_->document.layers();
    for (int i = 0; i < layers.size(); ++i) {
        const auto& layer = layers.at(i);
        const QString name = layer.name.trimmed().isEmpty()
            ? QStringLiteral("Layer %1").arg(i + 1)
            : layer.name.trimmed();
        const QString boundsText =
            QStringLiteral("%1 x %2").arg(layer.bounds.width()).arg(layer.bounds.height());
        auto* item = new QListWidgetItem(
            QStringLiteral("%1   [%2]").arg(name, boundsText));
        item->setData(Qt::UserRole, i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        // Hidden layers start unchecked so the default import matches what the
        // artist sees in Photoshop; empty layers cannot contribute pixels.
        const bool importable = layer.visible && layer.bounds.isValid() &&
                                !layer.bounds.isEmpty();
        item->setCheckState(importable ? Qt::Checked : Qt::Unchecked);
        if (!layer.visible) {
            item->setToolTip(dialogText(
                QStringLiteral("dialog.import_psd.hidden_hint"),
                QStringLiteral("非表示のため既定で除外されます")));
        } else if (!layer.bounds.isValid() || layer.bounds.isEmpty()) {
            item->setToolTip(dialogText(
                QStringLiteral("dialog.import_psd.empty_hint"),
                QStringLiteral("空のレイヤーです")));
        }
        impl_->layerList->addItem(item);
    }
    QObject::connect(impl_->layerList, &QListWidget::itemChanged, this,
                     [this](QListWidgetItem*) { impl_->refreshSummary(); });
    impl_->refreshSummary();
}

bool ArtifactImportPsdDialog::documentOpened() const
{
    return impl_ && impl_->opened;
}

QVector<ArtifactPsdLayerSelection> ArtifactImportPsdDialog::selectedLayers() const
{
    QVector<ArtifactPsdLayerSelection> result;
    if (!impl_ || !impl_->layerList) {
        return result;
    }
    const int total = impl_->layerList->count();
    for (int i = 0; i < total; ++i) {
        const auto* item = impl_->layerList->item(i);
        if (!item || item->checkState() != Qt::Checked) {
            continue;
        }
        ArtifactPsdLayerSelection entry;
        entry.layerIndex = item->data(Qt::UserRole).toInt();
        entry.name = item->text();
        entry.selected = true;
        result.push_back(entry);
    }
    return result;
}

} // namespace Artifact