module;
#include <utility>
#include <wobjectimpl.h>
#include <QWidget>
#include <QString>
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

        QSize sizeHint() const override;
    };
}
