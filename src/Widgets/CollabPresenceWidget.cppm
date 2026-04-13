module;
#include <utility>
#include <QHash>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <wobjectimpl.h>

module Artifact.Widgets.CollabPresenceWidget;

import std;

namespace Artifact {

class CollabPresenceWidget::Impl {
public:
    struct UserPresence {
        QString userId;
        QString userName;
        QColor color;
        QString cursorLocation;
        QStringList selectedLayers;
    };

    QHash<QString, UserPresence> users_;
    UserPresence localUser_;
};

CollabPresenceWidget::CollabPresenceWidget(QWidget* parent)
    : QFrame(parent)
    , impl_(new Impl())
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setMinimumWidth(150);
    setMaximumWidth(220);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QLabel* titleLabel = new QLabel(QStringLiteral("Connected Users"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() - 1);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    setLayout(layout);
}

CollabPresenceWidget::~CollabPresenceWidget() {
    delete impl_;
}

void CollabPresenceWidget::addUser(const QString& userId, const QString& userName, const QColor& color) {
    Impl::UserPresence presence;
    presence.userId = userId;
    presence.userName = userName;
    presence.color = color;
    impl_->users_[userId] = presence;

    // ユーザーラベルを追加
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        // 既存のラベルを削除
        for (int i = layout->count() - 1; i >= 1; --i) {
            QLayoutItem* item = layout->itemAt(i);
            if (QLabel* label = qobject_cast<QLabel*>(item->widget())) {
                if (label->property("userId").toString() == userId) {
                    layout->removeItem(item);
                    delete label;
                }
            }
        }

        // 新規ラベル追加
        QLabel* userLabel = new QLabel(this);
        userLabel->setProperty("userId", userId);
        userLabel->setText(QStringLiteral("● %1").arg(userName));

        QPalette pal = userLabel->palette();
        pal.setColor(QPalette::WindowText, color);
        userLabel->setPalette(pal);

        QFont font = userLabel->font();
        font.setPointSize(font.pointSize() - 1);
        userLabel->setFont(font);

        layout->addWidget(userLabel);
    }

    update();
}

void CollabPresenceWidget::updateUser(const QString& userId, const QJsonObject& presence) {
    auto it = impl_->users_.find(userId);
    if (it != impl_->users_.end()) {
        if (presence.contains(QStringLiteral("cursorLocation"))) {
            it->cursorLocation = presence.value(QStringLiteral("cursorLocation")).toString();
        }
        if (presence.contains(QStringLiteral("selectedLayers"))) {
            QJsonArray layers = presence.value(QStringLiteral("selectedLayers")).toArray();
            it->selectedLayers.clear();
            for (const auto& layer : layers) {
                it->selectedLayers.append(layer.toString());
            }
        }
        update();
    }
}

void CollabPresenceWidget::removeUser(const QString& userId) {
    impl_->users_.remove(userId);

    // ラベルを削除
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        for (int i = layout->count() - 1; i >= 1; --i) {
            QLayoutItem* item = layout->itemAt(i);
            if (QLabel* label = qobject_cast<QLabel*>(item->widget())) {
                if (label->property("userId").toString() == userId) {
                    layout->removeItem(item);
                    delete label;
                    break;
                }
            }
        }
    }

    update();
}

void CollabPresenceWidget::clearUsers() {
    impl_->users_.clear();

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (layout) {
        for (int i = layout->count() - 1; i >= 1; --i) {
            QLayoutItem* item = layout->itemAt(i);
            if (item->widget()) {
                layout->removeItem(item);
                delete item->widget();
            }
        }
    }

    update();
}

void CollabPresenceWidget::setLocalUser(const QString& userId, const QString& userName, const QColor& color) {
    impl_->localUser_.userId = userId;
    impl_->localUser_.userName = userName;
    impl_->localUser_.color = color;
}

QList<CollabPresenceWidget::UserPresence> CollabPresenceWidget::users() const {
    QList<UserPresence> list;
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
