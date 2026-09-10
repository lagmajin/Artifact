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
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <wobjectimpl.h>

module Artifact.Widgets.CollabPresenceWidget;


namespace Artifact {

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
        const QString selectionText =
            selectedCount == 1
                ? QStringLiteral("1 layer selected")
                : QStringLiteral("%1 layers selected").arg(selectedCount);
        auto* selectionLabel = new QLabel(selectionText, row);
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
        rowIt->selectionLabel->setText(
            selectedCount == 1
                ? QStringLiteral("1 layer selected")
                : QStringLiteral("%1 layers selected").arg(selectedCount));
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
