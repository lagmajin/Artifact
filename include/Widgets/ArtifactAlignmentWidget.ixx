module;
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <wobjectdefs.h>

export module Artifact.Widgets.Alignment;

namespace Artifact {

export class AlignmentWidget : public QWidget {
    W_OBJECT(AlignmentWidget)
public:
    explicit AlignmentWidget(QWidget* parent = nullptr);
    virtual ~AlignmentWidget();

private:
    void setupUi();
    void onAlignClicked(int type); // ArtifactCore::AlignType
    void onDistributeClicked(int type); // ArtifactCore::DistributeType

    class Impl;
    Impl* impl_;
};

} // namespace Artifact
