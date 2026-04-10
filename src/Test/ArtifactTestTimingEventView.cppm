module;

#include <QDebug>

export module Artifact.Test.TimingEventView;

import Artifact.Widgets.TimingEventView;

namespace Artifact {

export class ArtifactTestTimingEventView {
public:
    void runAllTests();
};

namespace {
struct TimingEventViewTestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[TimingEventView Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[TimingEventView Test][OK]" << label;
        }
    }
};
} // namespace

void ArtifactTestTimingEventView::runAllTests()
{
    TimingEventViewTestReport report;

    TimingEventView view;

    QVector<TimingEventItem> items;
    items.push_back(TimingEventItem{QStringLiteral("cue-1"), QStringLiteral("Intro Cue"), 0, 12, 42, false});
    items.push_back(TimingEventItem{QStringLiteral("cue-2"), QStringLiteral("Beat Drop"), 1, 48, 72, false});

    view.setEvents(items);
    report.check(view.events().size() == 2, QStringLiteral("events roundtrip"));

    view.setSelectedEventId(QStringLiteral("cue-2"));
    report.check(view.selectedEventId() == QStringLiteral("cue-2"), QStringLiteral("selection updates"));

    view.setCurrentFrame(64);
    report.check(view.currentFrame() == 64, QStringLiteral("current frame updates"));

    view.setVisibleStartFrame(8);
    view.setVisibleEndFrame(128);
    report.check(view.visibleStartFrame() == 8, QStringLiteral("visible start updates"));
    report.check(view.visibleEndFrame() >= 9, QStringLiteral("visible end stays ahead of start"));

    view.setPixelsPerFrame(9.0);
    report.check(view.pixelsPerFrame() > 0.0, QStringLiteral("zoom updates"));

    view.clearEvents();
    report.check(view.events().isEmpty(), QStringLiteral("clear events works"));

    qInfo().noquote() << "[TimingEventView Test] failures:" << report.failures;
}

} // namespace Artifact
