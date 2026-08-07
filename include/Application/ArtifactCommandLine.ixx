module;

#include <QString>
#include <QStringList>

export module Artifact.Application.CommandLine;

export namespace Artifact {

enum class CommandType {
  Gui,
  Help,
  Version,
  McpServer,
  Interactive,
  PluginList,
  PluginInfo,
  Render,
};

struct GlobalOptions {
  QString languageCode;
  QString rendererBackend = QStringLiteral("auto");
  bool safeMode = false;
  bool verbose = false;
  QString logFile;
  bool noSplash = false;
  int renderThreads = 0;
};

struct GuiCommand {
  QStringList projectPaths;
  QString scriptPath;
  bool scriptRequested = false;
};

// Reserved command payload for the future headless renderer. Keeping this
// separate prevents render-only options from accumulating on GuiCommand.
struct RenderCommand {
  QString inputPath;
  QString outputPath;
  QString format;
  int startFrame = -1;
  int endFrame = -1;
};

struct PluginInfoCommand {
  QString pluginId;
};

struct CommandLine {
  CommandType type = CommandType::Gui;
  GlobalOptions global;
  GuiCommand gui;
  RenderCommand render;
  PluginInfoCommand pluginInfo;
  QStringList unknownOptions;
  QStringList missingOptionValues;
  QStringList invalidOptionValues;
};

struct CommandLineResult {
  CommandLine commandLine;
  QString errorMessage;

  [[nodiscard]] bool isValid() const { return errorMessage.isEmpty(); }
};

[[nodiscard]] CommandLine parseCommandLine(const QStringList& arguments);
[[nodiscard]] CommandLineResult validateCommandLine(CommandLine commandLine);

} // namespace Artifact
