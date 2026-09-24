module;
#include <utility>
#include <QHash>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Widgets.CollabPresenceWidget;

export namespace Artifact {

/**
 * @brief コラボレーションプレゼンス表示ウィジェット
 *
 * 現在プロジェクトに接続中の他ユーザーの一覧と、
 * 各ユーザーの位置・選択状態を表示する。
 */
class CollabPresenceWidget : public QFrame {
    W_OBJECT(CollabPresenceWidget)
public:
    explicit CollabPresenceWidget(QWidget* parent = nullptr);
    ~CollabPresenceWidget();

    // ユーザー追加・更新・削除
    void addUser(const QString& userId, const QString& userName, const QColor& color);
    void updateUser(const QString& userId, const QJsonObject& presence);
    void removeUser(const QString& userId);
    void clearUsers();

    // ローカルユーザーの設定
    void setLocalUser(const QString& userId, const QString& userName, const QColor& color);

    // ユーザー情報取得
    struct UserPresence {
        QString userId;
        QString userName;
        QColor color;
        QString cursorLocation;  // "timeline:150", "inspector:transform"
        QStringList selectedLayers;
        QStringList selectedLayerNames;
    };

    struct ReviewNote {
        QString commentId;
        QString parentCommentId;
        QString authorName;
        QString anchor;
        QString text;
        QStringList revisionHistory;
        bool resolved = false;
        bool deleted = false;
        bool revisionHistoryTruncated = false;
    };

    void setConnectionActionHandler(
        std::function<void(const QString& serverUrl, const QString& projectId,
                           const QString& userName,
                           const QString& accessToken)> handler);
    void setLayerLockActionHandler(
        std::function<void(const QString& layerId, bool acquire)> handler);
    void setConnectionStatus(const QString& statusText, bool active);
    void setLayerLockControlState(const QString& selectedLayerId,
                                  bool connected, bool locked, bool heldByLocalUser,
                                  bool pending, const QString& ownerName,
                                  const QString& reason = {});
    void setLayerEditBlockedStatus(const QString& reason);
    void setReviewCommentHandler(
        std::function<bool(const QString& text)> handler);
    void setReviewReplyHandler(
        std::function<bool(const QString& commentId, const QString& text)> handler);
    void setReviewResolveHandler(
        std::function<bool(const QString& commentId, bool resolved)> handler);
    void setReviewEditHandler(
        std::function<bool(const QString& commentId, const QString& text)> handler);
    void setReviewDeleteHandler(
        std::function<bool(const QString& commentId)> handler);
    void setReviewJumpHandler(
        std::function<bool(const QString& commentId)> handler);
    void setReviewContext(const QString& compositionId,
                          const QString& layerId, bool connected,
                          bool canEdit);
    void setReviewNotes(const QList<ReviewNote>& notes);
    void syncRemoteUsers(const QList<UserPresence>& users);
    QList<UserPresence> users() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void refreshPresenceRows();
    void updatePresenceRow(const UserPresence& presence);

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
