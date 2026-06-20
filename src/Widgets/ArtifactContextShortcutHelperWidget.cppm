module;
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <wobjectimpl.h>

module Artifact.Widgets.ContextShortcutHelperWidget;

import std;
import Artifact.Widgets.ContextShortcutProvider;

namespace Artifact {

W_OBJECT_IMPL(ArtifactContextShortcutHelperWidget)

class ArtifactContextShortcutHelperWidget::Impl {
public:
    explicit Impl(ArtifactContextShortcutHelperWidget *parent);

    ArtifactContextShortcutHelperWidget *widget = nullptr;
    WorkspaceMode currentMode_ = WorkspaceMode::Default;
    QLineEdit *searchEdit_ = nullptr;
    QTreeWidget *treeWidget_ = nullptr;

    void updateShortcutTree();
};

ArtifactContextShortcutHelperWidget::Impl::Impl(ArtifactContextShortcutHelperWidget *parent)
    : widget(parent) {
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    searchEdit_ = new QLineEdit(parent);
    searchEdit_->setPlaceholderText(QObject::tr("Search shortcuts..."));
    layout->addWidget(searchEdit_);

    treeWidget_ = new QTreeWidget(parent);
    treeWidget_->setColumnCount(2);
    treeWidget_->setHeaderLabels({QObject::tr("Action"), QObject::tr("Shortcut")});
    treeWidget_->setAlternatingRowColors(true);
    treeWidget_->setRootIsDecorated(true);
    layout->addWidget(treeWidget_);

    QObject::connect(searchEdit_, &QLineEdit::textChanged, parent, [this]() {
        updateShortcutTree();
    });

    updateShortcutTree();
}

void ArtifactContextShortcutHelperWidget::Impl::updateShortcutTree() {
    treeWidget_->clear();
    const QString filterText = searchEdit_->text().trimmed();

    const auto list = ArtifactContextShortcutProvider::getShortcutsForMode(currentMode_);
    std::unordered_map<std::wstring, QTreeWidgetItem*> categoryNodes;

    for (const auto &entry : list) {
        if (!filterText.isEmpty()) {
            bool matches = entry.actionName.contains(filterText, Qt::CaseInsensitive) ||
                           entry.category.contains(filterText, Qt::CaseInsensitive) ||
                           entry.description.contains(filterText, Qt::CaseInsensitive) ||
                           entry.shortcut.toString().contains(filterText, Qt::CaseInsensitive);
            if (!matches) {
                continue;
            }
        }

        std::wstring catKey = entry.category.toStdWString();
        QTreeWidgetItem *catNode = nullptr;
        auto it = categoryNodes.find(catKey);
        if (it == categoryNodes.end()) {
            catNode = new QTreeWidgetItem(treeWidget_);
            catNode->setText(0, entry.category);
            catNode->setFirstColumnSpanned(true);
            treeWidget_->addTopLevelItem(catNode);
            categoryNodes[catKey] = catNode;
            catNode->setExpanded(true);
        } else {
            catNode = it->second;
        }

        auto *item = new QTreeWidgetItem(catNode);
        item->setText(0, entry.actionName);
        item->setText(1, entry.shortcut.toString(QKeySequence::NativeText));
        item->setToolTip(0, entry.description);
        item->setToolTip(1, entry.description);
    }
}

ArtifactContextShortcutHelperWidget::ArtifactContextShortcutHelperWidget(QWidget *parent)
    : QWidget(parent), impl_(new Impl(this)) {}

ArtifactContextShortcutHelperWidget::~ArtifactContextShortcutHelperWidget() {
    delete impl_;
}

void ArtifactContextShortcutHelperWidget::setWorkspaceMode(WorkspaceMode mode) {
    if (impl_->currentMode_ != mode) {
        impl_->currentMode_ = mode;
        impl_->updateShortcutTree();
    }
}

} // namespace Artifact
