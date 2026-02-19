module;
#include <QWidget>
#include <wobjectdefs.h>
export module Widgets.PowerShellWidget;

import std;
import Utils.String.UniString;

export namespace Artifact {

class PowerShellWidget : public QWidget {
 W_OBJECT(PowerShellWidget)
private:
    class Impl;
    Impl* impl_;
public:
    PowerShellWidget(QWidget* parent = nullptr);
    ~PowerShellWidget();

    ArtifactCore::UniString runCommand(const ArtifactCore::UniString& command);
    void runCommandAsync(const ArtifactCore::UniString& command);
    void appendLog(const ArtifactCore::UniString& text);

    void commandFinished(const ArtifactCore::UniString& output) W_SIGNAL(commandFinished, output);
};

} // namespace Artifact

