module;

#include <QDialog>
#include <QWidget>
#include <QString>
#include <QVector>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Widgets.Timeline.EasingLab;

import Math.Interpolate;
import Animation.EasingCurveUtil;

export namespace Artifact {

class EasingPreviewWidget : public QWidget {
    W_OBJECT(EasingPreviewWidget)
public:
    explicit EasingPreviewWidget(QWidget* parent = nullptr);
    ~EasingPreviewWidget() override;

    void setCandidate(const ArtifactCore::EasingCandidate& candidate);
    void setPreviewProgress(float progress);
    ArtifactCore::EasingCandidate candidate() const;
    float previewProgress() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

class EasingLabDialog : public QDialog {
    W_OBJECT(EasingLabDialog)
public:
    explicit EasingLabDialog(QWidget* parent = nullptr,
                             std::function<void(ArtifactCore::InterpolationType)> applyCallback = {});
    ~EasingLabDialog() override;

    void setCandidates(const QVector<ArtifactCore::EasingCandidate>& candidates);
    QVector<ArtifactCore::EasingCandidate> candidates() const;
    void setPreviewProgress(float progress);
    float previewProgress() const;

private:
    class Impl;
    Impl* impl_ = nullptr;
    void clearPreviewGrid();
    void rebuildPreviewGrid();
};

} // namespace Artifact
