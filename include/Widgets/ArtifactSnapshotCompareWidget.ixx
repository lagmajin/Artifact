module;
#include <vector>
#include <string>
#include <memory>
#include <QWidget>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <wobjectdefs.h>
export module Artifact.Widgets.SnapshotCompareWidget;

export namespace Artifact {

class ArtifactSnapshotCompareWidget : public QWidget {
    W_OBJECT(ArtifactSnapshotCompareWidget)
    
public:
    explicit ArtifactSnapshotCompareWidget(QWidget* parent = nullptr);
    ~ArtifactSnapshotCompareWidget() override;

    void loadSnapshots(const std::vector<QString>& snapshotNames);
    void setSnapshotA(const QString& name);
    void setSnapshotB(const QString& name);

private slots:
    void onCompare();
    void onRestoreA();
    void onRestoreB();
    void onBranch();
    void onDiff();

private:
    class Impl;
    Impl* impl_;
};

}
