module;
#include <QWidget>
#include <wobjectdefs.h>
export module Widgets.PowerShellWidget;

import std;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;

class PowerShellWidget : public QWidget {
 W_OBJECT(PowerShellWidget)
private:
    class Impl;
    Impl* impl_;
public:
    PowerShellWidget(QWidget* parent = nullptr);
    ~PowerShellWidget();

    // Run a PowerShell command and return output (synchronous helper)
    UniString runCommand(const UniString& command);

    // Run asynchronously and emit signal when done
    void runCommandAsync(const UniString& command);

    void appendLog(const UniString& text);

    // Signals
    void commandFinished(const UniString& output) W_SIGNAL(commandFinished, output);
};

