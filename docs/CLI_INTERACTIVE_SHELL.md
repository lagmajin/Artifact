# Artifact CLI interactive shell

**最終更新:** 2026-09-23

## Artifact をスクリプトからツールとして使う

AI が生成したバッチや人が書いた自動化スクリプトから、Artifact を
プロジェクト検査・状態取得・編集の実行ツールとして呼び出せる。Python の
WorkspaceAutomation 呼出しは結果を dict / list として返すため、スクリプト側で
成功・失敗を判定して後続処理を選べる。CLI の終了コードはプロセス全体の成否を
表し、個々のアプリ操作結果は戻り値で確認する。
WorkspaceAutomation へ渡す引数は JSON 値として転送され、Python の真偽値、数値、
list、dict の型を保つ。JSON にできない値と NaN / Infinity は引数として拒否される。

```python
# tools/check_project.py
import artifact

compositions = artifact.core.composition.get_composition_list()
if not compositions:
    raise RuntimeError("Project has no compositions")

result = artifact.core.composition.set_work_area(0, 120)
if result is False or (
    isinstance(result, dict)
    and (result.get("success") is False or result.get("ok") is False
         or result.get("value") is False)
):
    raise RuntimeError(result.get("message", "Could not set work area"))

print({"compositionCount": len(compositions), "workArea": result})
```

```powershell
$output = & .\Artifact.exe python run .\tools\check_project.py `
    --project .\project.artifact --json
$exitCode = $LASTEXITCODE
$result = $output | ConvertFrom-Json
if ($exitCode -ne 0 -or -not $result.ok) {
    throw $result.error.message
}
$result.result.stdout
```

アプリ API の個別結果も検査し、操作が返す `success` / `ok` を確認してから
後続作業へ進める。例外で終了したスクリプトは非 0 の終了コードになる。
Python CLI は信頼されたローカルスクリプトを実行する機能であり、sandbox ではない。

`artifact.core.automation` から Command IR の catalog、検証、実行も利用できる。
たとえば layer ID と property path を snapshot から特定したうえで、実行前に
検証し、validation と execution の両方の結果を確認する。

```python
import artifact

commands = artifact.core.automation.command_vocabulary()
if "set_property" not in {entry.get("type") for entry in commands}:
    raise RuntimeError("This Artifact build does not support set_property")
request = {
    "type": "set_property",
    "target": {"layerId": "layer-id-from-snapshot", "propertyPath": "opacity"},
    "value": 0.5,
}
validation = artifact.core.automation.validate_command(request)
if not validation.get("ok"):
    raise RuntimeError(validation.get("error", "Command is invalid"))

result = artifact.core.automation.execute_command(request)
if not result.get("ok"):
    raise RuntimeError(result.get("error", "Command execution failed"))
```

Artifact can run a project without opening the GUI and expose the same command
surface interactively or from a command file.

```text
Artifact.exe --interactive project.artifact
Artifact.exe --script build_preview.artifact-cli
Artifact.exe --command "project.validate" --json project.artifact
Artifact.exe --request request.json project.artifact
Artifact.exe command-ir command.json --project project.artifact
Artifact.exe command-ir - --project project.artifact
Artifact.exe python run tools\batch.py --project project.artifact --json
Artifact.exe python eval "artifact.project_info()" --project project.artifact --json
Artifact.exe python repl --project project.artifact
Artifact.exe python repl --jsonl --project project.artifact
```

The top-level render namespace is reserved for the headless executor:

```text
Artifact.exe render project.artifact --output preview.mp4 --start 0 --end 120
```

Its arguments are parsed and validated independently from GUI launch options.

## Typical session

```text
project.stats
composition.list
composition.select MainComp
layer.list
layer.select Background
property.get opacity
property.get opacity --json
property.set opacity 0.5 --json
undo
redo
project.validate
project.save
project.open OtherProject.artifact
ls
select Main
get opacity --json
render.plan --json
render.list --json
render.enqueue render-job.json --start 0 --end 120 --output preview.mp4 --format mp4
render.status render-job.json --json
render.start render-job.json
render.finish render-job.json
quit
```

`source path.cli` executes another command file. Lines beginning with `#` are
ignored, and recursive `source` loops are rejected. `complete prefix` lists
registered commands for future tab-completion clients.

`--command` runs one command and exits without opening the interactive prompt.
For PowerShell, quote the complete command as one argument:

```powershell
$output = & .\Artifact.exe --command 'project.validate' --json .\project.artifact
$exitCode = $LASTEXITCODE
$result = $output | ConvertFrom-Json
if (-not $result.ok) { throw $result.error.message }
```

Top-level `--json` wraps the command payload in a common envelope with
`schemaVersion`, `ok`, `command`, `result` or `error`, and `warnings`. It is
supported with `--command` and `--request`; command-specific `--json` remains
available in interactive and script sessions.

An agent can discover registered command names, usage, short descriptions, and
declared effects through the same contract:

```powershell
$catalog = & .\Artifact.exe --command 'help --json' --json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or -not $catalog.ok) { throw $catalog.error.message }
$catalog.result.commands
```

Catalog `effect` values are `read`, `session`, `write-project`,
`write-manifest`, `batch`, `interactive-only`, or `unknown`. `batch` can invoke
other commands and must be treated according to those commands' effects.

`--request` reads a UTF-8 JSON file and avoids the extra PowerShell command
string quoting layer. Version 1 accepts one command in the interactive shell's
command syntax and an optional correlation id:

```json
{
  "schemaVersion": 1,
  "requestId": "job-042",
  "command": "project.validate --json"
}
```

Pass the project path as a normal CLI argument. The response includes the same
`requestId`, the command result, and the normal exit code:

```powershell
$result = & .\Artifact.exe --request .\request.json .\project.artifact | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or -not $result.ok) { throw $result.error.message }
```

The request file is limited to 1 MiB. Unsupported schema versions, malformed
JSON, missing commands, and unreadable files return a structured error; input
errors use exit code `2`.

For typed app operations, `command-ir <file>` accepts a structured Command IR
request without passing it through shell command text. Use `operation: "catalog"`
to discover supported command types, `"validate"` to check a command without
loading a project, or `"execute"` with `--project <file>` to load the project
and call the existing WorkspaceAutomation executor. Validation and execution
return CommandResult fields in `result`; catalog returns the command array.
Top-level `ok` and the process exit code reflect the operation.

```json
{ "schemaVersion": 1, "requestId": "discover-01", "operation": "catalog" }
```

```powershell
$catalog = & .\Artifact.exe command-ir .\catalog.json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or -not $catalog.ok) { throw $catalog.error.message }
$catalog.result
```

```json
{
  "schemaVersion": 1,
  "requestId": "set-opacity-01",
  "operation": "execute",
  "saveProject": true,
  "command": {
    "type": "set_property",
    "target": { "layerId": "layer-id-from-snapshot", "propertyPath": "opacity" },
    "value": 0.5
  }
}
```

```powershell
$result = & .\Artifact.exe command-ir .\command.json `
    --project .\project.artifact | ConvertFrom-Json
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0 -or -not $result.ok) { throw $result.error.message }
$result.result
```

Pass `command-ir -` to read a stream of UTF-8 JSON Lines requests. Artifact
keeps one process and project session alive, writes one response per input line,
and continues after individual request failures. Any failed request makes the
final process exit code non-zero. Each line has the same schema as a request
file, with its own optional `requestId`.

```powershell
$responses = Get-Content -LiteralPath .\commands.jsonl -Encoding utf8 |
    & .\Artifact.exe command-ir - --project .\project.artifact |
    ForEach-Object { $_ | ConvertFrom-Json }
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { throw 'One or more Command IR requests failed' }
$responses
```

Requests are UTF-8 JSON and limited to 1 MiB each. Catalog and validation do not
need a project; execute requires exactly one valid Artifact project path.
`saveProject` is an optional boolean for execute requests. Set it to `true` to
persist a successful project edit through Artifact's project exporter; the
response reports `result.projectSaved`, and a save failure makes top-level
`ok` false. If omitted or false, changes exist only in the short-lived process
and are discarded when it exits.
Malformed requests and missing project inputs return exit code `2`; invalid
commands and execution failures return non-zero with structured error details.

Use `--request -` to keep one Artifact process alive for multiple command
requests. It reads one UTF-8 JSON object per stdin line using the same version 1
schema, and writes one response envelope per line. Selection state and shell
undo history are shared across requests. A request may use `quit` to end the
stream; any failed request makes the process exit with code `1` after the
stream closes. Nested `project.open` is unavailable in machine request mode.

```powershell
$responses = Get-Content -LiteralPath .\requests.jsonl -Encoding utf8 |
    & .\Artifact.exe --request - .\project.artifact |
    ForEach-Object { $_ | ConvertFrom-Json }
if ($LASTEXITCODE -ne 0) { throw 'One or more Artifact requests failed' }
$responses
```

Python supports `run <file>`, `eval <expression>`, and a persistent `repl`.
Pass `--project <file>` to load a project before executing the script. Use
top-level `--json` with `run` or `eval` to receive stdout, stderr, and the
expression value in the same result envelope. The REPL is a human terminal
session and does not accept top-level `--json`. When only an external Python
fallback is available, scripts can run as Python, but the in-process Artifact
API is unavailable; this condition is reported in the JSON `warnings` array.

Use `python repl --jsonl` for a persistent Python session over redirected
stdin/stdout. Send one UTF-8 JSON object per input line with a string `code`
and optional `requestId`; each input line returns one JSON object. The response
status is `executed`, `needs_more_input`, or `error`. Variables and imported
modules stay alive for the session. An incomplete block is submitted by
sending its indented lines and then an empty `code` line. The embedded Python
runtime is required for this persistent mode.

```text
{"requestId":"set","code":"count = 40"}
{"requestId":"read","code":"count + 2"}
```

The second response carries `42` in `result.stdout` (Python's interactive
display output). The process exits with code `1` if any request fails, while
continuing to accept later input lines. Closing stdin while a block is
incomplete emits a `session_end` event and exits non-zero.

`--script` and `--command` return exit code `1` when a command is unknown or a command reports
an error to standard error, and `2` when the script file cannot be opened or
the command-line argument is incomplete. `project.validate --json` writes a
compact JSON result to standard output, so PowerShell can parse it while
checking `$LASTEXITCODE`:

```powershell
[IO.File]::WriteAllText(
    '.\validate.artifact-cli',
    "project.validate --json`n",
    [Text.UTF8Encoding]::new($false))
$output = & .\Artifact.exe --script .\validate.artifact-cli .\project.artifact
$exitCode = $LASTEXITCODE
$result = $output | ConvertFrom-Json
```

Exit codes: `0` means all script commands completed, `1` means an unknown
command or command error, and `2` means the script could not be opened or CLI
arguments were incomplete. Python runtime unavailability returns `3`; Python
execution errors return `1`. Command diagnostics are written to standard error.

`render.plan` reports frame ranges, resolution, and frame rate. Add `--json`
for machine-readable output. `render.enqueue` writes an `artifact.render` job
manifest that can be consumed by a render worker or the render queue adapter.
Supported render formats are `png`, `jpg`, `jpeg`, `mp4`, `mov`, and `webm`.

`project.open` starts a nested session for another project; exiting that
session returns to the parent shell.
