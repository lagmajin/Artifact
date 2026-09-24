module;

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <utility>

module Artifact.Application.CommandLine;

namespace Artifact {
namespace {

bool isProjectPath(const QString& path)
{
  const QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    return false;
  }

  const QString fileName = info.fileName().toLower();
  return fileName.endsWith(QStringLiteral(".artifact")) ||
         fileName.endsWith(QStringLiteral(".artifact.json"));
}

bool hasProjectExtension(const QString& path)
{
  const QString lower = QFileInfo(path).fileName().toLower();
  return lower.endsWith(QStringLiteral(".artifact")) ||
         lower.endsWith(QStringLiteral(".artifact.json"));
}

int commandPriority(CommandType type)
{
  switch (type) {
    case CommandType::Help: return 7;
    case CommandType::Version: return 6;
    case CommandType::McpServer: return 5;
    case CommandType::PluginList: return 4;
    case CommandType::PluginInfo: return 3;
    case CommandType::Render: return 2;
    case CommandType::CommandIR: return 3;
    case CommandType::Python: return 2;
    case CommandType::Interactive: return 1;
    case CommandType::Gui: return 0;
    default: return 0;
  }
}

void selectCommand(CommandLine& commandLine, CommandType candidate)
{
  if (commandPriority(candidate) > commandPriority(commandLine.type)) {
    commandLine.type = candidate;
  }
}

} // namespace

CommandLine parseCommandLine(const QStringList& arguments)
{
  CommandLine result;
  bool renderSubcommand = false;
  bool pythonSubcommand = false;

  for (int i = 1; i < arguments.size(); ++i) {
    const QString& argument = arguments[i];
    if (i == 1 && argument == QStringLiteral("render")) {
      renderSubcommand = true;
      selectCommand(result, CommandType::Render);
    } else if (i == 1 && argument == QStringLiteral("python")) {
      pythonSubcommand = true;
      selectCommand(result, CommandType::Python);
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.python.action = arguments[++i].toLower();
        if ((result.python.action == QStringLiteral("run") ||
             result.python.action == QStringLiteral("eval")) &&
            i + 1 < arguments.size() &&
            !arguments[i + 1].startsWith(QStringLiteral("--"))) {
          result.python.target = arguments[++i];
        }
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (i == 1 && argument == QStringLiteral("command-ir")) {
      selectCommand(result, CommandType::CommandIR);
      result.commandIR.requested = true;
      if (i + 1 < arguments.size() &&
          (arguments[i + 1] == QStringLiteral("-") ||
           !arguments[i + 1].startsWith(QLatin1Char('-')))) {
        result.commandIR.requestPath = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (renderSubcommand && argument == QStringLiteral("--output")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.render.outputPath = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (renderSubcommand && argument == QStringLiteral("--start")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        bool ok = false;
        result.render.startFrame = arguments[++i].toInt(&ok);
        if (!ok) result.invalidOptionValues.append(argument);
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (renderSubcommand && argument == QStringLiteral("--end")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        bool ok = false;
        result.render.endFrame = arguments[++i].toInt(&ok);
        if (!ok) result.invalidOptionValues.append(argument);
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (renderSubcommand && argument == QStringLiteral("--format")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.render.format = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (renderSubcommand && !argument.startsWith(QLatin1Char('-')) &&
               result.render.inputPath.isEmpty()) {
      result.render.inputPath = argument;
    } else if (pythonSubcommand && !argument.startsWith(QLatin1Char('-')) &&
               result.python.action == QStringLiteral("run") &&
               result.python.target.isEmpty()) {
      result.python.target = argument;
    } else if (argument == QStringLiteral("--help") || argument == QStringLiteral("-h")) {
      selectCommand(result, CommandType::Help);
    } else if (argument == QStringLiteral("--version")) {
      selectCommand(result, CommandType::Version);
    } else if (argument == QStringLiteral("--mcp-server") ||
               argument == QStringLiteral("--mcp-debug")) {
      selectCommand(result, CommandType::McpServer);
    } else if (argument == QStringLiteral("--mcp-port")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        bool ok = false;
        const int port = arguments[++i].toInt(&ok);
        if (!ok || port <= 0 || port > 65535) result.invalidOptionValues.append(argument);
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--interactive") ||
               argument == QStringLiteral("-i")) {
      selectCommand(result, CommandType::Interactive);
    } else if (argument == QStringLiteral("--command")) {
      selectCommand(result, CommandType::Interactive);
      result.gui.commandRequested = true;
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.gui.singleCommand = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--request")) {
      selectCommand(result, CommandType::Interactive);
      result.gui.requestRequested = true;
      result.gui.jsonOutput = true;
      if (i + 1 < arguments.size() &&
          (arguments[i + 1] == QStringLiteral("-") ||
           !arguments[i + 1].startsWith(QLatin1Char('-')))) {
        result.gui.requestPath = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--json")) {
      result.gui.jsonOutput = true;
    } else if (argument == QStringLiteral("--jsonl")) {
      result.python.jsonLines = true;
    } else if (argument == QStringLiteral("--script")) {
      selectCommand(result, CommandType::Interactive);
      result.gui.scriptRequested = true;
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.gui.scriptPath = arguments[i + 1];
        ++i;
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--plugin-list")) {
      selectCommand(result, CommandType::PluginList);
    } else if (argument == QStringLiteral("--plugin-info")) {
      selectCommand(result, CommandType::PluginInfo);
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.pluginInfo.pluginId = arguments[i + 1];
        ++i;
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--lang")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.global.languageCode = arguments[i + 1];
        ++i;
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--headless") ||
               argument == QStringLiteral("--export")) {
      // Legacy launch switches remain accepted while their dedicated command
      // payloads are migrated to RenderCommand.
    } else if (argument == QStringLiteral("--safe-mode")) {
      result.global.safeMode = true;
    } else if (argument == QStringLiteral("--verbose")) {
      result.global.verbose = true;
    } else if (argument == QStringLiteral("--no-splash")) {
      result.global.noSplash = true;
    } else if (argument == QStringLiteral("--log-file")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.global.logFile = arguments[++i];
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--threads")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        bool ok = false;
        result.global.renderThreads = arguments[++i].toInt(&ok);
        if (!ok) result.invalidOptionValues.append(argument);
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--project") ||
               argument == QStringLiteral("--open")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        const QString path = arguments[++i];
        if (result.commandIR.requested) {
          if (!result.commandIR.projectPath.isEmpty()) {
            result.invalidOptionValues.append(QStringLiteral("--project (only one project may be supplied)"));
          } else {
            result.commandIR.projectPath = path;
          }
        } else if (pythonSubcommand) {
          result.python.projectPath = path;
        } else {
          result.gui.projectPaths.append(path);
        }
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--renderer")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        result.global.rendererBackend = arguments[++i].toLower();
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument == QStringLiteral("--output") ||
               argument == QStringLiteral("--start-frame") ||
               argument == QStringLiteral("--end-frame") ||
               argument == QStringLiteral("--log-level")) {
      if (i + 1 < arguments.size() && !arguments[i + 1].startsWith(QLatin1Char('-'))) {
        ++i;
      } else {
        result.missingOptionValues.append(argument);
      }
    } else if (argument.startsWith(QLatin1Char('-'))) {
      result.unknownOptions.append(argument);
    } else if (!renderSubcommand && !argument.startsWith(QLatin1Char('-'))) {
      if (result.commandIR.requested) {
        if (!result.commandIR.projectPath.isEmpty()) {
          result.invalidOptionValues.append(QStringLiteral("command-ir (only one project may be supplied)"));
        } else {
          result.commandIR.projectPath = argument;
        }
      } else {
        result.gui.projectPaths.append(argument);
      }
    }
  }

  return result;
}

CommandLineResult validateCommandLine(CommandLine commandLine)
{
  if (!commandLine.unknownOptions.isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("Unknown option: %1").arg(commandLine.unknownOptions.constFirst())};
  }
  if (!commandLine.missingOptionValues.isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("Option requires a value: %1")
                .arg(commandLine.missingOptionValues.constFirst())};
  }
  if (!commandLine.invalidOptionValues.isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("Invalid option value: %1")
                .arg(commandLine.invalidOptionValues.constFirst())};
  }
  const QString backend = commandLine.global.rendererBackend.toLower();
  if (backend != QStringLiteral("auto") && backend != QStringLiteral("dx12") &&
      backend != QStringLiteral("d3d12") && backend != QStringLiteral("vulkan")) {
    return {std::move(commandLine),
            QStringLiteral("Unsupported renderer backend: %1").arg(commandLine.global.rendererBackend)};
  }
  commandLine.global.rendererBackend = backend == QStringLiteral("d3d12")
      ? QStringLiteral("dx12") : backend;
  if (!commandLine.global.languageCode.isEmpty()) {
    const QString language = commandLine.global.languageCode.trimmed().toLower();
    if (language == QStringLiteral("japanese")) {
      commandLine.global.languageCode = QStringLiteral("ja");
    } else if (language == QStringLiteral("english")) {
      commandLine.global.languageCode = QStringLiteral("en");
    } else if (language == QStringLiteral("chinese-simplified")) {
      commandLine.global.languageCode = QStringLiteral("zh");
    } else if (language == QStringLiteral("chinese-traditional")) {
      commandLine.global.languageCode = QStringLiteral("zh-tw");
    } else if (language == QStringLiteral("ja") || language == QStringLiteral("en") ||
               language == QStringLiteral("zh") || language == QStringLiteral("zh-tw")) {
      commandLine.global.languageCode = language;
    } else {
      return {std::move(commandLine),
              QStringLiteral("Unsupported language: %1").arg(language)};
    }
  }
  if (commandLine.global.renderThreads < 0) {
    return {std::move(commandLine), QStringLiteral("--threads must be zero or greater")};
  }
  if (commandLine.gui.scriptRequested && commandLine.gui.scriptPath.isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("--script requires a command file path")};
  }
  if (commandLine.gui.commandRequested && commandLine.gui.singleCommand.trimmed().isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("--command requires a command string")};
  }
  if (commandLine.gui.commandRequested && commandLine.gui.scriptRequested) {
    return {std::move(commandLine),
            QStringLiteral("--command and --script cannot be used together")};
  }
  if (commandLine.gui.requestRequested &&
      (commandLine.gui.commandRequested || commandLine.gui.scriptRequested)) {
    return {std::move(commandLine),
            QStringLiteral("--request cannot be combined with --command or --script")};
  }
  if (commandLine.gui.jsonOutput && !commandLine.gui.commandRequested &&
      !commandLine.gui.requestRequested) {
    if (commandLine.type != CommandType::CommandIR &&
        (commandLine.type != CommandType::Python ||
        (commandLine.python.action != QStringLiteral("run") &&
         commandLine.python.action != QStringLiteral("eval")))) {
      return {std::move(commandLine),
              QStringLiteral("--json requires --command or python run/eval")};
    }
  }
  if (commandLine.type == CommandType::Python &&
      commandLine.python.action != QStringLiteral("run") &&
      commandLine.python.action != QStringLiteral("eval") &&
      commandLine.python.action != QStringLiteral("repl")) {
    return {std::move(commandLine),
            QStringLiteral("python action must be run, eval, or repl")};
  }
  if (commandLine.type == CommandType::CommandIR &&
      commandLine.commandIR.requestPath.trimmed().isEmpty()) {
    return {std::move(commandLine), QStringLiteral("command-ir requires a request file")};
  }
  if (commandLine.commandIR.requested && commandLine.type != CommandType::CommandIR) {
    return {std::move(commandLine), QStringLiteral("command-ir cannot be combined with another mode")};
  }
  if (commandLine.type == CommandType::CommandIR) {
    if (commandLine.commandIR.requestPath != QStringLiteral("-")) {
      commandLine.commandIR.requestPath =
          QFileInfo(commandLine.commandIR.requestPath).absoluteFilePath();
    }
    if (!commandLine.commandIR.projectPath.isEmpty()) {
      commandLine.commandIR.projectPath =
          QFileInfo(commandLine.commandIR.projectPath).absoluteFilePath();
    }
  }
  if (commandLine.type == CommandType::Python &&
      commandLine.python.action != QStringLiteral("repl") &&
      commandLine.python.target.trimmed().isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("python %1 requires a target").arg(commandLine.python.action)};
  }
  if (commandLine.python.jsonLines &&
      (commandLine.type != CommandType::Python ||
       commandLine.python.action != QStringLiteral("repl"))) {
    return {std::move(commandLine), QStringLiteral("--jsonl requires python repl")};
  }
  if (commandLine.type == CommandType::Python &&
      !commandLine.python.projectPath.isEmpty()) {
    const QString projectPath = QFileInfo(commandLine.python.projectPath).absoluteFilePath();
    if (!isProjectPath(projectPath)) {
      return {std::move(commandLine),
              QStringLiteral("Invalid Python CLI project path: %1").arg(projectPath)};
    }
    commandLine.python.projectPath = projectPath;
  }
  if (commandLine.gui.commandRequested && commandLine.type != CommandType::Interactive) {
    return {std::move(commandLine), QStringLiteral("--command cannot be combined with another mode")};
  }
  if (commandLine.gui.scriptRequested && commandLine.type != CommandType::Interactive) {
    return {std::move(commandLine), QStringLiteral("--script cannot be combined with another mode")};
  }
  if (commandLine.gui.requestRequested && commandLine.type != CommandType::Interactive) {
    return {std::move(commandLine), QStringLiteral("--request cannot be combined with another mode")};
  }
  if (commandLine.type == CommandType::PluginInfo &&
      commandLine.pluginInfo.pluginId.isEmpty()) {
    return {std::move(commandLine),
            QStringLiteral("--plugin-info requires a plugin id")};
  }
  if (commandLine.type == CommandType::Render) {
    if (commandLine.render.inputPath.isEmpty()) {
      return {std::move(commandLine), QStringLiteral("render requires an input project path")};
    }
    if (commandLine.render.startFrame >= 0 && commandLine.render.endFrame >= 0 &&
        commandLine.render.startFrame > commandLine.render.endFrame) {
      return {std::move(commandLine), QStringLiteral("render start frame must not exceed end frame")};
    }
    if (!commandLine.render.format.isEmpty()) {
      const QString format = commandLine.render.format.toLower();
      if (format != QStringLiteral("png") && format != QStringLiteral("jpg") &&
          format != QStringLiteral("jpeg") && format != QStringLiteral("mp4") &&
          format != QStringLiteral("mov") && format != QStringLiteral("webm")) {
        return {std::move(commandLine),
                QStringLiteral("Unsupported render format: %1").arg(commandLine.render.format)};
      }
      commandLine.render.format = format;
    }
    commandLine.render.inputPath = QFileInfo(commandLine.render.inputPath).absoluteFilePath();
    if (!hasProjectExtension(commandLine.render.inputPath)) {
      return {std::move(commandLine), QStringLiteral("render input must be an Artifact project")};
    }
  }

  QStringList validatedPaths;
  validatedPaths.reserve(commandLine.gui.projectPaths.size());
  for (const QString& path : commandLine.gui.projectPaths) {
    if (isProjectPath(path)) {
      validatedPaths.append(QFileInfo(path).absoluteFilePath());
    }
  }
  validatedPaths.removeDuplicates();
  commandLine.gui.projectPaths = std::move(validatedPaths);

  return {std::move(commandLine), {}};
}

} // namespace Artifact
