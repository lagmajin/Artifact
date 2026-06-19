module;
#include <QString>

export module Artifact.Widgets.ProjectHealthSummary;

export namespace Artifact {

auto projectHealthSummaryText(int total, int errors, int warnings, int infos, bool hasProject) -> QString
{
    const QString warningText =
        !hasProject ? QStringLiteral("none")
                    : (errors > 0 ? QStringLiteral("%1 critical").arg(errors)
                                  : (warnings > 0 ? QStringLiteral("%1 warnings").arg(warnings)
                                                  : QStringLiteral("none")));
    const QString nextText =
        !hasProject ? QStringLiteral("open project")
                    : (errors > 0 ? QStringLiteral("open items")
                                  : (warnings > 0 ? QStringLiteral("review items")
                                                  : QStringLiteral("continue editing")));
    const QString nowText =
        !hasProject ? QStringLiteral("none")
                    : QStringLiteral("%1 items e%2 w%3 i%4")
                          .arg(total)
                          .arg(errors)
                          .arg(warnings)
                          .arg(infos);
    return QStringLiteral("goal: keep the project healthy | now: %1 | warning: %2 | next: %3")
        .arg(nowText)
        .arg(warningText)
        .arg(nextText);
}

}
