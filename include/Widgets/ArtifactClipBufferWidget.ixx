module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.ClipBufferWidget;

import Artifact.Widgets.ClipBufferModel;

export namespace Artifact {

class ArtifactClipBufferWidget : public QWidget {
    W_OBJECT(ArtifactClipBufferWidget)
public:
    explicit ArtifactClipBufferWidget(QWidget *parent = nullptr);
    ~ArtifactClipBufferWidget();

public:
    // Cross-widget paste is published as ClipPasteRequestedEvent.
    void clipPasteRequested(const QVariant &data);

private:
    class Impl;
    Impl *impl_;
};

} // namespace Artifact
