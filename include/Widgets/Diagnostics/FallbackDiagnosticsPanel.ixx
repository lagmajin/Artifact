module;
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QDateTime>
#include <wobjectdefs.h>

export module Artifact.Widgets.Diagnostics.FallbackPanel;

import Core.Diagnostics.FallbackPolicy;

export class FallbackDiagnosticsPanel : public QWidget {
    W_OBJECT(FallbackDiagnosticsPanel)
public:
    explicit FallbackDiagnosticsPanel(QWidget* parent = nullptr);

    void refresh();

public:
    void cleared()
    W_SIGNAL(cleared)

private:
    void setupUi();
    void populateTree();
    QString categoryText(ArtifactCore::FallbackCategory cat) const;
    QString actionText(ArtifactCore::FallbackAction action) const;

    QTreeWidget* tree_;
    QComboBox* filterCombo_;
    QLabel* summaryLabel_;
    QPushButton* clearButton_;
    QPushButton* refreshButton_;
};
