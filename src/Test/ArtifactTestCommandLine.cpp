module;

#include <QDebug>
#include <QStringList>

export module Artifact.Test.CommandLine;

import Artifact.Application.CommandLine;

export namespace Artifact {

int runCommandLineTests()
{
  int failures = 0;
  const auto expect = [&failures](bool condition, const char* message) {
    if (!condition) {
      ++failures;
      qWarning().noquote() << "[Test][CommandLine]" << message;
    }
  };

  const CommandLine gui = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--lang"),
       QStringLiteral("ja"), QStringLiteral("project.artifact")});
  expect(gui.type == CommandType::Gui, "positional project keeps GUI command");
  expect(gui.global.languageCode == QStringLiteral("ja"), "language is global");
  expect(gui.gui.projectPaths == QStringList{QStringLiteral("project.artifact")},
         "GUI positional paths are collected");

  const CommandLine legacyProject = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--project"),
       QStringLiteral("legacy.artifact")});
  expect(legacyProject.gui.projectPaths == QStringList{QStringLiteral("legacy.artifact")},
         "legacy project option remains accepted");

  const CommandLine renderer = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--renderer"),
       QStringLiteral("Vulkan")});
  const CommandLineResult rendererResult = validateCommandLine(renderer);
  expect(rendererResult.isValid() &&
             rendererResult.commandLine.global.rendererBackend == QStringLiteral("vulkan"),
         "renderer backend is normalized");
  expect(!validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--renderer"),
       QStringLiteral("metal")})).isValid(),
         "unsupported renderer backend is rejected");
  expect(!validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--lang"),
       QStringLiteral("klingon")})).isValid(),
         "unsupported language is rejected");
  const CommandLineResult normalizedLanguage = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--lang"),
       QStringLiteral("Japanese")}));
  expect(normalizedLanguage.isValid() &&
             normalizedLanguage.commandLine.global.languageCode == QStringLiteral("ja"),
         "language aliases are normalized");

  const CommandLine diagnostics = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--safe-mode"),
       QStringLiteral("--verbose")});
  expect(diagnostics.global.safeMode && diagnostics.global.verbose,
         "safe mode and verbose flags are parsed globally");
  const CommandLine startup = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--no-splash"),
       QStringLiteral("--log-file"), QStringLiteral("artifact.log")});
  expect(startup.global.noSplash && startup.global.logFile == QStringLiteral("artifact.log"),
         "startup diagnostics options are parsed");
  const CommandLine threaded = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--threads"), QStringLiteral("6")});
  expect(validateCommandLine(threaded).isValid() &&
             threaded.global.renderThreads == 6,
         "render thread count is parsed");

  const CommandLine precedence = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--plugin-list"),
       QStringLiteral("--version"), QStringLiteral("--help")});
  expect(precedence.type == CommandType::Help,
         "legacy early-command precedence is preserved");

  const CommandLine optionValuePrecedence = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--plugin-info"),
       QStringLiteral("--help")});
  expect(optionValuePrecedence.type == CommandType::Help,
         "help is still recognized after an option expecting a value");

  const CommandLine interactive = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--interactive"),
       QStringLiteral("scene.artifact")});
  expect(interactive.type == CommandType::Interactive,
         "interactive mode is a first-class command");

  const CommandLine mcpDebug = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--mcp-debug"),
       QStringLiteral("--mcp-port"), QStringLiteral("4711")});
  expect(mcpDebug.type == CommandType::McpServer &&
             validateCommandLine(mcpDebug).isValid(),
         "legacy MCP debug arguments remain accepted");

  const CommandLine scripted = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--script"),
       QStringLiteral("startup.cli"), QStringLiteral("scene.artifact")});
  expect(scripted.type == CommandType::Interactive,
         "script mode uses the interactive command family");
  expect(scripted.gui.scriptRequested &&
             scripted.gui.scriptPath == QStringLiteral("startup.cli"),
         "script path is preserved while positional projects remain available");
  expect(scripted.gui.projectPaths == QStringList{QStringLiteral("scene.artifact")},
         "script mode still accepts a project path");

  const CommandLine render = parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.artifact"), QStringLiteral("--output"),
       QStringLiteral("preview.mp4"), QStringLiteral("--start"),
       QStringLiteral("2"), QStringLiteral("--end"), QStringLiteral("8")});
  expect(render.type == CommandType::Render &&
             render.render.inputPath == QStringLiteral("scene.artifact") &&
             render.render.outputPath == QStringLiteral("preview.mp4") &&
             render.render.startFrame == 2 && render.render.endFrame == 8,
         "render subcommand keeps its options separate from GUI options");
  expect(validateCommandLine(render).isValid(), "valid render range passes validation");
  const CommandLineResult invalidRender = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.txt")}));
  expect(!invalidRender.isValid(), "render rejects non-Artifact input");
  const CommandLineResult unknownOption = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("--typo")}));
  expect(!unknownOption.isValid(), "unknown options fail validation");
  const CommandLineResult missingRenderValue = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.artifact"), QStringLiteral("--output")}));
  expect(!missingRenderValue.isValid(), "render options require values");
  const CommandLineResult invalidFormat = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.artifact"), QStringLiteral("--format"),
       QStringLiteral("gif")}));
  expect(!invalidFormat.isValid(), "render rejects unsupported formats");
  const CommandLineResult normalizedFormat = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.artifact"), QStringLiteral("--format"),
       QStringLiteral("MP4")}));
  expect(normalizedFormat.isValid() &&
             normalizedFormat.commandLine.render.format == QStringLiteral("mp4"),
         "render formats are normalized");
  const CommandLineResult invalidFrame = validateCommandLine(parseCommandLine(
      {QStringLiteral("Artifact.exe"), QStringLiteral("render"),
       QStringLiteral("scene.artifact"), QStringLiteral("--start"),
       QStringLiteral("abc")}));
  expect(!invalidFrame.isValid(), "render rejects non-numeric frame values");

  const CommandLineResult missingScript = validateCommandLine(
      parseCommandLine({QStringLiteral("Artifact.exe"), QStringLiteral("--script")}));
  expect(!missingScript.isValid(), "script mode requires a command file");
  const CommandLineResult scriptHelpCollision = validateCommandLine(
      parseCommandLine({QStringLiteral("Artifact.exe"), QStringLiteral("--script"),
                        QStringLiteral("--help")}));
  expect(!scriptHelpCollision.isValid(), "script option cannot consume another option");

  const CommandLineResult missingPluginId = validateCommandLine(
      parseCommandLine({QStringLiteral("Artifact.exe"),
                        QStringLiteral("--plugin-info")}));
  expect(!missingPluginId.isValid(), "plugin-info requires its command argument");

  if (failures == 0) {
    qInfo().noquote() << "[Test][CommandLine] passed";
  }
  return failures;
}

} // namespace Artifact
