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
    // Event/Signal to request paste of stored item
    void clipPasteRequested(const QVariant &data) W_SIGNAL(clipPasteRequested, data);

private:
    class Impl;
    Impl *impl_;
};

} // namespace Artifact
