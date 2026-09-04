module;
#include <QHBoxLayout>
#include <QEvent>
#include <QDrag>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGui/QIcon>
#include <memory>
#include <wobjectimpl.h>

module Artifact.Widgets.TemplateLibrary;

import Artifact.Service.Project;
import Undo.UndoManager;

namespace {

class ArtifactTemplateListWidget final : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        const auto* item = currentItem();
        if (!item) return;
        const QString fileName = item->data(Qt::UserRole).toString();
        if (fileName.isEmpty()) return;
        auto* mimeData = new QMimeData();
        mimeData->setData("application/x-artifact-template",
                          fileName.toUtf8());
        mimeData->setText(fileName);
        auto* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec(supportedActions, Qt::CopyAction);
    }
};

} // namespace

namespace Artifact {

W_OBJECT_IMPL(ArtifactTemplateLibraryWidget)

ArtifactTemplateLibraryWidget::ArtifactTemplateLibraryWidget(QWidget* parent)
    : QWidget(parent) {
    list_ = new ArtifactTemplateListWidget(this);
    list_->setDragEnabled(true);
    list_->setDefaultDropAction(Qt::CopyAction);
    list_->installEventFilter(this);
    list_->setAccessibleName(QStringLiteral("Artifact template library"));
    refreshButton_ = new QPushButton(QStringLiteral("Refresh"), this);
    refreshButton_->setAccessibleName(QStringLiteral("Refresh template library"));

    auto* controls = new QHBoxLayout();
    controls->addWidget(refreshButton_);
    controls->addStretch();
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(list_);

    connect(refreshButton_, &QPushButton::clicked, this,
            [this]() { refresh(); });
}

void ArtifactTemplateLibraryWidget::setLibrary(
    const ArtifactTemplateLibrary& library) {
    library_ = library;
    refresh();
}

ArtifactTemplateLibrary ArtifactTemplateLibraryWidget::library() const {
    return library_;
}

void ArtifactTemplateLibraryWidget::refresh() {
    if (!list_) return;
    list_->clear();
    for (const auto& fileName : library_.templateFiles()) {
        auto* item = new QListWidgetItem(fileName, list_);
        item->setData(Qt::UserRole, fileName);
    }
}

ArtifactTemplateDocument ArtifactTemplateLibraryWidget::selectedDocument(
    QString* errorMessage) const {
    if (!list_ || !list_->currentItem()) {
        if (errorMessage) *errorMessage = QStringLiteral("No template selected");
        return {};
    }
    return library_.load(list_->currentItem()->data(Qt::UserRole).toString(),
                         errorMessage);
}

int ArtifactTemplateLibraryWidget::applySelectedToCurrentComposition(
    QString* errorMessage) const {
    auto* service = ArtifactProjectService::instance();
    const auto composition = service ? service->currentComposition().lock()
                                     : ArtifactCompositionPtr{};
    if (!composition) {
        if (errorMessage) *errorMessage = QStringLiteral("No current composition");
        return 0;
    }
    const auto document = selectedDocument(errorMessage);
    if (document.name.isEmpty() && document.layerSnapshots.isEmpty()) return 0;
    auto* undoManager = UndoManager::instance();
    if (!undoManager) {
        return document.appendToComposition(*composition);
    }
    const auto layers = document.instantiateLayers();
    auto transaction = std::make_unique<MacroUndoCommand>(
        QStringLiteral("Import Template"));
    int appended = 0;
    for (auto it = layers.crbegin(); it != layers.crend(); ++it) {
        if (!*it) continue;
        transaction->addChild(std::make_unique<AddLayerCommand>(
            composition, *it, true));
        ++appended;
    }
    if (appended == 0 || !undoManager->push(std::move(transaction))) {
        if (appended > 0 && errorMessage) {
            *errorMessage = QStringLiteral("Could not record template import");
        }
        return 0;
    }
    return appended;
}

bool ArtifactTemplateLibraryWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == list_ && event && event->type() == QEvent::MouseButtonDblClick) {
        QString error;
        const int appended = applySelectedToCurrentComposition(&error);
        if (appended > 0) {
            setToolTip(QStringLiteral("Added %1 template layer(s) to the current composition")
                           .arg(appended));
        } else if (!error.isEmpty()) {
            setToolTip(error);
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace Artifact
