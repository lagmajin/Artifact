module;
#include <utility>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QColor>
#include <QString>
#include <QWidget>
#include <wobjectimpl.h>

export module Artifact.Widgets.LayerLockIndicator;

export namespace Artifact {

/**
 * @brief レイヤーロックインジケーター
 *
 * タイムラインのレイヤー行の端に、ロック状態をアイコンで表示する。
 * ロック中は編集不可を視覚的に示す。
 */
class LayerLockIndicator : public QWidget {
    W_OBJECT(LayerLockIndicator)
public:
    explicit LayerLockIndicator(QWidget* parent = nullptr);
    ~LayerLockIndicator();

    // ロック状態の設定
    void setLocked(bool locked, const QString& userName = QString(), const QString& userColor = QString());
    bool isLocked() const;
    QString lockingUserName() const;
    QString lockingUserColor() const;

    // サイズヒント
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
