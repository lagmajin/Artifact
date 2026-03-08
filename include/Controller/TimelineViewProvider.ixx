module;
#include <QObject>
#include <wobjectdefs.h>

export module Artifact.Controller.TimelineViewProvider;

import Utils.Id;
import Artifact.Widgets.Timeline;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactTimelineWidget;

/**
 * @brief Manage and provide timeline widgets for compositions.
 */
class TimelineViewProvider : public QObject {
    W_OBJECT(TimelineViewProvider)
public:
    explicit TimelineViewProvider(QObject* parent = nullptr);
    virtual ~TimelineViewProvider();

    ArtifactTimelineWidget* timelineWidgetForComposition(const ArtifactCore::CompositionID& id, QWidget* parent = nullptr);

private:
    class Impl;
    Impl* m_impl;
};

}