module;

#include <QStringList>

export module Artifact.Application.InteractiveShell;

export namespace Artifact {

struct InteractiveShellResult {
  int exitCode = 0;
  bool quitRequested = false;
};

[[nodiscard]] InteractiveShellResult runInteractiveShell(
    const QStringList& projectPaths,
    const QString& scriptPath = {},
    const QString& singleCommand = {},
    bool jsonOutput = false,
    const QString& requestPath = {});

// Windows CLI モードで親 console に接続したプロセスは、
// Ctrl+C / Ctrl+Break をデフォルトでは終了させない。
// ハンドラがこの状態を立て、CLI の読み取りループが要求の
// 合間に参照して中断を 130 終了コードへ変換する。
// ブロック中の読み取り自体は割り込まない（制約は
// docs/planned/MILESTONE_CLI_PYTHON_AUTOMATION_2026-09-23.md
// の未完了項目を参照）。
bool consoleInterruptRequested();
void noteConsoleInterrupt();

} // namespace Artifact
