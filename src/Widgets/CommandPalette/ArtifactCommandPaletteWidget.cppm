module;
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Input.Operator";

module Command.Palette;

namespace Artifact
{

namespace
{
QString normalizedShortcut(const QKeySequence& seq)
{
    if (seq.isEmpty()) {
        return {};
    }
    return seq.toString(QKeySequence::PortableText);
}

int fuzzyScore(const QString& haystack, const QString& needle)
{
    if (needle.isEmpty()) {
        return 1;
    }
    const int idx = haystack.indexOf(needle);
    if (idx < 0) {
        return 0;
    }
    return 100 - idx;
}

void collectMenuActions(QMenu* menu, std::vector<QAction*>& out)
{
    if (!menu) {
        return;
    }
    const auto actions = menu->actions();
    for (QAction* action : actions) {
        if (!action) {
            continue;
        }
        if (action->isSeparator()) {
            continue;
        }
        if (!action->isVisible() || !action->isEnabled()) {
            // Still keep the entry visible; we only filter at the model level
            // for "executable" later. For now, include to surface all
            // discoverable commands.
        }
        out.push_back(action);
        if (action->menu()) {
            collectMenuActions(action->menu(), out);
        }
    }
}

QString synthesizeActionId(const QAction* action)
{
    if (!action) {
        return {};
    }
    const QString oname = action->objectName();
    if (!oname.isEmpty()) {
        return QStringLiteral("qaction::") + oname;
    }
    const QString text = action->text();
    QString stripped = text;
    stripped.remove(QChar('&'));
    stripped = stripped.trimmed();
    if (stripped.isEmpty()) {
        return {};
    }
    return QStringLiteral("qaction::text::") + stripped;
}
} // namespace

ArtifactCommandPaletteWidget::ArtifactCommandPaletteWidget(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("ArtifactCommandPaletteWidget"));
    setWindowTitle(tr("Command Palette"));
    setModal(true);
    resize(640, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setObjectName(QStringLiteral("ArtifactCommandPaletteFilter"));
    filterEdit_->setPlaceholderText(tr("Type a command (fuzzy match, e.g. \"exp png\", \"key dup\", \"ease\")..."));
    filterEdit_->setClearButtonEnabled(true);
    layout->addWidget(filterEdit_);

    actionList_ = new QListWidget(this);
    actionList_->setObjectName(QStringLiteral("ArtifactCommandPaletteList"));
    layout->addWidget(actionList_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(filterEdit_, &QLineEdit::textChanged,
            this, &ArtifactCommandPaletteWidget::setFilterText);
    connect(actionList_, &QListWidget::itemActivated,
            this, [this](QListWidgetItem*) { executeHighlighted(); });

    filterEdit_->setFocus();
}

ArtifactCommandPaletteWidget::~ArtifactCommandPaletteWidget() = default;

void ArtifactCommandPaletteWidget::setMainWindow(QWidget* mainWindow)
{
    mainWindow_ = mainWindow;
    refreshActionList();
}

QString ArtifactCommandPaletteWidget::filterText() const
{
    return filterEdit_ ? filterEdit_->text() : QString();
}

void ArtifactCommandPaletteWidget::setFilterText(const QString& text)
{
    if (!actionList_) {
        return;
    }
    const QString needle = text.trimmed().toLower();
    int firstVisible = -1;
    for (int i = 0; i < actionList_->count(); ++i) {
        auto* item = actionList_->item(i);
        const bool match = needle.isEmpty() ||
            item->text().toLower().contains(needle) ||
            item->data(Qt::UserRole).toString().toLower().contains(needle);
        item->setHidden(!match);
        if (match && firstVisible < 0) {
            firstVisible = i;
        }
    }
    if (firstVisible >= 0) {
        actionList_->setCurrentRow(firstVisible);
    }
}

void ArtifactCommandPaletteWidget::refreshActionList()
{
    using ArtifactCore::ActionManager;
    auto* mgr = ActionManager::instance();
    if (!mgr || !actionList_) {
        return;
    }

    // 1. Reflect QActions from the main window menu hierarchy into ActionManager.
    if (mainWindow_) {
        QMenuBar* menuBar = qobject_cast<QMenuBar*>(mainWindow_);
        if (!menuBar) {
            menuBar = mainWindow_->findChild<QMenuBar*>();
        }
        if (menuBar) {
            std::vector<QAction*> collected;
            for (QAction* topMenuAction : menuBar->actions()) {
                if (auto* subMenu = topMenuAction ? topMenuAction->menu() : nullptr) {
                    collectMenuActions(subMenu, collected);
                }
            }
            for (QAction* qa : collected) {
                if (!qa) {
                    continue;
                }
                const QString id = synthesizeActionId(qa);
                if (id.isEmpty()) {
                    continue;
                }
                if (mgr->getAction(id)) {
                    continue;
                }
                QString label = qa->text();
                label.remove(QChar('&'));
                label = label.trimmed();
                if (label.isEmpty()) {
                    label = id;
                }
                QString description = qa->toolTip();
                if (description.isEmpty()) {
                    description = qa->statusTip();
                }
                const QString category = qa->menu() ? qa->menu()->title() : QString();
                const QString shortcutStr = normalizedShortcut(qa->shortcut());

                auto* action = mgr->registerAction(id, label, description, category);
                if (!action) {
                    continue;
                }
                if (!shortcutStr.isEmpty()) {
                    action->setShortcutText(shortcutStr);
                }
                // Callback: trigger the original QAction. No new QObject::connect
                // is added to the host; we just invoke the action.
                QPointer<QAction> guard(qa);
                action->setExecuteCallback([guard](const QVariantMap&) {
                    if (guard) {
                        guard->trigger();
                    }
                });
            }
        }
    }

    // 2. Collect all actions (ActionManager-first; we already de-duped in step 1).
    std::vector<ArtifactCore::Action*> actions = mgr->allActions();

    // 3. Apply fuzzy filter if a filter is set; otherwise keep order.
    const QString needle = filterEdit_ ? filterEdit_->text().trimmed().toLower() : QString();
    struct Scored
    {
        ArtifactCore::Action* action;
        int score;
    };
    std::vector<Scored> scored;
    scored.reserve(actions.size());
    if (needle.isEmpty()) {
        // No filter: order by MRU score only (most recent on top).
        for (auto* a : actions) {
            if (!a) {
                continue;
            }
            const int mru = mruScoreFor(a->id());
            scored.push_back({a, mru});
        }
        std::sort(scored.begin(), scored.end(), [](const Scored& x, const Scored& y) {
            return x.score > y.score;
        });
    } else {
        for (auto* a : actions) {
            if (!a) {
                continue;
            }
            const QString label = (a->label().isEmpty() ? a->name() : a->label()).toLower();
            const QString id = a->id().toLower();
            const QString desc = a->description().toLower();
            const int fuzzy = std::max({
                fuzzyScore(label, needle) * 3,
                fuzzyScore(id, needle) * 2,
                fuzzyScore(desc, needle) * 1,
            });
            // MRU boost (only adds, never subtracts) keeps recently used
            // commands at the top even when several actions match the query.
            const int mru = mruScoreFor(a->id());
            const int s = fuzzy + mru;
            if (fuzzy > 0) {
                scored.push_back({a, s});
            }
        }
        std::sort(scored.begin(), scored.end(), [](const Scored& x, const Scored& y) {
            return x.score > y.score;
        });
    }

    // 4. Build the visible list with shortcut hints.
    actionList_->clear();
    for (const auto& s : scored) {
        const auto* action = s.action;
        if (!action) {
            continue;
        }
        const QString label = action->label().isEmpty() ? action->name() : action->label();
        const QString shortcut = action->shortcutText();
        const QString category = action->category();
        QString display = label;
        if (!shortcut.isEmpty()) {
            display = QStringLiteral("%1    %2").arg(label, shortcut);
        }
        if (!category.isEmpty()) {
            display = QStringLiteral("%1   [%2]").arg(display, category);
        }
        auto* item = new QListWidgetItem(display, actionList_);
        item->setData(Qt::UserRole, action->id());
        item->setToolTip(action->description().isEmpty() ? action->id() : action->description());
    }
    if (actionList_->count() > 0) {
        actionList_->setCurrentRow(0);
    }
}

void ArtifactCommandPaletteWidget::executeHighlighted()
{
    if (!actionList_) {
        return;
    }
    auto* item = actionList_->currentItem();
    if (!item) {
        return;
    }
    const QString actionId = item->data(Qt::UserRole).toString();
    if (actionId.isEmpty()) {
        return;
    }
    if (auto* mgr = ArtifactCore::ActionManager::instance()) {
        mgr->executeAction(actionId, QVariantMap());
        recordMruAction(actionId);
    }
    accept();
}

void ArtifactCommandPaletteWidget::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        QDialog::keyPressEvent(event);
        return;
    }
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        executeHighlighted();
        return;
    case Qt::Key_Down:
        if (actionList_) {
            const int next = actionList_->currentRow() + 1;
            if (next < actionList_->count()) {
                actionList_->setCurrentRow(next);
            }
        }
        return;
    case Qt::Key_Up:
        if (actionList_) {
            const int prev = actionList_->currentRow() - 1;
            if (prev >= 0) {
                actionList_->setCurrentRow(prev);
            }
        }
        return;
    default:
        QDialog::keyPressEvent(event);
        return;
    }
}

void ArtifactCommandPaletteWidget::bootDummyCommandPaletteActions()
{
    using ArtifactCore::ActionManager;
    auto* mgr = ActionManager::instance();
    if (!mgr) {
        return;
    }
    struct Dummy {
        const char* id;
        const char* name;
        const char* desc;
        const char* category;
    };
    static const Dummy kDummies[] = {
        {"palette.dummy.echo", "Echo", "Print a hello to stdout (dummy)", "Palette"},
        {"palette.dummy.about", "About Command Palette", "Show that the palette shell is alive (dummy)", "Palette"},
        {"palette.dummy.noop", "No-Op", "Does nothing (dummy)", "Palette"},
        {"palette.dummy.addMask", "Add Mask", "Add a mask to the selected layer (stub; menu integration pending)", "Layer"},
    };
    for (const auto& d : kDummies) {
        if (mgr->getAction(QString::fromUtf8(d.id))) {
            continue;
        }
        mgr->createAction(
            QString::fromUtf8(d.id),
            QString::fromUtf8(d.name),
            QString::fromUtf8(d.desc),
            [id = QString::fromUtf8(d.id)]() {
                std::fprintf(stdout, "[CommandPalette] executed: %s\n",
                             id.toUtf8().constData());
                std::fflush(stdout);
            });
    }
}

// ---- MRU implementation -------------------------------------------------
//
// Static, in-memory MRU. Persistence (QStandardPaths / project extension
// data) is intentionally out of scope for this cycle: the JSON helpers
// below expose a stable surface that a later cycle can wire through
// ArtifactProject::setExtensionData() without touching the palette itself.

namespace {
QStringList &mruStorage()
{
    static QStringList sList;
    return sList;
}

constexpr int kMruMaxEntries = 16;
constexpr int kMruTopBoost   = 1000;
} // namespace

const QStringList &ArtifactCommandPaletteWidget::mruList()
{
    return mruStorage();
}

void ArtifactCommandPaletteWidget::recordMruAction(const QString& actionId)
{
    if (actionId.isEmpty()) {
        return;
    }
    auto &list = mruStorage();
    // Remove existing occurrence so the new push is on top.
    list.removeAll(actionId);
    list.prepend(actionId);
    while (list.size() > kMruMaxEntries) {
        list.removeLast();
    }
}

int ArtifactCommandPaletteWidget::mruScoreFor(const QString& actionId)
{
    const auto &list = mruStorage();
    const int idx = list.indexOf(actionId);
    if (idx < 0) {
        return 0;
    }
    // Linear decay: top entry gets kMruTopBoost, second slightly less, etc.
    return kMruTopBoost - idx;
}

QJsonObject ArtifactCommandPaletteWidget::mruAsJson()
{
    QJsonObject root;
    QJsonArray arr;
    for (const auto &id : mruStorage()) {
        arr.append(id);
    }
    root.insert(QStringLiteral("ids"), arr);
    return root;
}

void ArtifactCommandPaletteWidget::loadMruFromJson(const QJsonObject &data)
{
    auto &list = mruStorage();
    list.clear();
    if (!data.contains(QStringLiteral("ids")) || !data.value(QStringLiteral("ids")).isArray()) {
        return;
    }
    const QJsonArray arr = data.value(QStringLiteral("ids")).toArray();
    for (const auto &v : arr) {
        if (v.isString()) {
            list.append(v.toString());
        }
        if (list.size() >= kMruMaxEntries) {
            break;
        }
    }
}

} // namespace Artifact
