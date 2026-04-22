module;
#include <algorithm>
#include <cstddef>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QLabel>
#include <QModelIndex>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>

module Artifact.Widgets.FrameResourceInspectorWidget;

namespace Artifact {

W_OBJECT_IMPL(FrameResourceInspectorWidget)

class FrameResourceInspectorWidget::Impl {
public:
    void updateDetailForRow(int row);

    class ResourceTableWidget : public QTableWidget {
    public:
        explicit ResourceTableWidget(Impl* impl, QWidget* parent = nullptr)
            : QTableWidget(parent), impl_(impl)
        {}

    protected:
        void currentChanged(const QModelIndex& current, const QModelIndex& previous) override
        {
            QTableWidget::currentChanged(current, previous);
            if (impl_) {
                impl_->updateDetailForRow(current.row());
            }
        }

    private:
        Impl* impl_ = nullptr;
    };

    FrameResourceInspectorWidget* owner_ = nullptr;
    QLabel* summary_ = nullptr;
    ResourceTableWidget* table_ = nullptr;
    QPlainTextEdit* detail_ = nullptr;
    ArtifactCore::FrameDebugSnapshot snapshot_;
    ArtifactCore::TraceSnapshot trace_;
    bool hasSnapshot_ = false;

    explicit Impl(FrameResourceInspectorWidget* owner)
        : owner_(owner)
    {}

    void setupUI()
    {
        auto* layout = new QVBoxLayout(owner_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        summary_ = new QLabel(owner_);
        summary_->setTextFormat(Qt::PlainText);
        summary_->setWordWrap(false);
        summary_->setMinimumHeight(42);
        layout->addWidget(summary_);

        auto* splitter = new QSplitter(Qt::Vertical, owner_);
        table_ = new ResourceTableWidget(this, splitter);
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels(QStringList{
            QStringLiteral("Label"),
            QStringLiteral("Type"),
            QStringLiteral("Relation"),
            QStringLiteral("Cache"),
            QStringLiteral("State"),
            QStringLiteral("Details")
        });
        table_->horizontalHeader()->setStretchLastSection(true);
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

        detail_ = new QPlainTextEdit(splitter);
        detail_->setReadOnly(true);
        detail_->setLineWrapMode(QPlainTextEdit::NoWrap);

        splitter->addWidget(table_);
        splitter->addWidget(detail_);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);
        layout->addWidget(splitter);
    }

    void showFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                const ArtifactCore::TraceSnapshot& trace)
    {
        snapshot_ = snapshot;
        trace_ = trace;
        hasSnapshot_ = true;

        QStringList lines;
        lines << QStringLiteral("Always-on Resource Inspector");
        lines << QStringLiteral("frame: %1").arg(snapshot.frame.framePosition());
        lines << QStringLiteral("composition: %1").arg(snapshot.compositionName.isEmpty()
                                                            ? QStringLiteral("<none>")
                                                            : snapshot.compositionName);
        lines << QStringLiteral("renderTiming: last=%1ms avg=%2ms")
                      .arg(QString::number(snapshot.renderLastFrameMs, 'f', 1))
                      .arg(QString::number(snapshot.renderAverageFrameMs, 'f', 1));
        lines << QStringLiteral("resourceCount: %1").arg(static_cast<int>(snapshot.resources.size()));
        lines << QStringLiteral("attachmentCount: %1").arg(static_cast<int>(snapshot.attachments.size()));
        int textureViews = 0;
        for (const auto& resource : snapshot.resources) {
            textureViews += resource.texture.valid ? 1 : 0;
        }
        for (const auto& attachment : snapshot.attachments) {
            textureViews += attachment.texture.valid ? 1 : 0;
        }
        lines << QStringLiteral("textureViews: %1").arg(textureViews);
        if (summary_) {
            summary_->setText(QStringLiteral("frame=%1  render=%2ms/%3ms  resources=%4  attachments=%5  traces=%6")
                                  .arg(snapshot.frame.framePosition())
                                  .arg(QString::number(snapshot.renderLastFrameMs, 'f', 1))
                                  .arg(QString::number(snapshot.renderAverageFrameMs, 'f', 1))
                                  .arg(static_cast<int>(snapshot.resources.size()))
                                  .arg(static_cast<int>(snapshot.attachments.size()))
                                  .arg(static_cast<int>(trace.frames.size())));
        }

        if (table_) {
            table_->setRowCount(0);
            table_->setRowCount(static_cast<int>(snapshot.resources.size() + snapshot.attachments.size()));
            int row = 0;
            for (const auto& resource : snapshot.resources) {
                populateRow(row++, resource.label, resource.type, resource.relation,
                            resource.cacheHit ? QStringLiteral("hit") : QStringLiteral("miss"),
                            resource.stale ? QStringLiteral("stale") : QStringLiteral("fresh"),
                            resourceDetails(resource));
            }
            for (const auto& attachment : snapshot.attachments) {
                populateRow(row++, attachment.name, attachment.role, QStringLiteral("attachment"),
                            attachment.readOnly ? QStringLiteral("read-only") : QStringLiteral("mutable"),
                            attachment.texture.valid ? QStringLiteral("texture") : QStringLiteral("buffer"),
                            attachmentDetails(attachment));
            }
            if (table_->rowCount() > 0 && table_->currentRow() < 0) {
                table_->selectRow(0);
            }
        }

        const int currentRow = table_ ? table_->currentRow() : -1;
        updateDetailForRow(currentRow >= 0 ? currentRow : 0);
    }

    QString resourceDetails(const ArtifactCore::FrameDebugResourceRecord& resource) const
    {
        QStringList details;
        if (resource.texture.valid) {
            details << QStringLiteral("texture=%1 %2x%3")
                           .arg(resource.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : resource.texture.format)
                           .arg(resource.texture.width)
                           .arg(resource.texture.height);
            details << QStringLiteral("view=mip%1/%2 slice%3 array%4 samples%5")
                           .arg(resource.texture.mipLevel)
                           .arg(resource.texture.mipLevels)
                           .arg(resource.texture.sliceIndex)
                           .arg(resource.texture.arrayLayers)
                           .arg(resource.texture.sampleCount);
            details << QStringLiteral("srgb=%1").arg(resource.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"));
        }
        if (resource.buffer.valid) {
            details << QStringLiteral("buffer=%1 bytes=%2")
                           .arg(resource.buffer.name.isEmpty() ? QStringLiteral("<buffer?>") : resource.buffer.name)
                           .arg(resource.buffer.byteSize);
        }
        details << QStringLiteral("cacheHit=%1").arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"));
        details << QStringLiteral("stale=%1").arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"));
        return details.join(QStringLiteral(" | "));
    }

    QString attachmentDetails(const ArtifactCore::FrameDebugAttachmentRecord& attachment) const
    {
        QStringList details;
        if (attachment.texture.valid) {
            details << QStringLiteral("texture=%1 %2x%3")
                           .arg(attachment.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : attachment.texture.format)
                           .arg(attachment.texture.width)
                           .arg(attachment.texture.height);
            details << QStringLiteral("view=mip%1/%2 slice%3 array%4 samples%5")
                           .arg(attachment.texture.mipLevel)
                           .arg(attachment.texture.mipLevels)
                           .arg(attachment.texture.sliceIndex)
                           .arg(attachment.texture.arrayLayers)
                           .arg(attachment.texture.sampleCount);
            details << QStringLiteral("srgb=%1").arg(attachment.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"));
        }
        if (attachment.buffer.valid) {
            details << QStringLiteral("buffer=%1 bytes=%2")
                           .arg(attachment.buffer.name.isEmpty() ? QStringLiteral("<buffer?>") : attachment.buffer.name)
                           .arg(attachment.buffer.byteSize);
        }
        details << QStringLiteral("readOnly=%1").arg(attachment.readOnly ? QStringLiteral("true") : QStringLiteral("false"));
        return details.join(QStringLiteral(" | "));
    }

    void populateRow(int row,
                     const QString& label,
                     const QString& type,
                     const QString& relation,
                     const QString& cacheState,
                     const QString& state,
                     const QString& details)
    {
        if (!table_ || row < 0 || row >= table_->rowCount()) {
            return;
        }
        table_->setItem(row, 0, new QTableWidgetItem(label.isEmpty() ? QStringLiteral("<unnamed>") : label));
        table_->setItem(row, 1, new QTableWidgetItem(type.isEmpty() ? QStringLiteral("<type?>") : type));
        table_->setItem(row, 2, new QTableWidgetItem(relation.isEmpty() ? QStringLiteral("<none>") : relation));
        table_->setItem(row, 3, new QTableWidgetItem(cacheState));
        table_->setItem(row, 4, new QTableWidgetItem(state));
        table_->setItem(row, 5, new QTableWidgetItem(details));
    }

};


void Artifact::FrameResourceInspectorWidget::Impl::updateDetailForRow(int row)
{
    if (!detail_ || !hasSnapshot_ || row < 0) {
        return;
    }

    QStringList lines;
    const int resourceCount = static_cast<int>(snapshot_.resources.size());
    if (row < resourceCount) {
        const auto& resource = snapshot_.resources[static_cast<std::size_t>(row)];
        QStringList readers;
        QString producer;
        int firstUse = -1;
        int lastUse = -1;
        for (int passIndex = 0; passIndex < static_cast<int>(snapshot_.passes.size()); ++passIndex) {
            const auto& pass = snapshot_.passes[static_cast<std::size_t>(passIndex)];
            for (const auto& output : pass.outputs) {
                if (output.name == resource.label) {
                    producer = pass.name;
                }
            }
            for (const auto& input : pass.inputs) {
                if (input.name == resource.label && !readers.contains(pass.name)) {
                    readers << pass.name;
                }
            }
            bool touched = false;
            for (const auto& input : pass.inputs) {
                if (input.name == resource.label) {
                    touched = true;
                    break;
                }
            }
            if (!touched) {
                for (const auto& output : pass.outputs) {
                    if (output.name == resource.label) {
                        touched = true;
                        break;
                    }
                }
            }
            if (touched) {
                if (firstUse < 0) {
                    firstUse = passIndex;
                }
                lastUse = passIndex;
            }
        }
        lines << QStringLiteral("Resource %1").arg(row);
        lines << QStringLiteral("label: %1").arg(resource.label.isEmpty() ? QStringLiteral("<unnamed>") : resource.label);
        lines << QStringLiteral("type: %1").arg(resource.type.isEmpty() ? QStringLiteral("<type?>") : resource.type);
        lines << QStringLiteral("relation: %1").arg(resource.relation.isEmpty() ? QStringLiteral("<none>") : resource.relation);
        lines << QStringLiteral("cacheHit: %1").arg(resource.cacheHit ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("stale: %1").arg(resource.stale ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("producer: %1").arg(producer.isEmpty() ? QStringLiteral("<unknown>") : producer);
        lines << QStringLiteral("readers: %1").arg(readers.isEmpty() ? QStringLiteral("<none>") : readers.join(QStringLiteral(", ")));
        lines << QStringLiteral("firstUse: %1").arg(firstUse >= 0 ? QString::number(firstUse) : QStringLiteral("<none>"));
        lines << QStringLiteral("lastUse: %1").arg(lastUse >= 0 ? QString::number(lastUse) : QStringLiteral("<none>"));
        if (resource.texture.valid) {
            lines << QStringLiteral("texture: %1 %2x%3")
                          .arg(resource.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : resource.texture.format)
                          .arg(resource.texture.width)
                          .arg(resource.texture.height);
            lines << QStringLiteral("view: mip=%1/%2 slice=%3 array=%4 samples=%5 srgb=%6")
                          .arg(resource.texture.mipLevel)
                          .arg(resource.texture.mipLevels)
                          .arg(resource.texture.sliceIndex)
                          .arg(resource.texture.arrayLayers)
                          .arg(resource.texture.sampleCount)
                          .arg(resource.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"));
            lines << QStringLiteral("pixelProbe: center=(%1,%2) format=%3")
                          .arg(resource.texture.width / 2)
                          .arg(resource.texture.height / 2)
                          .arg(resource.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : resource.texture.format);
            lines << QStringLiteral("pixelRGBA: <unavailable in snapshot>");
        }
        if (resource.buffer.valid) {
            lines << QStringLiteral("buffer: %1 bytes=%2")
                          .arg(resource.buffer.name.isEmpty() ? QStringLiteral("<buffer?>") : resource.buffer.name)
                          .arg(resource.buffer.byteSize);
        }
    } else if (row < resourceCount + static_cast<int>(snapshot_.attachments.size())) {
        const auto& attachment = snapshot_.attachments[static_cast<std::size_t>(row - resourceCount)];
        QStringList passRefs;
        int firstUse = -1;
        int lastUse = -1;
        for (int passIndex = 0; passIndex < static_cast<int>(snapshot_.passes.size()); ++passIndex) {
            const auto& pass = snapshot_.passes[static_cast<std::size_t>(passIndex)];
            bool matches = false;
            for (const auto& input : pass.inputs) {
                if (input.name == attachment.name) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                for (const auto& output : pass.outputs) {
                    if (output.name == attachment.name) {
                        matches = true;
                        break;
                    }
                }
            }
            if (matches && !passRefs.contains(pass.name)) {
                passRefs << pass.name;
            }
            if (matches) {
                if (firstUse < 0) {
                    firstUse = passIndex;
                }
                lastUse = passIndex;
            }
        }
        lines << QStringLiteral("Attachment %1").arg(row - resourceCount);
        lines << QStringLiteral("name: %1").arg(attachment.name.isEmpty() ? QStringLiteral("<unnamed>") : attachment.name);
        lines << QStringLiteral("role: %1").arg(attachment.role.isEmpty() ? QStringLiteral("<none>") : attachment.role);
        lines << QStringLiteral("readOnly: %1").arg(attachment.readOnly ? QStringLiteral("true") : QStringLiteral("false"));
        lines << QStringLiteral("passRefs: %1").arg(passRefs.isEmpty() ? QStringLiteral("<none>") : passRefs.join(QStringLiteral(", ")));
        lines << QStringLiteral("firstUse: %1").arg(firstUse >= 0 ? QString::number(firstUse) : QStringLiteral("<none>"));
        lines << QStringLiteral("lastUse: %1").arg(lastUse >= 0 ? QString::number(lastUse) : QStringLiteral("<none>"));
        if (attachment.texture.valid) {
            lines << QStringLiteral("texture: %1 %2x%3")
                          .arg(attachment.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : attachment.texture.format)
                          .arg(attachment.texture.width)
                          .arg(attachment.texture.height);
            lines << QStringLiteral("view: mip=%1/%2 slice=%3 array=%4 samples=%5 srgb=%6")
                          .arg(attachment.texture.mipLevel)
                          .arg(attachment.texture.mipLevels)
                          .arg(attachment.texture.sliceIndex)
                          .arg(attachment.texture.arrayLayers)
                          .arg(attachment.texture.sampleCount)
                          .arg(attachment.texture.srgb ? QStringLiteral("true") : QStringLiteral("false"));
            lines << QStringLiteral("pixelProbe: center=(%1,%2) format=%3")
                          .arg(attachment.texture.width / 2)
                          .arg(attachment.texture.height / 2)
                          .arg(attachment.texture.format.isEmpty() ? QStringLiteral("<fmt?>") : attachment.texture.format);
            lines << QStringLiteral("pixelRGBA: <unavailable in snapshot>");
        }
        if (attachment.buffer.valid) {
            lines << QStringLiteral("buffer: %1 bytes=%2")
                          .arg(attachment.buffer.name.isEmpty() ? QStringLiteral("<buffer?>") : attachment.buffer.name)
                          .arg(attachment.buffer.byteSize);
        }
    } else {
        lines << QStringLiteral("Trace");
        lines << QStringLiteral("frames=%1").arg(static_cast<int>(trace_.frames.size()));
        lines << QStringLiteral("threads=%1").arg(static_cast<int>(trace_.threads.size()));
        if (!trace_.threads.empty()) {
            ArtifactCore::TraceThreadRecord hotThread = trace_.threads.front();
            for (const auto& thread : trace_.threads) {
                if (thread.lockDepth > hotThread.lockDepth ||
                    (thread.lockDepth == hotThread.lockDepth && thread.lockCount > hotThread.lockCount)) {
                    hotThread = thread;
                }
            }
            lines << QStringLiteral("hotThread: %1 depth=%2 locks=%3 last=%4")
                          .arg(hotThread.threadName.isEmpty() ? QStringLiteral("<unnamed>") : hotThread.threadName)
                          .arg(hotThread.lockDepth)
                          .arg(hotThread.lockCount)
                          .arg(hotThread.lastMutexName.isEmpty() ? QStringLiteral("<none>") : hotThread.lastMutexName);
        }
        if (!trace_.frames.empty()) {
            const auto& frame = trace_.frames.back();
            lines << QStringLiteral("lastTraceFrame=%1").arg(frame.frameIndex);
            lines << QStringLiteral("lanes=%1").arg(static_cast<int>(frame.lanes.size()));
        }
    }

    detail_->setPlainText(lines.join(QStringLiteral("\n")));
};

Artifact::FrameResourceInspectorWidget::FrameResourceInspectorWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    impl_->setupUI();
}

Artifact::FrameResourceInspectorWidget::~FrameResourceInspectorWidget()
{
    delete impl_;
}

void Artifact::FrameResourceInspectorWidget::setFrameDebugSnapshot(const ArtifactCore::FrameDebugSnapshot& snapshot,
                                                                   const ArtifactCore::TraceSnapshot& trace)
{
    if (!impl_) {
        return;
    }
    impl_->showFrameDebugSnapshot(snapshot, trace);
}

} // namespace Artifact
