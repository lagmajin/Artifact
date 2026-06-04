module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QColor>
#include <QFont>
#include <QPalette>
module Artifact.Widgets.QuickToolBox;

import Artifact.Widgets.QuickToolBox;
import Artifact.Widgets.AnchorPointTool;
import Artifact.Widgets.Alignment;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(ArtifactQuickToolBox)

class ArtifactQuickToolBox::Impl {
public:
    QTabWidget* tabWidget = nullptr;
};

ArtifactQuickToolBox::ArtifactQuickToolBox(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Theme setup
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    setPalette(widgetPalette);

    // Tab widget
    impl_->tabWidget = new QTabWidget(this);

    auto* anchorTab = new ArtifactAnchorPointTool(impl_->tabWidget);
    impl_->tabWidget->addTab(anchorTab, QStringLiteral("Anchor"));

    auto* alignTab = new AlignmentWidget(impl_->tabWidget);
    impl_->tabWidget->addTab(alignTab, QStringLiteral("Align"));

    auto* transformTab = new QWidget(impl_->tabWidget);
    {
        auto* layout = new QVBoxLayout(transformTab);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->addWidget(new QLabel(QStringLiteral("Transform helpers are routed through the composition editor."), transformTab));
        layout->addStretch(1);
    }
    impl_->tabWidget->addTab(transformTab, QStringLiteral("Transform"));

    auto* calcTab = new QWidget(impl_->tabWidget);
    {
        auto* layout = new QVBoxLayout(calcTab);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->addWidget(new QLabel(QStringLiteral("Calculator utilities are reserved for a later pass."), calcTab));
        layout->addStretch(1);
    }
    impl_->tabWidget->addTab(calcTab, QStringLiteral("Calc"));

    root->addWidget(impl_->tabWidget);
}

ArtifactQuickToolBox::~ArtifactQuickToolBox()
{
    delete impl_;
}

} // namespace Artifact
