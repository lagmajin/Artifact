module;
#include <utility>
#include <wobjectimpl.h>
#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QString>
#include <functional>
export module Artifact.Widgets.ExpressionCopilotWidget;

export namespace Artifact {
    class ArtifactExpressionCopilotWidget final : public QWidget {
        W_OBJECT(ArtifactExpressionCopilotWidget)
    private:
        class Impl;
        Impl* impl_;
    public:
        explicit ArtifactExpressionCopilotWidget(QWidget* parent = nullptr);
        ~ArtifactExpressionCopilotWidget() override;

        void setExpressionText(const QString& expression);
        QString expressionText() const;
        void setApplyHandler(std::function<void(const QString& expression)> handler);

        QSize sizeHint() const override;
        bool eventFilter(QObject* watched, QEvent* event) override;
    };
}
