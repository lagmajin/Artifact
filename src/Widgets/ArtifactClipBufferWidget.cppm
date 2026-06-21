module;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QLabel>
#include <wobjectimpl.h>

module Artifact.Widgets.ClipBufferWidget;

import std;
import Event.Bus;
import Artifact.Event.Types;

namespace Artifact {

W_OBJECT_IMPL(ArtifactClipBufferWidget)

class ArtifactClipBufferWidget::Impl {
public:
    explicit Impl(ArtifactClipBufferWidget *parent);

    ArtifactClipBufferWidget *widget = nullptr;
    ArtifactClipBufferModel model_;
    QListView *listView_ = nullptr;
    QStandardItemModel *itemModel_ = nullptr;
    QPushButton *clearButton_ = nullptr;
    QPushButton *pasteButton_ = nullptr;
    ArtifactCore::EventBus::Subscription copySubscription_;

    void refreshList();
    void onClear();
    void onPasteSelected();
    void onDoubleClicked(const QModelIndex &index);
    void handleClipboardCopy(const QString &layerName, qint64 frame, const QString &desc, const QVariant &data);
};

ArtifactClipBufferWidget::Impl::Impl(ArtifactClipBufferWidget *parent)
    : widget(parent) {
    auto *mainLayout = new QVBoxLayout(parent);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    auto *buttonLayout = new QHBoxLayout();
    pasteButton_ = new QPushButton(QObject::tr("Paste"), parent);
    clearButton_ = new QPushButton(QObject::tr("Clear"), parent);
    buttonLayout->addWidget(pasteButton_);
    buttonLayout->addWidget(clearButton_);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    listView_ = new QListView(parent);
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    itemModel_ = new QStandardItemModel(parent);
    listView_->setModel(itemModel_);
    mainLayout->addWidget(listView_);

    auto *hintLabel = new QLabel(QObject::tr("Double-click or select & Paste to re-apply clips."), parent);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    QObject::connect(pasteButton_, &QPushButton::clicked, parent, [this]() { onPasteSelected(); });
    QObject::connect(clearButton_, &QPushButton::clicked, parent, [this]() { onClear(); });
    QObject::connect(listView_, &QListView::doubleClicked, parent, [this](const QModelIndex &idx) { onDoubleClicked(idx); });

    // Subscribe to EventBus copies if applicable
    copySubscription_ = ArtifactCore::globalEventBus().subscribe<FrameChangedEvent>(
        [this](const auto &) {
            // Placeholder
        });
}

void ArtifactClipBufferWidget::Impl::refreshList() {
    itemModel_->clear();
    const auto items = model_.items();
    for (const auto &item : items) {
        auto *listItem = new QStandardItem(
            QStringLiteral("[%1] F%2: %3").arg(item.layerName).arg(item.frame).arg(item.description));
        listItem->setData(item.id.toString(), Qt::UserRole);
        itemModel_->appendRow(listItem);
    }
}

void ArtifactClipBufferWidget::Impl::onClear() {
    model_.clear();
    refreshList();
}

void ArtifactClipBufferWidget::Impl::onPasteSelected() {
    const QModelIndex index = listView_->currentIndex();
    if (!index.isValid()) {
        return;
    }
    onDoubleClicked(index);
}

void ArtifactClipBufferWidget::Impl::onDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) {
        return;
    }
    const QString idString = index.data(Qt::UserRole).toString();
    const QUuid id(idString);
    const auto items = model_.items();
    const auto it = std::find_if(items.cbegin(), items.cend(),
                                 [&id](const ArtifactClipBufferItem &item) { return item.id == id; });
    if (it != items.cend()) {
        Q_EMIT widget->clipPasteRequested(it->data);
    }
}

void ArtifactClipBufferWidget::Impl::handleClipboardCopy(const QString &layerName, qint64 frame,
                                                         const QString &desc, const QVariant &data) {
    model_.addClip(layerName, frame, desc, data);
    refreshList();
}

ArtifactClipBufferWidget::ArtifactClipBufferWidget(QWidget *parent)
    : QWidget(parent), impl_(new Impl(this)) {}

ArtifactClipBufferWidget::~ArtifactClipBufferWidget() {
    delete impl_;
}

} // namespace Artifact
