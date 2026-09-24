module;
#include <algorithm>
#include <utility>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QPalette>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QUrl>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <wobjectimpl.h>
#include <functional>
#include <utility>

module Artifact.Widgets.CollabPresenceWidget;


namespace Artifact {

namespace detail {

class CollabConnectionButton final : public QPushButton {
public:
    using Callback = std::function<void()>;
    using QPushButton::QPushButton;

    void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
    void nextCheckState() override {
        QPushButton::nextCheckState();
        if (callback_) callback_();
    }

private:
    Callback callback_;
};

} // namespace detail

W_OBJECT_IMPL(CollabPresenceWidget)

class CollabPresenceWidget::Impl {
public:
    struct PresenceRow {
        QLabel* locationLabel = nullptr;
        QLabel* selectionLabel = nullptr;
    };

    QHash<QString, CollabPresenceWidget::UserPresence> users_;
    QHash<QString, PresenceRow> rows_;
    CollabPresenceWidget::UserPresence localUser_;
    QVBoxLayout* rosterLayout_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QLabel* emptyLabel_ = nullptr;
    QLineEdit* serverUrlEdit_ = nullptr;
    QLineEdit* projectIdEdit_ = nullptr;
    QLineEdit* userNameEdit_ = nullptr;
    QLineEdit* accessTokenEdit_ = nullptr;
    QLabel* connectionStatusLabel_ = nullptr;
    detail::CollabConnectionButton* connectionButton_ = nullptr;
    QLabel* layerLockStatusLabel_ = nullptr;
    detail::CollabConnectionButton* layerLockButton_ = nullptr;
    QLabel* reviewContextLabel_ = nullptr;
    QLabel* reviewStatusLabel_ = nullptr;
    QListWidget* reviewNotesList_ = nullptr;
    QLineEdit* reviewCommentEdit_ = nullptr;
    detail::CollabConnectionButton* addReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* replyReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* editReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* reviewHistoryButton_ = nullptr;
    detail::CollabConnectionButton* deleteReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* resolveReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* reopenReviewCommentButton_ = nullptr;
    detail::CollabConnectionButton* jumpReviewAnchorButton_ = nullptr;
    std::function<void(const QString&, const QString&, const QString&,
                       const QString&)>
        connectionActionHandler_;
    std::function<void(const QString&, bool)> layerLockActionHandler_;
    std::function<bool(const QString&)> reviewCommentHandler_;
    std::function<bool(const QString&, const QString&)> reviewReplyHandler_;
    std::function<bool(const QString&, bool)> reviewResolveHandler_;
    std::function<bool(const QString&, const QString&)> reviewEditHandler_;
    std::function<bool(const QString&)> reviewDeleteHandler_;
    std::function<bool(const QString&)> reviewJumpHandler_;
    QString reviewCompositionId_;
    QString reviewLayerId_;
    bool reviewConnected_ = false;
    bool reviewCanEdit_ = false;
    QString selectedLayerId_;
    QString layerLockOwnerName_;
    QString layerLockReason_;
    bool layerLocked_ = false;
    bool layerHeldByLocalUser_ = false;
    bool layerLockPending_ = false;
    bool layerLockConnected_ = false;
    bool connectionActive_ = false;
};

CollabPresenceWidget::CollabPresenceWidget(QWidget* parent)
    : QFrame(parent)
    , impl_(new Impl())
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setMinimumWidth(220);
    setMaximumWidth(360);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("COLLABORATION"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    if (titleFont.pointSize() > 0) {
        titleFont.setPointSize(titleFont.pointSize() + 1);
    }
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* connectionTitle = new QLabel(QStringLiteral("Session"), this);
    QFont connectionTitleFont = connectionTitle->font();
    connectionTitleFont.setBold(true);
    connectionTitle->setFont(connectionTitleFont);
    layout->addWidget(connectionTitle);

    auto* serverLabel = new QLabel(QStringLiteral("Server"), this);
    impl_->serverUrlEdit_ = new QLineEdit(this);
    impl_->serverUrlEdit_->setPlaceholderText(QStringLiteral("ws://localhost:8080"));
    impl_->serverUrlEdit_->setAccessibleName(QStringLiteral("Collaboration server URL"));
    serverLabel->setBuddy(impl_->serverUrlEdit_);
    layout->addWidget(serverLabel);
    layout->addWidget(impl_->serverUrlEdit_);

    auto* projectLabel = new QLabel(QStringLiteral("Shared Project ID"), this);
    impl_->projectIdEdit_ = new QLineEdit(this);
    impl_->projectIdEdit_->setPlaceholderText(QStringLiteral("Enter the shared project ID"));
    impl_->projectIdEdit_->setAccessibleName(QStringLiteral("Shared project ID"));
    projectLabel->setBuddy(impl_->projectIdEdit_);
    layout->addWidget(projectLabel);
    layout->addWidget(impl_->projectIdEdit_);

    auto* userLabel = new QLabel(QStringLiteral("Display Name"), this);
    impl_->userNameEdit_ = new QLineEdit(this);
    impl_->userNameEdit_->setPlaceholderText(QStringLiteral("Artist"));
    impl_->userNameEdit_->setAccessibleName(QStringLiteral("Collaboration display name"));
    userLabel->setBuddy(impl_->userNameEdit_);
    layout->addWidget(userLabel);
    layout->addWidget(impl_->userNameEdit_);

    auto* accessTokenLabel = new QLabel(QStringLiteral("Access Token (if required)"), this);
    impl_->accessTokenEdit_ = new QLineEdit(this);
    impl_->accessTokenEdit_->setEchoMode(QLineEdit::Password);
    impl_->accessTokenEdit_->setPlaceholderText(QStringLiteral("Not stored in settings"));
    impl_->accessTokenEdit_->setAccessibleName(QStringLiteral("Collaboration access token"));
    accessTokenLabel->setBuddy(impl_->accessTokenEdit_);
    layout->addWidget(accessTokenLabel);
    layout->addWidget(impl_->accessTokenEdit_);

    auto* prototypeNotice = new QLabel(
        QStringLiteral("When server token authentication is disabled, anyone with the project ID can join. Access tokens are not stored in settings; review history is stored on the server."),
        this);
    prototypeNotice->setWordWrap(true);
    QPalette noticePalette = prototypeNotice->palette();
    noticePalette.setColor(QPalette::WindowText,
                          palette().color(QPalette::Disabled,
                                          QPalette::WindowText));
    prototypeNotice->setPalette(noticePalette);
    layout->addWidget(prototypeNotice);

    auto* sessionActions = new QHBoxLayout();
    sessionActions->setContentsMargins(0, 0, 0, 0);
    sessionActions->setSpacing(8);
    impl_->connectionButton_ =
        new detail::CollabConnectionButton(QStringLiteral("Connect"), this);
    impl_->connectionButton_->setMinimumHeight(30);
    impl_->connectionButton_->setAccessibleName(QStringLiteral("Connect to collaboration session"));
    impl_->connectionStatusLabel_ = new QLabel(QStringLiteral("Disconnected"), this);
    QPalette statusPalette = impl_->connectionStatusLabel_->palette();
    statusPalette.setColor(QPalette::WindowText,
                           palette().color(QPalette::Disabled,
                                           QPalette::WindowText));
    impl_->connectionStatusLabel_->setPalette(statusPalette);
    sessionActions->addWidget(impl_->connectionButton_);
    sessionActions->addWidget(impl_->connectionStatusLabel_, 1);
    layout->addLayout(sessionActions);

    QSettings settings;
    settings.beginGroup(QStringLiteral("Collaboration"));
    const QString configuredServerUrl =
        settings.value(QStringLiteral("serverUrl")).toString();
    impl_->serverUrlEdit_->setText(
        configuredServerUrl.isEmpty()
            ? QStringLiteral("ws://localhost:8080")
            : configuredServerUrl);
    impl_->projectIdEdit_->setText(
        settings.value(QStringLiteral("projectId")).toString());
    const QString configuredUserName =
        settings.value(QStringLiteral("userName")).toString();
    impl_->userNameEdit_->setText(configuredUserName.isEmpty()
                                      ? QStringLiteral("Artist")
                                      : configuredUserName);
    settings.endGroup();

    impl_->connectionButton_->setCallback([this]() {
        if (!impl_->connectionActionHandler_) return;
        if (impl_->connectionActive_) {
            impl_->connectionActionHandler_(QString(), QString(), QString(), QString());
            return;
        }
        const QString serverUrl = impl_->serverUrlEdit_->text().trimmed();
        const QString projectId = impl_->projectIdEdit_->text().trimmed();
        const QString userName = impl_->userNameEdit_->text().trimmed();
        const QString accessToken = impl_->accessTokenEdit_->text();
        const QUrl url(serverUrl);
        if ((url.scheme() != QStringLiteral("ws") &&
             url.scheme() != QStringLiteral("wss")) || url.host().isEmpty()) {
            setConnectionStatus(QStringLiteral("Enter a valid ws:// or wss:// URL"),
                                false);
            return;
        }
        if (projectId.isEmpty() || userName.isEmpty()) {
            setConnectionStatus(QStringLiteral("Project ID and name are required"),
                                false);
            return;
        }
        QSettings settings;
        settings.beginGroup(QStringLiteral("Collaboration"));
        settings.setValue(QStringLiteral("serverUrl"), serverUrl);
        settings.setValue(QStringLiteral("projectId"), projectId);
        settings.setValue(QStringLiteral("userName"), userName);
        settings.endGroup();
        impl_->connectionActionHandler_(serverUrl, projectId, userName, accessToken);
    });

    auto* rosterHeader = new QHBoxLayout();
    rosterHeader->setContentsMargins(0, 0, 0, 0);
    rosterHeader->setSpacing(6);
    auto* rosterTitle = new QLabel(QStringLiteral("Connected Users"), this);
    QFont rosterFont = rosterTitle->font();
    rosterFont.setBold(true);
    rosterTitle->setFont(rosterFont);
    rosterHeader->addWidget(rosterTitle);
    rosterHeader->addStretch(1);
    impl_->countLabel_ = new QLabel(QStringLiteral("0"), this);
    rosterHeader->addWidget(impl_->countLabel_);
    layout->addLayout(rosterHeader);

    impl_->rosterLayout_ = new QVBoxLayout();
    impl_->rosterLayout_->setContentsMargins(0, 0, 0, 0);
    impl_->rosterLayout_->setSpacing(4);
    layout->addLayout(impl_->rosterLayout_);

    auto* lockTitle = new QLabel(QStringLiteral("Selected Layer Lock"), this);
    QFont lockTitleFont = lockTitle->font();
    lockTitleFont.setBold(true);
    lockTitle->setFont(lockTitleFont);
    layout->addWidget(lockTitle);
    auto* lockActions = new QHBoxLayout();
    lockActions->setContentsMargins(0, 0, 0, 0);
    lockActions->setSpacing(8);
    impl_->layerLockButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Request Lock"), this);
    impl_->layerLockButton_->setMinimumHeight(28);
    impl_->layerLockButton_->setAccessibleName(
        QStringLiteral("Request or release lock for the selected layer"));
    impl_->layerLockButton_->setEnabled(false);
    impl_->layerLockStatusLabel_ = new QLabel(
        QStringLiteral("Select one layer to request a lock"), this);
    impl_->layerLockStatusLabel_->setWordWrap(true);
    lockActions->addWidget(impl_->layerLockButton_);
    lockActions->addWidget(impl_->layerLockStatusLabel_, 1);
    layout->addLayout(lockActions);
    impl_->layerLockButton_->setCallback([this]() {
        if (!impl_->layerLockActionHandler_ || impl_->selectedLayerId_.isEmpty() ||
            impl_->layerLockPending_ ||
            (impl_->layerLocked_ && !impl_->layerHeldByLocalUser_)) {
            return;
        }
        impl_->layerLockPending_ = true;
        setLayerLockControlState(
            impl_->selectedLayerId_, impl_->connectionActive_,
            impl_->layerLocked_, impl_->layerHeldByLocalUser_, true,
            impl_->layerLockOwnerName_);
        impl_->layerLockActionHandler_(impl_->selectedLayerId_,
                                       !impl_->layerHeldByLocalUser_);
    });

    auto* reviewTitle = new QLabel(QStringLiteral("Review Notes"), this);
    QFont reviewTitleFont = reviewTitle->font();
    reviewTitleFont.setBold(true);
    reviewTitle->setFont(reviewTitleFont);
    layout->addWidget(reviewTitle);
    impl_->reviewContextLabel_ = new QLabel(
        QStringLiteral("Connect and select a Composition to add a note"), this);
    impl_->reviewContextLabel_->setWordWrap(true);
    layout->addWidget(impl_->reviewContextLabel_);
    impl_->reviewNotesList_ = new QListWidget(this);
    impl_->reviewNotesList_->setAccessibleName(
        QStringLiteral("Shared review notes for the current Composition"));
    impl_->reviewNotesList_->setMaximumHeight(150);
    layout->addWidget(impl_->reviewNotesList_);
    auto* reviewActions = new QHBoxLayout();
    reviewActions->setContentsMargins(0, 0, 0, 0);
    reviewActions->setSpacing(8);
    impl_->reviewCommentEdit_ = new QLineEdit(this);
    impl_->reviewCommentEdit_->setPlaceholderText(
        QStringLiteral("New note, reply, or replacement text"));
    impl_->reviewCommentEdit_->setMaxLength(4096);
    impl_->reviewCommentEdit_->setAccessibleName(
        QStringLiteral("New shared review note"));
    impl_->addReviewCommentButton_ =
        new detail::CollabConnectionButton(QStringLiteral("Add Note"), this);
    impl_->addReviewCommentButton_->setMinimumHeight(28);
    impl_->addReviewCommentButton_->setEnabled(false);
    reviewActions->addWidget(impl_->reviewCommentEdit_, 1);
    reviewActions->addWidget(impl_->addReviewCommentButton_);
    layout->addLayout(reviewActions);
    impl_->reviewStatusLabel_ = new QLabel(QString(), this);
    impl_->reviewStatusLabel_->setWordWrap(true);
    layout->addWidget(impl_->reviewStatusLabel_);
    auto* reviewThreadActions = new QHBoxLayout();
    reviewThreadActions->setContentsMargins(0, 0, 0, 0);
    reviewThreadActions->setSpacing(6);
    impl_->replyReviewCommentButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Reply"), this);
    impl_->editReviewCommentButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Edit"), this);
    impl_->reviewHistoryButton_ = new detail::CollabConnectionButton(
        QStringLiteral("History"), this);
    impl_->reviewHistoryButton_->setAccessibleName(
        QStringLiteral("View selected review note edit history"));
    impl_->deleteReviewCommentButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Delete"), this);
    impl_->resolveReviewCommentButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Resolve"), this);
    impl_->reopenReviewCommentButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Reopen"), this);
    impl_->jumpReviewAnchorButton_ = new detail::CollabConnectionButton(
        QStringLiteral("Jump to Anchor"), this);
    impl_->jumpReviewAnchorButton_->setAccessibleName(
        QStringLiteral("Jump to the selected review note anchor"));
    impl_->replyReviewCommentButton_->setEnabled(false);
    impl_->editReviewCommentButton_->setEnabled(false);
    impl_->reviewHistoryButton_->setEnabled(false);
    impl_->deleteReviewCommentButton_->setEnabled(false);
    impl_->resolveReviewCommentButton_->setEnabled(false);
    impl_->reopenReviewCommentButton_->setEnabled(false);
    reviewThreadActions->addWidget(impl_->replyReviewCommentButton_);
    reviewThreadActions->addWidget(impl_->editReviewCommentButton_);
    reviewThreadActions->addWidget(impl_->reviewHistoryButton_);
    reviewThreadActions->addWidget(impl_->deleteReviewCommentButton_);
    reviewThreadActions->addStretch(1);
    layout->addLayout(reviewThreadActions);
    auto* reviewDecisionActions = new QHBoxLayout();
    reviewDecisionActions->setContentsMargins(0, 0, 0, 0);
    reviewDecisionActions->setSpacing(6);
    reviewDecisionActions->addWidget(impl_->resolveReviewCommentButton_);
    reviewDecisionActions->addWidget(impl_->reopenReviewCommentButton_);
    reviewDecisionActions->addStretch(1);
    layout->addLayout(reviewDecisionActions);
    auto* reviewAnchorActions = new QHBoxLayout();
    reviewAnchorActions->setContentsMargins(0, 0, 0, 0);
    reviewAnchorActions->setSpacing(6);
    impl_->jumpReviewAnchorButton_->setEnabled(false);
    reviewAnchorActions->addWidget(impl_->jumpReviewAnchorButton_);
    reviewAnchorActions->addStretch(1);
    layout->addLayout(reviewAnchorActions);
    impl_->addReviewCommentButton_->setCallback([this]() {
        if (!impl_->reviewConnected_ || impl_->reviewCompositionId_.isEmpty() ||
            !impl_->reviewCommentHandler_) {
            return;
        }
        const QString text = impl_->reviewCommentEdit_->text().trimmed();
        if (text.isEmpty()) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Enter a note before adding it"));
            return;
        }
        if (impl_->reviewCommentHandler_(text)) {
            impl_->reviewCommentEdit_->clear();
            impl_->reviewStatusLabel_->setText(QStringLiteral("Note sent"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Could not share the note"));
        }
    });
    impl_->replyReviewCommentButton_->setCallback([this]() {
        if (!impl_->reviewReplyHandler_ || !impl_->reviewConnected_) return;
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note to reply to"));
            return;
        }
        QString commentId = item->data(Qt::UserRole).toString();
        const QString parentCommentId =
            item->data(Qt::UserRole + 1).toString();
        if (!parentCommentId.isEmpty()) commentId = parentCommentId;
        const QString text = impl_->reviewCommentEdit_->text().trimmed();
        if (text.isEmpty()) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Enter a reply before sending it"));
            return;
        }
        if (impl_->reviewReplyHandler_(commentId, text)) {
            impl_->reviewCommentEdit_->clear();
            impl_->reviewStatusLabel_->setText(QStringLiteral("Reply sent"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Could not share the reply"));
        }
    });
    impl_->editReviewCommentButton_->setCallback([this]() {
        if (!impl_->reviewEditHandler_ || !impl_->reviewConnected_) return;
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note to edit"));
            return;
        }
        const QString text = impl_->reviewCommentEdit_->text().trimmed();
        if (text.isEmpty()) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Enter replacement text before editing"));
            return;
        }
        if (impl_->reviewEditHandler_(
                item->data(Qt::UserRole).toString(), text)) {
            impl_->reviewCommentEdit_->clear();
            impl_->reviewStatusLabel_->setText(QStringLiteral("Update sent"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Only the note author can edit it"));
        }
    });
    impl_->reviewHistoryButton_->setCallback([this]() {
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note to view its edit history"));
            return;
        }
        if (item->data(Qt::UserRole + 2).toBool()) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Edit history is hidden for deleted notes"));
            return;
        }
        QString history = item->data(Qt::UserRole + 3).toString();
        if (history.isEmpty()) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("This note has no previous edits"));
            return;
        }
        if (item->data(Qt::UserRole + 4).toBool()) {
            history.prepend(
                QStringLiteral("Showing the most recent 50 edits.\n\n"));
        }
        QMessageBox::information(this, QStringLiteral("Review note edit history"),
                                 history);
    });
    impl_->deleteReviewCommentButton_->setCallback([this]() {
        if (!impl_->reviewDeleteHandler_ || !impl_->reviewConnected_) return;
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note to delete"));
            return;
        }
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Delete review note"),
            QStringLiteral("Delete this shared note? Replies remain visible."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        if (impl_->reviewDeleteHandler_(
                item->data(Qt::UserRole).toString())) {
            impl_->reviewStatusLabel_->setText(QStringLiteral("Deletion sent"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Only the note author can delete it"));
        }
    });
    const auto resolveSelectedReviewThread = [this](const bool resolved) {
        if (!impl_->reviewResolveHandler_ || !impl_->reviewConnected_) return;
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note first"));
            return;
        }
        QString commentId = item->data(Qt::UserRole).toString();
        const QString parentCommentId =
            item->data(Qt::UserRole + 1).toString();
        if (!parentCommentId.isEmpty()) commentId = parentCommentId;
        if (impl_->reviewResolveHandler_(commentId, resolved)) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Thread update sent"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Could not update the thread"));
        }
    };
    impl_->resolveReviewCommentButton_->setCallback(
        [resolveSelectedReviewThread]() { resolveSelectedReviewThread(true); });
    impl_->reopenReviewCommentButton_->setCallback(
        [resolveSelectedReviewThread]() { resolveSelectedReviewThread(false); });
    impl_->jumpReviewAnchorButton_->setCallback([this]() {
        if (!impl_->reviewJumpHandler_ || !impl_->reviewConnected_) return;
        const auto* item = impl_->reviewNotesList_->currentItem();
        if (!item) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Select a note to jump to its anchor"));
            return;
        }
        if (impl_->reviewJumpHandler_(item->data(Qt::UserRole).toString())) {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("Moved to the note anchor"));
        } else {
            impl_->reviewStatusLabel_->setText(
                QStringLiteral("The note anchor is unavailable"));
        }
    });
    layout->addStretch(1);

    refreshPresenceRows();
}

CollabPresenceWidget::~CollabPresenceWidget() {
    delete impl_;
}

void CollabPresenceWidget::addUser(const QString& userId, const QString& userName, const QColor& color) {
    CollabPresenceWidget::UserPresence presence;
    presence.userId = userId;
    presence.userName = userName;
    presence.color = color;
    impl_->users_[userId] = presence;
    refreshPresenceRows();
}

void CollabPresenceWidget::updateUser(const QString& userId, const QJsonObject& presence) {
    if (impl_->localUser_.userId == userId) {
        if (presence.contains(QStringLiteral("cursorLocation"))) {
            impl_->localUser_.cursorLocation =
                presence.value(QStringLiteral("cursorLocation")).toString();
        }
        if (presence.contains(QStringLiteral("selectedLayers"))) {
            impl_->localUser_.selectedLayers.clear();
            const QJsonArray layers =
                presence.value(QStringLiteral("selectedLayers")).toArray();
            for (const auto& layer : layers) {
                impl_->localUser_.selectedLayers.append(layer.toString());
            }
        }
        updatePresenceRow(impl_->localUser_);
        return;
    }

    auto it = impl_->users_.find(userId);
    if (it != impl_->users_.end()) {
        if (presence.contains(QStringLiteral("cursorLocation"))) {
            it->cursorLocation = presence.value(QStringLiteral("cursorLocation")).toString();
        }
        if (presence.contains(QStringLiteral("selectedLayers"))) {
            const QJsonArray layers = presence.value(QStringLiteral("selectedLayers")).toArray();
            it->selectedLayers.clear();
            for (const auto& layer : layers) {
                it->selectedLayers.append(layer.toString());
            }
        }
        updatePresenceRow(it.value());
    }
}

void CollabPresenceWidget::removeUser(const QString& userId) {
    impl_->users_.remove(userId);
    refreshPresenceRows();
}

void CollabPresenceWidget::clearUsers() {
    impl_->users_.clear();
    refreshPresenceRows();
}

void CollabPresenceWidget::setLocalUser(const QString& userId, const QString& userName, const QColor& color) {
    impl_->localUser_.userId = userId;
    impl_->localUser_.userName = userName;
    impl_->localUser_.color = color;
    refreshPresenceRows();
}

void CollabPresenceWidget::setConnectionActionHandler(
    std::function<void(const QString&, const QString&, const QString&,
                       const QString&)> handler) {
    impl_->connectionActionHandler_ = std::move(handler);
}

void CollabPresenceWidget::setLayerLockActionHandler(
    std::function<void(const QString&, bool)> handler) {
    impl_->layerLockActionHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewCommentHandler(
    std::function<bool(const QString&)> handler) {
    impl_->reviewCommentHandler_ = std::move(handler);
}

void CollabPresenceWidget::setLayerEditBlockedStatus(const QString& reason) {
    if (impl_->layerLockStatusLabel_) {
        impl_->layerLockStatusLabel_->setText(reason);
    }
}

void CollabPresenceWidget::setReviewReplyHandler(
    std::function<bool(const QString&, const QString&)> handler) {
    impl_->reviewReplyHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewResolveHandler(
    std::function<bool(const QString&, bool)> handler) {
    impl_->reviewResolveHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewEditHandler(
    std::function<bool(const QString&, const QString&)> handler) {
    impl_->reviewEditHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewDeleteHandler(
    std::function<bool(const QString&)> handler) {
    impl_->reviewDeleteHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewJumpHandler(
    std::function<bool(const QString&)> handler) {
    impl_->reviewJumpHandler_ = std::move(handler);
}

void CollabPresenceWidget::setReviewContext(const QString& compositionId,
                                            const QString& layerId,
                                            const bool connected,
                                            const bool canEdit) {
    if (impl_->reviewCompositionId_ == compositionId &&
        impl_->reviewLayerId_ == layerId &&
        impl_->reviewConnected_ == connected &&
        impl_->reviewCanEdit_ == canEdit) {
        return;
    }
    impl_->reviewCompositionId_ = compositionId;
    impl_->reviewLayerId_ = layerId;
    impl_->reviewConnected_ = connected;
    impl_->reviewCanEdit_ = canEdit;
    if (impl_->reviewContextLabel_) {
        if (!connected) {
            impl_->reviewContextLabel_->setText(
                QStringLiteral("Connect to a session to view shared notes"));
        } else if (compositionId.isEmpty()) {
            impl_->reviewContextLabel_->setText(
                QStringLiteral("Open a Composition to view shared notes"));
        } else {
            QString anchor = QStringLiteral("Composition %1").arg(compositionId);
            if (!layerId.isEmpty()) anchor += QStringLiteral(" · Selected layer");
            if (!canEdit) anchor += QStringLiteral(" · Read-only session");
            anchor += QStringLiteral(" · Notes include the current frame when available");
            impl_->reviewContextLabel_->setText(anchor);
        }
    }
    if (impl_->addReviewCommentButton_) {
        impl_->addReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->replyReviewCommentButton_) {
        impl_->replyReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->editReviewCommentButton_) {
        impl_->editReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->reviewHistoryButton_) {
        impl_->reviewHistoryButton_->setEnabled(
            connected && !compositionId.isEmpty());
    }
    if (impl_->deleteReviewCommentButton_) {
        impl_->deleteReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->resolveReviewCommentButton_) {
        impl_->resolveReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->reopenReviewCommentButton_) {
        impl_->reopenReviewCommentButton_->setEnabled(
            canEdit && !compositionId.isEmpty());
    }
    if (impl_->jumpReviewAnchorButton_) {
        impl_->jumpReviewAnchorButton_->setEnabled(
            connected && !compositionId.isEmpty());
    }
}

void CollabPresenceWidget::setReviewNotes(const QList<ReviewNote>& notes) {
    if (!impl_->reviewNotesList_) return;
    impl_->reviewNotesList_->clear();
    for (const ReviewNote& note : notes) {
        const QString prefix = note.parentCommentId.isEmpty()
                                   ? (note.resolved
                                          ? QStringLiteral("Resolved · ")
                                          : QString())
                                   : QStringLiteral("↳ ");
        const QString author = note.anchor.isEmpty()
                                   ? note.authorName
                                   : QStringLiteral("%1 (%2)")
                                         .arg(note.authorName, note.anchor);
        const QString noteText = note.deleted
                                     ? QStringLiteral("[deleted by author]")
                                     : note.text;
        const QString title = QStringLiteral("%1%2: %3")
                                  .arg(prefix, author, noteText);
        auto* item = new QListWidgetItem(title, impl_->reviewNotesList_);
        item->setToolTip(note.anchor);
        item->setData(Qt::UserRole, note.commentId);
        item->setData(Qt::UserRole + 1, note.parentCommentId);
        item->setData(Qt::UserRole + 2, note.deleted);
        item->setData(Qt::UserRole + 3, note.revisionHistory.join(QStringLiteral("\n\n")));
        item->setData(Qt::UserRole + 4, note.revisionHistoryTruncated);
    }
    if (impl_->reviewStatusLabel_) {
        impl_->reviewStatusLabel_->setText(
            notes.isEmpty() ? QStringLiteral("No shared notes for this Composition")
                            : QStringLiteral("%1 shared note(s)").arg(notes.size()));
    }
}

void CollabPresenceWidget::setConnectionStatus(const QString& statusText,
                                               const bool active) {
    impl_->connectionActive_ = active;
    if (impl_->connectionStatusLabel_) {
        impl_->connectionStatusLabel_->setText(statusText);
    }
    if (impl_->connectionButton_) {
        impl_->connectionButton_->setText(active ? QStringLiteral("Disconnect")
                                                : QStringLiteral("Connect"));
        impl_->connectionButton_->setAccessibleName(
            active ? QStringLiteral("Disconnect from collaboration session")
                   : QStringLiteral("Connect to collaboration session"));
    }
    setLayerLockControlState(impl_->selectedLayerId_,
                             impl_->layerLockConnected_,
                             impl_->layerLocked_,
                             impl_->layerHeldByLocalUser_,
                             impl_->layerLockPending_,
                             impl_->layerLockOwnerName_,
                             impl_->layerLockReason_);
}

void CollabPresenceWidget::setLayerLockControlState(
    const QString& selectedLayerId, const bool connected, const bool locked,
    const bool heldByLocalUser, const bool pending, const QString& ownerName,
    const QString& reason) {
    if (impl_->selectedLayerId_ == selectedLayerId &&
        impl_->layerLockConnected_ == connected &&
        impl_->layerLocked_ == locked &&
        impl_->layerHeldByLocalUser_ == heldByLocalUser &&
        impl_->layerLockPending_ == pending &&
        impl_->layerLockOwnerName_ == ownerName &&
        impl_->layerLockReason_ == reason) {
        return;
    }
    impl_->selectedLayerId_ = selectedLayerId;
    impl_->layerLockConnected_ = connected;
    impl_->layerLocked_ = locked;
    impl_->layerHeldByLocalUser_ = heldByLocalUser;
    impl_->layerLockPending_ = pending;
    impl_->layerLockOwnerName_ = ownerName;
    impl_->layerLockReason_ = reason;

    const bool hasSelection = !selectedLayerId.isEmpty();
    if (impl_->layerLockButton_) {
        impl_->layerLockButton_->setEnabled(
            connected && hasSelection && !pending &&
            (!locked || heldByLocalUser));
        impl_->layerLockButton_->setText(
            heldByLocalUser ? QStringLiteral("Release Lock")
                            : QStringLiteral("Request Lock"));
    }
    if (!impl_->layerLockStatusLabel_) return;

    QString status;
    if (!connected) {
        status = QStringLiteral("Connect to a session to reserve a layer");
    } else if (!hasSelection) {
        status = QStringLiteral("Select exactly one layer to request a lock");
    } else if (pending) {
        status = QStringLiteral("Waiting for server lock confirmation");
    } else if (locked && heldByLocalUser) {
        status = QStringLiteral("Reserved by you");
    } else if (locked) {
        status = ownerName.isEmpty()
                     ? QStringLiteral("Locked by another collaborator")
                     : QStringLiteral("Locked by %1").arg(ownerName);
    } else if (!reason.isEmpty()) {
        status = reason;
    } else {
        status = QStringLiteral(
            "No lock on the selected layer; reserve it before editing");
    }
    impl_->layerLockStatusLabel_->setText(status);
}

void CollabPresenceWidget::syncRemoteUsers(const QList<UserPresence>& users) {
    QSet<QString> desiredIds;
    const QString localId = impl_->localUser_.userId;
    bool rebuildRows = false;
    for (const UserPresence& incoming : users) {
        if (incoming.userId.isEmpty() || incoming.userId == localId) continue;
        desiredIds.insert(incoming.userId);
        auto it = impl_->users_.find(incoming.userId);
        if (it == impl_->users_.end()) {
            impl_->users_.insert(incoming.userId, incoming);
            rebuildRows = true;
            continue;
        }
        if (it->userName != incoming.userName || it->color != incoming.color) {
            it.value() = incoming;
            rebuildRows = true;
            continue;
        }
        const bool rowChanged =
            it->cursorLocation != incoming.cursorLocation ||
            it->selectedLayers != incoming.selectedLayers ||
            it->selectedLayerNames != incoming.selectedLayerNames;
        it->cursorLocation = incoming.cursorLocation;
        it->selectedLayers = incoming.selectedLayers;
        it->selectedLayerNames = incoming.selectedLayerNames;
        if (rowChanged) updatePresenceRow(it.value());
    }
    for (auto it = impl_->users_.begin(); it != impl_->users_.end();) {
        if (!desiredIds.contains(it.key())) {
            it = impl_->users_.erase(it);
            rebuildRows = true;
        } else {
            ++it;
        }
    }
    if (rebuildRows) refreshPresenceRows();
}

void CollabPresenceWidget::refreshPresenceRows() {
    if (!impl_->rosterLayout_) {
        return;
    }

    impl_->emptyLabel_ = nullptr;
    impl_->rows_.clear();
    while (QLayoutItem* item = impl_->rosterLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    QList<UserPresence> roster;
    const bool hasLocalUser = !impl_->localUser_.userId.isEmpty();
    if (hasLocalUser) {
        roster.append(impl_->localUser_);
    }
    for (auto it = impl_->users_.cbegin(); it != impl_->users_.cend(); ++it) {
        if (it.key() != impl_->localUser_.userId) {
            roster.append(it.value());
        }
    }
    std::sort(roster.begin() + (hasLocalUser ? 1 : 0), roster.end(),
              [](const UserPresence& lhs, const UserPresence& rhs) {
                  return lhs.userName.localeAwareCompare(rhs.userName) < 0;
              });

    if (impl_->countLabel_) {
        impl_->countLabel_->setText(QString::number(roster.size()));
    }

    if (roster.isEmpty()) {
        impl_->emptyLabel_ =
            new QLabel(QStringLiteral("No collaborators connected"), this);
        QPalette emptyPalette = impl_->emptyLabel_->palette();
        emptyPalette.setColor(QPalette::WindowText,
                              palette().color(QPalette::Disabled,
                                              QPalette::WindowText));
        impl_->emptyLabel_->setPalette(emptyPalette);
        impl_->rosterLayout_->addWidget(impl_->emptyLabel_);
        return;
    }

    for (const UserPresence& presence : roster) {
        const bool isLocal = hasLocalUser &&
                             presence.userId == impl_->localUser_.userId;
        auto* row = new QFrame(this);
        row->setObjectName(isLocal ? QStringLiteral("collabPresenceLocalRow")
                                   : QStringLiteral("collabPresenceUserRow"));
        row->setFrameShape(QFrame::StyledPanel);
        row->setFrameShadow(QFrame::Plain);
        row->setProperty("userId", presence.userId);
        if (isLocal) {
            const QColor base = palette().color(QPalette::Window);
            const QColor accent = palette().color(QPalette::Highlight);
            const QColor focusedSurface(
                (base.red() * 7 + accent.red()) / 8,
                (base.green() * 7 + accent.green()) / 8,
                (base.blue() * 7 + accent.blue()) / 8);
            QPalette rowPalette = row->palette();
            rowPalette.setColor(QPalette::Window, focusedSurface);
            row->setPalette(rowPalette);
            row->setAutoFillBackground(true);
        }

        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 7, 8, 7);
        rowLayout->setSpacing(8);

        auto* identityDot = new QLabel(QStringLiteral("●"), row);
        identityDot->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        identityDot->setFixedWidth(16);
        QPalette dotPalette = identityDot->palette();
        dotPalette.setColor(QPalette::WindowText,
                            presence.color.isValid()
                                ? presence.color
                                : palette().color(QPalette::Highlight));
        identityDot->setPalette(dotPalette);
        rowLayout->addWidget(identityDot);

        auto* textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        const QString displayName =
            isLocal ? QStringLiteral("%1  (You)").arg(presence.userName)
                    : presence.userName;
        auto* nameLabel = new QLabel(displayName, row);
        QFont nameFont = nameLabel->font();
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        textLayout->addWidget(nameLabel);

        const QString location = presence.cursorLocation.trimmed().isEmpty()
                                     ? QStringLiteral("Workspace")
                                     : presence.cursorLocation.trimmed();
        auto* locationLabel = new QLabel(location, row);
        QPalette secondaryPalette = locationLabel->palette();
        secondaryPalette.setColor(QPalette::WindowText,
                                  palette().color(QPalette::Disabled,
                                                  QPalette::WindowText));
        locationLabel->setPalette(secondaryPalette);
        textLayout->addWidget(locationLabel);

        const qsizetype selectedCount = presence.selectedLayers.size();
        QString selectionText;
        if (!presence.selectedLayerNames.isEmpty()) {
            const qsizetype visibleNameCount =
                std::min<qsizetype>(presence.selectedLayerNames.size(), 3);
            QStringList visibleNames =
                presence.selectedLayerNames.mid(0, visibleNameCount);
            selectionText = visibleNames.join(QStringLiteral(", "));
            if (selectedCount > visibleNameCount) {
                selectionText += QStringLiteral(" +%1").arg(
                    selectedCount - visibleNameCount);
            }
        } else {
            selectionText = selectedCount == 1
                ? QStringLiteral("1 layer selected")
                : QStringLiteral("%1 layers selected").arg(selectedCount);
        }
        auto* selectionLabel = new QLabel(selectionText, row);
        selectionLabel->setWordWrap(true);
        selectionLabel->setPalette(secondaryPalette);
        textLayout->addWidget(selectionLabel);
        rowLayout->addLayout(textLayout, 1);

        impl_->rows_.insert(
            presence.userId,
            Impl::PresenceRow{locationLabel, selectionLabel});
        impl_->rosterLayout_->addWidget(row);
    }
}

void CollabPresenceWidget::updatePresenceRow(const UserPresence& presence) {
    const auto rowIt = impl_->rows_.find(presence.userId);
    if (rowIt == impl_->rows_.end()) {
        refreshPresenceRows();
        return;
    }

    if (rowIt->locationLabel) {
        rowIt->locationLabel->setText(
            presence.cursorLocation.trimmed().isEmpty()
                ? QStringLiteral("Workspace")
                : presence.cursorLocation.trimmed());
    }
    if (rowIt->selectionLabel) {
        const qsizetype selectedCount = presence.selectedLayers.size();
        QString selectionText;
        if (!presence.selectedLayerNames.isEmpty()) {
            const qsizetype visibleNameCount =
                std::min<qsizetype>(presence.selectedLayerNames.size(), 3);
            QStringList visibleNames =
                presence.selectedLayerNames.mid(0, visibleNameCount);
            selectionText = visibleNames.join(QStringLiteral(", "));
            if (selectedCount > visibleNameCount) {
                selectionText += QStringLiteral(" +%1").arg(
                    selectedCount - visibleNameCount);
            }
        } else {
            selectionText = selectedCount == 1
                ? QStringLiteral("1 layer selected")
                : QStringLiteral("%1 layers selected").arg(selectedCount);
        }
        rowIt->selectionLabel->setText(selectionText);
    }
}

QList<CollabPresenceWidget::UserPresence> CollabPresenceWidget::users() const {
    QList<CollabPresenceWidget::UserPresence> list;
    for (auto it = impl_->users_.begin(); it != impl_->users_.end(); ++it) {
        list.append(it.value());
    }
    return list;
}

void CollabPresenceWidget::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);

    // 追加の描画が必要な場合はここに実装
    // 現状はQLabelで十分だが、将来的にカーソル位置の可視化などを追加可能
}

} // namespace Artifact
