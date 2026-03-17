module;

#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>

module Artifact.Widgets.ObjectPicker;

import std;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;

namespace Artifact {

W_OBJECT_IMPL(ArtifactObjectPickerDialog)

ArtifactObjectPickerDialog::ArtifactObjectPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Select Object"));
    setMinimumSize(400, 500);
    
    auto* layout = new QVBoxLayout(this);
    
    // 検索フィルター
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(QStringLiteral("Filter:"), this));
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("Search..."));
    filterLayout->addWidget(searchEdit_, 1);
    layout->addLayout(filterLayout);
    
    // オブジェクトツリー
    objectTree_ = new QTreeWidget(this);
    objectTree_->setHeaderLabels(QStringList{QStringLiteral("Name"), QStringLiteral("ID"), QStringLiteral("Type")});
    objectTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    objectTree_->setExpandsOnDoubleClick(false);
    layout->addWidget(objectTree_, 1);
    
    // OK/Cancel ボタン
    buttonBox_ = new QDialogButtonBox(static_cast<QDialogButtonBox::StandardButtons>(QDialogButtonBox::Ok | QDialogButtonBox::Cancel), this);
    layout->addWidget(buttonBox_);
    
    // シグナル接続
    connect(objectTree_, &QTreeWidget::itemDoubleClicked, this, &ArtifactObjectPickerDialog::onObjectDoubleClicked);
    connect(searchEdit_, &QLineEdit::textChanged, this, &ArtifactObjectPickerDialog::onSearchTextChanged);
    connect(buttonBox_, &QDialogButtonBox::accepted, this, &ArtifactObjectPickerDialog::onOkClicked);
    connect(buttonBox_, &QDialogButtonBox::rejected, this, &ArtifactObjectPickerDialog::onCancelClicked);
    
    // ツリー構築
    buildObjectTree();
}

ArtifactObjectPickerDialog::~ArtifactObjectPickerDialog()
{
}

void ArtifactObjectPickerDialog::setReferenceType(const QString& typeName)
{
    referenceType_ = typeName;
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
    if (item) {
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
    if (item) {
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
    
    // Compositions
    addCompositionTree(nullptr);
    
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
