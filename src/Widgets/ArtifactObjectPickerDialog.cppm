module;
#include <utility>

#include <wobjectimpl.h>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QHeaderView>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>

module Artifact.Widgets.ObjectPicker;

import std;
import Artifact.Service.Project;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;

namespace Artifact {

W_OBJECT_IMPL(ArtifactObjectPickerDialog)

ArtifactObjectPickerDialog::ArtifactObjectPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Select Object"));
    setMinimumSize(620, 680);
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("Select Object"), this);
    title->setObjectName(QStringLiteral("objectPickerTitle"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(14);
    title->setFont(titleFont);
    layout->addWidget(title);
    auto* description = new QLabel(
        QStringLiteral("Search for an object or select it from the hierarchy below."), this);
    QPalette descriptionPalette = description->palette();
    descriptionPalette.setColor(QPalette::WindowText,
                                palette().color(QPalette::PlaceholderText));
    description->setPalette(descriptionPalette);
    layout->addWidget(description);
    
    // 検索フィルター
    auto* filterLayout = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("Search by name or type…"));
    searchEdit_->setMinimumHeight(36);
    filterLayout->addWidget(searchEdit_, 1);
    layout->addLayout(filterLayout);
    
    // オブジェクトツリー
    objectTree_ = new QTreeWidget(this);
    objectTree_->setHeaderLabels(QStringList{QStringLiteral("Name"), QStringLiteral("ID"), QStringLiteral("Type")});
    objectTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    objectTree_->setExpandsOnDoubleClick(false);
    objectTree_->setAlternatingRowColors(true);
    objectTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    objectTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    objectTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(objectTree_, 1);
    
    // OK/Cancel ボタン
    const DialogButtonRow buttons = createWindowsDialogButtonRow(this);
    buttonRow_ = buttons.widget;
    okButton_ = buttons.okButton;
    cancelButton_ = buttons.cancelButton;
    okButton_->setText(QStringLiteral("Select"));
    cancelButton_->setText(QStringLiteral("Cancel"));
    okButton_->setMinimumSize(110, 36);
    cancelButton_->setMinimumSize(110, 36);
    QPalette selectPalette = okButton_->palette();
    selectPalette.setColor(QPalette::Button, QColor(43, 111, 232));
    selectPalette.setColor(QPalette::ButtonText, Qt::white);
    okButton_->setPalette(selectPalette);
    okButton_->setAutoFillBackground(true);
    layout->addWidget(buttonRow_);
    
    // シグナル接続
    connect(objectTree_, &QTreeWidget::itemDoubleClicked, this, &ArtifactObjectPickerDialog::onObjectDoubleClicked);
    connect(searchEdit_, &QLineEdit::textChanged, this, &ArtifactObjectPickerDialog::onSearchTextChanged);
    connect(okButton_, &QPushButton::clicked, this, &ArtifactObjectPickerDialog::onOkClicked);
    connect(cancelButton_, &QPushButton::clicked, this, &ArtifactObjectPickerDialog::onCancelClicked);
    
    // ツリー構築
    buildObjectTree();
}

ArtifactObjectPickerDialog::~ArtifactObjectPickerDialog()
{
}

void ArtifactObjectPickerDialog::setReferenceType(const QString& typeName)
{
    referenceType_ = typeName;
    auto* title = findChild<QLabel*>(QStringLiteral("objectPickerTitle"));
    if (referenceType_.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
        setWindowTitle(QStringLiteral("Select Layer"));
        if (title) title->setText(QStringLiteral("Select Layer"));
    } else {
        setWindowTitle(QStringLiteral("Select Object"));
        if (title) title->setText(QStringLiteral("Select Object"));
    }
}

void ArtifactObjectPickerDialog::setCurrentSelectionId(const ArtifactCore::Id& id)
{
    currentSelectionId_ = id;
    
    // 該当アイテムを選択状態に
    auto items = objectTree_->findItems(id.toString(), Qt::MatchExactly, 1);
    if (!items.isEmpty()) {
        objectTree_->setCurrentItem(items.first());
    }
}

bool ArtifactObjectPickerDialog::isSelectableItem(QTreeWidgetItem* item) const
{
    if (!item) {
        return false;
    }
    if (referenceType_.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
        return item->text(2).compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0;
    }
    return true;
}

ArtifactCore::Id ArtifactObjectPickerDialog::selectedId() const
{
    auto* item = objectTree_->currentItem();
    if (!item) {
        return ArtifactCore::Id::Nil();
    }
    // 文字列から ID を復元（あるいは UserRole から取得）
    return ArtifactCore::Id(item->text(1));
}

void ArtifactObjectPickerDialog::onObjectDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (isSelectableItem(item)) {
        currentSelectionId_ = ArtifactCore::Id(item->text(1));
        accept();
    }
}

void ArtifactObjectPickerDialog::onSearchTextChanged(const QString& text)
{
    filterObjectTree(text);
}

void ArtifactObjectPickerDialog::onOkClicked()
{
    auto* item = objectTree_->currentItem();
    if (isSelectableItem(item)) {
        currentSelectionId_ = ArtifactCore::Id(item->text(1));
        accept();
    } else {
        reject();
    }
}

void ArtifactObjectPickerDialog::onCancelClicked()
{
    reject();
}

void ArtifactObjectPickerDialog::buildObjectTree()
{
    objectTree_->clear();
    
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    
    if (referenceType_.compare(QStringLiteral("Layer"), Qt::CaseInsensitive) == 0) {
        addLayerTree(nullptr);
    } else {
        // Default to the existing composition tree with nested layers.
        addCompositionTree(nullptr);
    }
    
    // 現在選択中のアイテムを選択
    if (!currentSelectionId_.isNil()) {
        auto items = objectTree_->findItems(currentSelectionId_.toString(), Qt::MatchExactly, 1);
        if (!items.isEmpty()) {
            objectTree_->setCurrentItem(items.first());
        }
    }
}

void ArtifactObjectPickerDialog::addCompositionTree(QTreeWidgetItem* parent)
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    
    // Composition アイテムを追加
    auto comp = service->currentComposition().lock();
    if (comp) {
        auto* compItem = new QTreeWidgetItem(parent);
        compItem->setText(0, comp->settings().compositionName().toQString());
        compItem->setText(1, comp->id().toString());
        compItem->setText(2, QStringLiteral("Composition"));
        compItem->setExpanded(true);
        
        // レイヤーを追加
        const auto layers = comp->allLayer();
        for (const auto& layer : layers) {
            if (!layer) continue;
            
            auto* layerItem = new QTreeWidgetItem(compItem);
            layerItem->setText(0, layer->layerName());
            layerItem->setText(1, layer->id().toString());
            layerItem->setText(2, QStringLiteral("Layer"));
        }
    }
}

void ArtifactObjectPickerDialog::addLayerTree(QTreeWidgetItem* parent)
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }

    auto comp = service->currentComposition().lock();
    if (comp) {
        auto* compItem = new QTreeWidgetItem(parent);
        compItem->setText(0, comp->settings().compositionName().toQString());
        compItem->setText(1, comp->id().toString());
        compItem->setText(2, QStringLiteral("Composition"));
        compItem->setExpanded(true);

        const auto layers = comp->allLayer();
        for (const auto& layer : layers) {
            if (!layer) {
                continue;
            }

            auto* layerItem = new QTreeWidgetItem(compItem);
            layerItem->setText(0, layer->layerName());
            layerItem->setText(1, layer->id().toString());
            layerItem->setText(2, QStringLiteral("Layer"));
        }
    }
}

void ArtifactObjectPickerDialog::filterObjectTree(const QString& filter)
{
    if (filter.isEmpty()) {
        // 全アイテム表示
        for (int i = 0; i < objectTree_->topLevelItemCount(); ++i) {
            auto* item = objectTree_->topLevelItem(i);
            if (item) {
                item->setHidden(false);
                for (int j = 0; j < item->childCount(); ++j) {
                    item->child(j)->setHidden(false);
                }
            }
        }
    } else {
        // フィルター適用
        for (int i = 0; i < objectTree_->topLevelItemCount(); ++i) {
            auto* item = objectTree_->topLevelItem(i);
            if (item) {
                bool hasVisibleChild = false;
                for (int j = 0; j < item->childCount(); ++j) {
                    auto* child = item->child(j);
                    bool match = child->text(0).contains(filter, Qt::CaseInsensitive) ||
                                 child->text(2).contains(filter, Qt::CaseInsensitive);
                    child->setHidden(!match);
                    hasVisibleChild |= match;
                }
                item->setHidden(!hasVisibleChild && !item->text(0).contains(filter, Qt::CaseInsensitive));
            }
        }
    }
}

} // namespace Artifact
