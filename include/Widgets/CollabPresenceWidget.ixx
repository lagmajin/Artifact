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
    };
    QList<UserPresence> users() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
