module;
#include <utility>
#include <wobjectimpl.h>
#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QSize>
#include <QStringList>
#include <QVariant>
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
        void setPreviewContext(const QString& compositionName,
                               const QSize& compositionSize,
                               const QStringList& layerNames,
                               int currentLayerIndex,
                               const QString& layerName,
                               const QVariant& propertyValue,
                               double timeSeconds);
        void clearPreviewContext();

        QSize sizeHint() const override;
        bool eventFilter(QObject* watched, QEvent* event) override;
    };
}
