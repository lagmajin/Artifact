module;

#include <QTextStream>
#include <QRegularExpression>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QSaveFile>
#include <memory>
#include <vector>
#include <utility>
#include <cmath>
#include <QString>
#include <QStringList>
#include <functional>
#include <map>

module Artifact.Application.InteractiveShell;

namespace Artifact {
namespace {

void printHelp(QTextStream& out)
{
  out << "Commands:\n"
      << "  help [--json]        Show this help\n"
      << "  project.info         Show the launch project\n"
      << "  project.open <path>  Open a nested project session\n"
      << "  project.json         Print project JSON\n"
      << "  project.stats [--json] Show project counts\n"
      << "  render.plan [--json] Show render plan\n"
      << "  render.enqueue <job> [--start n --end n] [--output file] [--format x] [--json] Create a render job manifest\n"
      << "  render.list [--json]   List render job manifests in the project folder\n"
      << "  render.status <job> [--json] Show render job status\n"
      << "  render.start <job>    Mark a render job running\n"
      << "  render.finish <job>   Mark a render job completed\n"
      << "  render.cancel <job>    Cancel a queued render job\n"
      << "  project.validate     Validate project JSON\n"
      << "  project.save [path]  Save project JSON\n"
      << "  composition.list [--json]  List compositions\n"
      << "  composition.select <name>  Select a composition\n"
      << "  layer.select <name>       Select a layer\n"
      << "  property.get <name> [--json] Read a layer property\n"
      << "  property.set <name> <v> [--json] Set a layer property\n"
      << "  undo, redo                Revert or reapply the last edit\n"
      << "  history                  Show edit history depth\n"
      << "  source <file>            Execute commands from a file\n"
      << "  complete <prefix>        List matching command names\n"
      << "  open/save/ls/select/get/set  DCC-style command aliases\n"
      << "  layer.list [name] [--json] List layers\n"
      << "  echo <text>          Print text\n"
      << "  quit, exit           Leave interactive mode\n";
}

QString commandName(const QString& line)
{
  const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
  return (separator < 0 ? line : line.left(separator)).toLower();
}

using CommandHandler = std::function<void(const QString&, QTextStream&, QTextStream&)>;
using HistoryEntry = std::pair<QJsonObject, QJsonObject>;

QJsonObject loadProjectJson(const QStringList& projectPaths, QTextStream& err)
{
  if (projectPaths.isEmpty()) {
    return {};
  }
  QFile file(projectPaths.constFirst());
  if (!file.open(QIODevice::ReadOnly)) {
    err << "Unable to read project: " << projectPaths.constFirst() << "\n";
    return {};
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (!document.isObject()) {
    err << "Invalid project JSON: " << parseError.errorString() << "\n";
    return {};
  }
  return document.object();
}

bool saveProjectJson(const QStringList& projectPaths, const QJsonObject& project, QTextStream& err)
{
  if (projectPaths.isEmpty()) {
    err << "No project is open.\n";
    return false;
  }
  QSaveFile file(projectPaths.constFirst());
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(project).toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    err << "Unable to save project: " << projectPaths.constFirst() << "\n";
    return false;
  }
  return true;
}

std::map<QString, CommandHandler> createCommandRegistry(const QStringList& projectPaths,
                                                         const std::shared_ptr<bool>& scriptFailed)
{
  std::map<QString, CommandHandler> commands;
  const auto selectedComposition = std::make_shared<QString>();
  const auto selectedLayer = std::make_shared<QString>();
  const auto undoStack = std::make_shared<std::vector<HistoryEntry>>();
  const auto redoStack = std::make_shared<std::vector<HistoryEntry>>();
  const auto activeScripts = std::make_shared<QSet<QString>>();
  const auto commandNames = std::make_shared<QStringList>();
  const auto commandStore = std::make_shared<std::map<QString, CommandHandler>>();
  const auto dispatch = std::make_shared<std::function<void(const QString&, QTextStream&, QTextStream&)>>();
  commands.emplace(QStringLiteral("help"), [commandNames](const QString& line, QTextStream& out, QTextStream&) {
    if (line.contains(QStringLiteral("--json"))) {
      QJsonArray names;
      for (const QString& command : *commandNames) names.append(command);
      out << QString::fromUtf8(QJsonDocument(names).toJson(QJsonDocument::Compact)) << "\n";
    } else {
      printHelp(out);
    }
  });
  commands.emplace(QStringLiteral("project.info"), [projectPaths](const QString&, QTextStream& out,
                                                                     QTextStream&) {
    out << (projectPaths.isEmpty() ? QStringLiteral("No project is open.\n")
                                   : QStringLiteral("Project: %1\n").arg(projectPaths.constFirst()));
  });
  commands.emplace(QStringLiteral("project.open"), [scriptFailed](const QString& line, QTextStream& out, QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString arguments = separator < 0 ? QString() : line.mid(separator).trimmed();
    const QString path = arguments.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0);
    if (path.isEmpty()) {
      err << "Usage: project.open <project.artifact>\n";
      return;
    }
    if (!QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
      err << "Project not found: " << path << "\n";
      return;
    }
    const QString lowerName = QFileInfo(path).fileName().toLower();
    if (!lowerName.endsWith(QStringLiteral(".artifact")) &&
        !lowerName.endsWith(QStringLiteral(".artifact.json"))) {
      err << "Not an Artifact project: " << path << "\n";
      return;
    }
    out << "Opening nested project session: " << QFileInfo(path).absoluteFilePath() << "\n";
    const InteractiveShellResult nestedSession =
        runInteractiveShell(QStringList{QFileInfo(path).absoluteFilePath()});
    if (nestedSession.exitCode != 0) {
      *scriptFailed = true;
    }
  });
  commands.emplace(QStringLiteral("project.json"), [projectPaths](const QString&, QTextStream& out,
                                                                     QTextStream& err) {
    const QJsonObject project = loadProjectJson(projectPaths, err);
    if (!project.isEmpty()) {
      out << QString::fromUtf8(QJsonDocument(project).toJson(QJsonDocument::Compact)) << "\n";
    }
  });
  commands.emplace(QStringLiteral("project.stats"), [projectPaths](const QString& line, QTextStream& out,
                                                                      QTextStream& err) {
    const QJsonObject project = loadProjectJson(projectPaths, err);
    if (project.isEmpty()) return;
    const QJsonArray compositions = project.value(QStringLiteral("compositions")).toArray();
    int layerCount = 0;
    for (const QJsonValue& value : compositions) {
      layerCount += value.toObject().value(QStringLiteral("layers")).toArray().size();
    }
    const int assetCount = project.value(QStringLiteral("assets")).toArray().size();
    if (line.contains(QStringLiteral("--json"))) {
      QJsonObject stats;
      stats[QStringLiteral("compositions")] = compositions.size();
      stats[QStringLiteral("layers")] = layerCount;
      stats[QStringLiteral("assets")] = assetCount;
      out << QString::fromUtf8(QJsonDocument(stats).toJson(QJsonDocument::Compact)) << "\n";
      return;
    }
    out << "Compositions: " << compositions.size() << "\n"
        << "Layers: " << layerCount << "\n"
        << "Assets: " << assetCount << "\n";
  });
  commands.emplace(QStringLiteral("render.plan"), [projectPaths](const QString& line, QTextStream& out,
                                                                     QTextStream& err) {
    const QJsonObject project = loadProjectJson(projectPaths, err);
    const QJsonArray compositions = project.value(QStringLiteral("compositions")).toArray();
    if (compositions.isEmpty()) {
      out << "No compositions to render.\n";
      return;
    }
    const bool jsonOutput = line.contains(QStringLiteral("--json"));
    QJsonArray plan;
    for (const QJsonValue& value : compositions) {
      const QJsonObject composition = value.toObject();
      const QJsonObject settings = composition.value(QStringLiteral("settings")).toObject();
      const int startFrame = composition.value(QStringLiteral("inPoint")).toInt(0);
      const int endFrame = composition.value(QStringLiteral("outPoint")).toInt(
          composition.value(QStringLiteral("duration")).toInt(0));
      const int width = settings.value(QStringLiteral("width")).toInt(
          composition.value(QStringLiteral("width")).toInt(0));
      const int height = settings.value(QStringLiteral("height")).toInt(
          composition.value(QStringLiteral("height")).toInt(0));
      const double frameRate = settings.value(QStringLiteral("frameRate")).toDouble(
          composition.value(QStringLiteral("frameRate")).toDouble(0.0));
      if (jsonOutput) {
        QJsonObject item;
        item[QStringLiteral("name")] = composition.value(QStringLiteral("name"));
        item[QStringLiteral("startFrame")] = startFrame;
        item[QStringLiteral("endFrame")] = endFrame;
        item[QStringLiteral("width")] = width;
        item[QStringLiteral("height")] = height;
        item[QStringLiteral("frameRate")] = frameRate;
        plan.append(item);
        continue;
      }
      out << composition.value(QStringLiteral("name")).toString()
          << ": frames " << startFrame << "-" << endFrame;
      if (width > 0 && height > 0) out << ", size " << width << "x" << height;
      if (frameRate > 0.0) out << ", fps " << frameRate;
      out << "\n";
    }
    if (jsonOutput) {
      out << QString::fromUtf8(QJsonDocument(plan).toJson(QJsonDocument::Compact)) << "\n";
    }
  });
  commands.emplace(QStringLiteral("render.enqueue"), [projectPaths](const QString& line, QTextStream& out,
                                                                        QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    if (separator < 0 || line.mid(separator).trimmed().isEmpty()) {
      err << "Usage: render.enqueue <job.json>\n";
      return;
    }
    const QJsonObject project = loadProjectJson(projectPaths, err);
    const QJsonArray compositions = project.value(QStringLiteral("compositions")).toArray();
    if (compositions.isEmpty()) {
      err << "No compositions to enqueue.\n";
      return;
    }
    QJsonObject job;
    job[QStringLiteral("type")] = QStringLiteral("artifact.render");
    job[QStringLiteral("project")] = projectPaths.value(0);
    job[QStringLiteral("compositions")] = compositions;
    job[QStringLiteral("createdBy")] = QStringLiteral("ArtifactInteractiveShell");
    job[QStringLiteral("status")] = QStringLiteral("queued");
    const QString arguments = line.mid(separator).trimmed();
    const bool jsonOutput = arguments.contains(QStringLiteral("--json"));
    const QStringList tokens = arguments.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const QString destination = tokens.value(0);
    const int outputIndex = tokens.indexOf(QStringLiteral("--output"));
    if (outputIndex >= 0 && outputIndex + 1 < tokens.size()) {
      job[QStringLiteral("output")] = tokens[outputIndex + 1];
    }
    const int formatIndex = tokens.indexOf(QStringLiteral("--format"));
    if (formatIndex >= 0 && formatIndex + 1 < tokens.size()) {
      const QString format = tokens[formatIndex + 1].toLower();
      const QStringList supportedFormats = {QStringLiteral("png"), QStringLiteral("jpg"),
                                            QStringLiteral("jpeg"), QStringLiteral("mp4"),
                                            QStringLiteral("mov"), QStringLiteral("webm")};
      if (!supportedFormats.contains(format)) {
        err << "Unsupported render format: " << format << "\n";
        return;
      }
      job[QStringLiteral("format")] = format;
    }
    const int startIndex = tokens.indexOf(QStringLiteral("--start"));
    const int endIndex = tokens.indexOf(QStringLiteral("--end"));
    const bool missingFormatValue = formatIndex >= 0 && formatIndex + 1 >= tokens.size();
    if ((startIndex >= 0 && startIndex + 1 >= tokens.size()) ||
        (endIndex >= 0 && endIndex + 1 >= tokens.size()) || missingFormatValue) {
      err << "Render frame options require a value.\n";
      return;
    }
    bool startOk = true;
    bool endOk = true;
    const int startFrame = startIndex >= 0 && startIndex + 1 < tokens.size()
        ? tokens[startIndex + 1].toInt(&startOk) : 0;
    const int endFrame = endIndex >= 0 && endIndex + 1 < tokens.size()
        ? tokens[endIndex + 1].toInt(&endOk) : -1;
    if (!startOk || !endOk || startFrame < 0 || (endFrame >= 0 && endFrame < startFrame)) {
      err << "Invalid render frame range.\n";
      return;
    }
    job[QStringLiteral("startFrame")] = startFrame;
    if (endFrame >= 0) job[QStringLiteral("endFrame")] = endFrame;
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(job).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
      err << "Unable to write render job: " << destination << "\n";
      return;
    }
    if (jsonOutput) {
      out << QString::fromUtf8(QJsonDocument(job).toJson(QJsonDocument::Compact)) << "\n";
    } else {
      out << "Render job written: " << destination << "\n";
    }
  });
  commands.emplace(QStringLiteral("render.list"), [projectPaths](const QString& line, QTextStream& out, QTextStream& err) {
    const bool jsonOutput = line.contains(QStringLiteral("--json"));
    if (projectPaths.isEmpty()) {
      err << "No project is open.\n";
      return;
    }
    const QDir directory(QFileInfo(projectPaths.constFirst()).absolutePath());
    const QStringList paths = directory.entryList(QStringList{QStringLiteral("*.artifact.render")}, QDir::Files, QDir::Name);
    QJsonArray jobs;
    for (const QString& name : paths) {
      QFile file(directory.filePath(name));
      if (!file.open(QIODevice::ReadOnly)) continue;
      QJsonObject job = QJsonDocument::fromJson(file.readAll()).object();
      if (!job.isEmpty()) jobs.append(job);
    }
    if (jsonOutput) {
      out << QString::fromUtf8(QJsonDocument(jobs).toJson(QJsonDocument::Compact)) << "\n";
      return;
    }
    for (const QJsonValue& value : jobs) {
      const QJsonObject job = value.toObject();
      out << job.value(QStringLiteral("job")).toString() << "  "
          << job.value(QStringLiteral("status")).toString(QStringLiteral("queued")) << "\n";
    }
    out << jobs.size() << " render job(s).\n";
  });
  commands.emplace(QStringLiteral("render.status"), [](const QString& line, QTextStream& out, QTextStream& err) {
    const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const bool jsonOutput = tokens.contains(QStringLiteral("--json"));
    const QString path = tokens.size() > 1 ? tokens.at(1) : QString();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
      err << "Unable to open render job: " << path << "\n";
      return;
    }
    const QJsonObject job = QJsonDocument::fromJson(file.readAll()).object();
    if (jsonOutput) {
      out << QString::fromUtf8(QJsonDocument(job).toJson(QJsonDocument::Compact)) << "\n";
    } else {
      out << "Status: " << job.value(QStringLiteral("status")).toString(QStringLiteral("queued")) << "\n";
    }
  });
  const auto setRenderStatus = [](const QString& line, const QString& status, QTextStream& out, QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString path = separator < 0 ? QString() : line.mid(separator).trimmed();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
      err << "Unable to open render job: " << path << "\n";
      return;
    }
    QJsonObject job = QJsonDocument::fromJson(file.readAll()).object();
    const QString current = job.value(QStringLiteral("status")).toString(QStringLiteral("queued"));
    if (status == QStringLiteral("running") && current == QStringLiteral("cancelled")) {
      err << "Cannot start a cancelled render job.\n";
      return;
    }
    job[QStringLiteral("status")] = status;
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(QJsonDocument(job).toJson(QJsonDocument::Indented)) < 0 ||
        !output.commit()) {
      err << "Unable to update render job: " << path << "\n";
      return;
    }
    out << "Status: " << status << "\n";
  };
  commands.emplace(QStringLiteral("render.start"), [setRenderStatus](const QString& line, QTextStream& out, QTextStream& err) {
    setRenderStatus(line, QStringLiteral("running"), out, err);
  });
  commands.emplace(QStringLiteral("render.finish"), [setRenderStatus](const QString& line, QTextStream& out, QTextStream& err) {
    setRenderStatus(line, QStringLiteral("completed"), out, err);
  });
  commands.emplace(QStringLiteral("render.cancel"), [](const QString& line, QTextStream& out, QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString path = separator < 0 ? QString() : line.mid(separator).trimmed();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
      err << "Unable to open render job: " << path << "\n";
      return;
    }
    QJsonObject job = QJsonDocument::fromJson(file.readAll()).object();
    job[QStringLiteral("status")] = QStringLiteral("cancelled");
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) ||
        output.write(QJsonDocument(job).toJson(QJsonDocument::Indented)) < 0 ||
        !output.commit()) {
      err << "Unable to update render job: " << path << "\n";
      return;
    }
    out << "Cancelled: " << path << "\n";
  });
  commands.emplace(QStringLiteral("echo"), [](const QString& line, QTextStream& out, QTextStream&) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    out << (separator < 0 ? QString() : line.mid(separator).trimmed()) << "\n";
  });
  commands.emplace(QStringLiteral("project.validate"), [projectPaths](const QString&, QTextStream& out,
                                                                        QTextStream& err) {
    const QJsonObject project = loadProjectJson(projectPaths, err);
    if (project.isEmpty()) {
      return;
    }
    const bool hasCompositions = project.value(QStringLiteral("compositions")).isArray();
    out << (hasCompositions ? "Project JSON is valid.\n"
                            : "Project JSON is valid, but has no compositions array.\n");
  });
  commands.emplace(QStringLiteral("project.save"), [projectPaths](const QString& line, QTextStream& out,
                                                                     QTextStream& err) {
    const QJsonObject project = loadProjectJson(projectPaths, err);
    if (project.isEmpty()) {
      return;
    }
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString destination = separator < 0 ? projectPaths.value(0)
                                               : line.mid(separator).trimmed();
    if (destination.isEmpty()) {
      err << "project.save requires a project path.\n";
      return;
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(project).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit()) {
      err << "Unable to save project: " << destination << "\n";
      return;
    }
    out << "Saved: " << destination << "\n";
  });
  commands.emplace(QStringLiteral("composition.select"), [projectPaths, selectedComposition](const QString& line,
                                                                                               QTextStream& out,
                                                                                               QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString requested = separator < 0 ? QString() : line.mid(separator).trimmed();
    const QJsonArray compositions = loadProjectJson(projectPaths, err).value(QStringLiteral("compositions")).toArray();
    for (const QJsonValue& value : compositions) {
      const QString name = value.toObject().value(QStringLiteral("name")).toString();
      if (name == requested) {
        *selectedComposition = name;
        out << "Selected composition: " << name << "\n";
        return;
      }
    }
    err << "Composition not found: " << requested << "\n";
  });
  commands.emplace(QStringLiteral("layer.select"), [projectPaths, selectedComposition, selectedLayer](const QString& line,
                                                                                                      QTextStream& out,
                                                                                                      QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString requested = separator < 0 ? QString() : line.mid(separator).trimmed();
    const QJsonArray compositions = loadProjectJson(projectPaths, err).value(QStringLiteral("compositions")).toArray();
    for (const QJsonValue& value : compositions) {
      const QJsonObject composition = value.toObject();
      if (!selectedComposition->isEmpty() && composition.value(QStringLiteral("name")).toString() != *selectedComposition) {
        continue;
      }
      for (const QJsonValue& layerValue : composition.value(QStringLiteral("layers")).toArray()) {
        if (layerValue.toObject().value(QStringLiteral("name")).toString() == requested) {
          *selectedLayer = requested;
          out << "Selected layer: " << requested << "\n";
          return;
        }
      }
    }
    err << "Layer not found: " << requested << "\n";
  });
  commands.emplace(QStringLiteral("property.get"), [projectPaths, selectedComposition, selectedLayer](const QString& line,
                                                                                                      QTextStream& out,
                                                                                                      QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    QString property = separator < 0 ? QString() : line.mid(separator).trimmed();
    const bool jsonOutput = property.contains(QStringLiteral("--json"));
    property.remove(QStringLiteral("--json"), Qt::CaseInsensitive);
    property = property.trimmed();
    const QJsonArray compositions = loadProjectJson(projectPaths, err).value(QStringLiteral("compositions")).toArray();
    for (const QJsonValue& value : compositions) {
      const QJsonObject composition = value.toObject();
      if (!selectedComposition->isEmpty() && composition.value(QStringLiteral("name")).toString() != *selectedComposition) continue;
      for (const QJsonValue& layerValue : composition.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (layer.value(QStringLiteral("name")).toString() == *selectedLayer) {
          if (!layer.contains(property)) { err << "Property not found: " << property << "\n"; return; }
          if (jsonOutput) {
            QJsonObject result;
            result[QStringLiteral("layer")] = *selectedLayer;
            result[QStringLiteral("property")] = property;
            result[QStringLiteral("value")] = layer.value(property);
            out << QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)) << "\n";
          } else {
            out << property << " = " << layer.value(property).toVariant().toString() << "\n";
          }
          return;
        }
      }
    }
    err << "No selected layer.\n";
  });
  commands.emplace(QStringLiteral("property.set"), [projectPaths, selectedComposition, selectedLayer, undoStack, redoStack](const QString& line,
                                                                                                      QTextStream& out,
                                                                                                      QTextStream& err) {
    QString commandLine = line;
    commandLine.remove(QStringLiteral("--json"), Qt::CaseInsensitive);
    const QStringList parts = commandLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() < 3) { err << "Usage: property.set <name> <value>\n"; return; }
    const QString property = parts[1];
    const QString rawValue = parts.mid(2).join(QLatin1Char(' '));
    const bool jsonOutput = line.contains(QStringLiteral("--json"));
    const bool numericProperty = property == QStringLiteral("opacity") ||
                                 property == QStringLiteral("labelColorIndex");
    const bool booleanProperty = property == QStringLiteral("isVisible") ||
                                 property == QStringLiteral("isLocked") ||
                                 property == QStringLiteral("isSolo");
    const bool stringProperty = property == QStringLiteral("layerNote");
    if (!numericProperty && !booleanProperty && !stringProperty) {
      err << "Property is not editable: " << property << "\n";
      return;
    }
    QJsonObject project = loadProjectJson(projectPaths, err);
    const QJsonObject before = project;
    QJsonArray compositions = project.value(QStringLiteral("compositions")).toArray();
    for (int ci = 0; ci < compositions.size(); ++ci) {
      QJsonObject composition = compositions[ci].toObject();
      if (!selectedComposition->isEmpty() && composition.value(QStringLiteral("name")).toString() != *selectedComposition) continue;
      QJsonArray layers = composition.value(QStringLiteral("layers")).toArray();
      for (int li = 0; li < layers.size(); ++li) {
        QJsonObject layer = layers[li].toObject();
        if (layer.value(QStringLiteral("name")).toString() != *selectedLayer) continue;
        bool ok = false; const double number = rawValue.toDouble(&ok);
        const bool isTrue = rawValue.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        const bool isFalse = rawValue.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0;
        if (booleanProperty && !isTrue && !isFalse) {
          err << "Expected true or false for " << property << "\n";
          return;
        }
        if (numericProperty && !ok) {
          err << "Expected a number for " << property << "\n";
          return;
        }
        if (property == QStringLiteral("opacity") && (number < 0.0 || number > 1.0)) {
          err << "Opacity must be between 0 and 1.\n";
          return;
        }
        if (property == QStringLiteral("labelColorIndex") && number != std::floor(number)) {
          err << "labelColorIndex must be an integer.\n";
          return;
        }
        if (isTrue) {
          layer[property] = true;
        } else if (isFalse) {
          layer[property] = false;
        } else {
          layer[property] = ok ? QJsonValue(number) : QJsonValue(rawValue);
        }
        layers[li] = layer; composition[QStringLiteral("layers")] = layers; compositions[ci] = composition;
        project[QStringLiteral("compositions")] = compositions;
        if (saveProjectJson(projectPaths, project, err)) {
          undoStack->push_back({before, project});
          redoStack->clear();
          if (jsonOutput) {
            QJsonObject result;
            result[QStringLiteral("layer")] = *selectedLayer;
            result[QStringLiteral("property")] = property;
            result[QStringLiteral("value")] = layer[property];
            out << QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)) << "\n";
          } else {
            out << "Set " << property << " = " << rawValue << "\n";
          }
        }
        return;
      }
    }
    err << "No selected layer.\n";
  });
  commands.emplace(QStringLiteral("undo"), [projectPaths, undoStack, redoStack](const QString&, QTextStream& out,
                                                                                  QTextStream& err) {
    if (undoStack->empty()) { out << "Nothing to undo.\n"; return; }
    const HistoryEntry entry = undoStack->back();
    undoStack->pop_back();
    if (saveProjectJson(projectPaths, entry.first, err)) {
      redoStack->push_back(entry);
      out << "Undone.\n";
    }
  });
  commands.emplace(QStringLiteral("redo"), [projectPaths, undoStack, redoStack](const QString&, QTextStream& out,
                                                                                  QTextStream& err) {
    if (redoStack->empty()) { out << "Nothing to redo.\n"; return; }
    const HistoryEntry entry = redoStack->back();
    redoStack->pop_back();
    if (saveProjectJson(projectPaths, entry.second, err)) {
      undoStack->push_back(entry);
      out << "Redone.\n";
    }
  });
  commands.emplace(QStringLiteral("history"), [undoStack, redoStack](const QString&, QTextStream& out,
                                                                       QTextStream&) {
    out << "Undo: " << undoStack->size() << ", Redo: " << redoStack->size() << "\n";
  });
  commands.emplace(QStringLiteral("source"), [dispatch, activeScripts](const QString& line, QTextStream& out, QTextStream& err) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    if (separator < 0 || line.mid(separator).trimmed().isEmpty()) {
      err << "Usage: source <file>\n";
      return;
    }
    QFile file(line.mid(separator).trimmed());
    const QString canonicalPath = QFileInfo(file.fileName()).absoluteFilePath();
    if (activeScripts->contains(canonicalPath)) {
      err << "Recursive source detected: " << canonicalPath << "\n";
      return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      err << "Unable to open command file: " << file.fileName() << "\n";
      return;
    }
    activeScripts->insert(canonicalPath);
    while (!file.atEnd()) {
      const QString commandLine = QString::fromUtf8(file.readLine()).trimmed();
      if (!commandLine.isEmpty() && !commandLine.startsWith(QLatin1Char('#'))) {
        (*dispatch)(commandLine, out, err);
      }
    }
    activeScripts->remove(canonicalPath);
  });
  commands.emplace(QStringLiteral("complete"), [commandNames](const QString& line, QTextStream& out, QTextStream&) {
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    const QString prefix = separator < 0 ? QString() : line.mid(separator).trimmed().toLower();
    for (const QString& command : *commandNames) {
      if (prefix.isEmpty() || command.startsWith(prefix)) {
        out << command << "\n";
      }
    }
  });
  *dispatch = [commandStore, scriptFailed](const QString& line, QTextStream& out, QTextStream& err) {
    const QString name = commandName(line);
    if (name == QStringLiteral("quit") || name == QStringLiteral("exit")) {
      return;
    }
    if (const auto command = commandStore->find(name); command != commandStore->end()) {
      command->second(line, out, err);
    } else {
      *scriptFailed = true;
      err << "Unknown command: " << line << "\n";
    }
  };
  commands.emplace(QStringLiteral("composition.list"), [projectPaths](const QString& line, QTextStream& out,
                                                                         QTextStream& err) {
    const QJsonArray compositions = loadProjectJson(projectPaths, err).value(QStringLiteral("compositions")).toArray();
    if (line.contains(QStringLiteral("--json"))) {
      out << QString::fromUtf8(QJsonDocument(compositions).toJson(QJsonDocument::Compact)) << "\n";
      return;
    }
    if (compositions.isEmpty()) {
      out << "No compositions.\n";
      return;
    }
    for (int index = 0; index < compositions.size(); ++index) {
      const QJsonObject composition = compositions[index].toObject();
      out << index << ": " << composition.value(QStringLiteral("name")).toString()
          << " (" << composition.value(QStringLiteral("layers")).toArray().size() << " layers)\n";
    }
  });
  commands.emplace(QStringLiteral("layer.list"), [projectPaths, selectedComposition](const QString& line, QTextStream& out,
                                                                   QTextStream& err) {
    const QJsonArray compositions = loadProjectJson(projectPaths, err).value(QStringLiteral("compositions")).toArray();
    const int separator = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
    QString requested = separator < 0 ? *selectedComposition : line.mid(separator).trimmed();
    const bool jsonOutput = requested.contains(QStringLiteral("--json"));
    requested.remove(QStringLiteral("--json"), Qt::CaseInsensitive);
    requested = requested.trimmed();
    QJsonArray outputLayers;
    for (const QJsonValue& value : compositions) {
      const QJsonObject composition = value.toObject();
      if (!requested.isEmpty() && composition.value(QStringLiteral("name")).toString() != requested) {
        continue;
      }
      const QJsonArray layers = composition.value(QStringLiteral("layers")).toArray();
      if (jsonOutput) {
        for (const QJsonValue& layer : layers) outputLayers.append(layer);
        if (!requested.isEmpty()) break;
        continue;
      }
      out << composition.value(QStringLiteral("name")).toString() << ":\n";
      for (int index = 0; index < layers.size(); ++index) {
        out << "  " << index << ": " << layers[index].toObject().value(QStringLiteral("name")).toString() << "\n";
      }
      if (!requested.isEmpty()) {
        return;
      }
    }
    if (jsonOutput) {
      out << QString::fromUtf8(QJsonDocument(outputLayers).toJson(QJsonDocument::Compact)) << "\n";
    }
  });
  // Familiar DCC-style aliases keep the shell approachable without creating a
  // second command implementation or a growing set of boolean modes.
  commands.emplace(QStringLiteral("open"), commands.at(QStringLiteral("project.open")));
  commands.emplace(QStringLiteral("save"), commands.at(QStringLiteral("project.save")));
  commands.emplace(QStringLiteral("ls"), commands.at(QStringLiteral("composition.list")));
  commands.emplace(QStringLiteral("select"), commands.at(QStringLiteral("composition.select")));
  commands.emplace(QStringLiteral("get"), commands.at(QStringLiteral("property.get")));
  commands.emplace(QStringLiteral("set"), commands.at(QStringLiteral("property.set")));
  for (const auto& command : commands) {
    commandNames->append(command.first);
  }
  *commandStore = commands;
  return commands;
}

} // namespace

InteractiveShellResult runInteractiveShell(const QStringList& projectPaths, const QString& scriptPath)
{
  QTextStream in(stdin);
  QTextStream out(stdout);
  QTextStream err(stderr);
  const bool scriptedMode = !scriptPath.isEmpty();
  if (!scriptedMode) {
    out << "Artifact interactive mode. Type 'help' for commands.\n";
  }
  if (!scriptedMode && !projectPaths.isEmpty()) {
    out << "Project: " << projectPaths.constFirst() << "\n";
  }
  const auto scriptFailed = std::make_shared<bool>(false);
  const auto commands = createCommandRegistry(projectPaths, scriptFailed);
  const auto dispatchLine = [&commands, scriptFailed](const QString& line, QTextStream& output, QTextStream& error) {
    const QString name = commandName(line);
    if (name == QStringLiteral("quit") || name == QStringLiteral("exit")) {
      return;
    }
    if (const auto command = commands.find(name); command != commands.end()) {
      command->second(line, output, error);
    } else {
      *scriptFailed = true;
      error << "Unknown command: " << line << "\n";
    }
  };

  if (!scriptPath.isEmpty()) {
    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      err << "Unable to open command file: " << scriptPath << "\n";
      return {2, false};
    }
    while (!file.atEnd()) {
      const QString commandLine = QString::fromUtf8(file.readLine()).trimmed();
      if (!commandLine.isEmpty() && !commandLine.startsWith(QLatin1Char('#'))) {
        dispatchLine(commandLine, out, err);
      }
    }
    return {*scriptFailed ? 1 : 0, false};
  }

  while (true) {
    out << "artifact> " << Qt::flush;
    if (in.atEnd()) {
      out << "\n";
      return {};
    }

    const QString line = in.readLine().trimmed();
    if (line.isEmpty()) {
      continue;
    }

    const QString name = commandName(line);
    if (name == QStringLiteral("quit") || name == QStringLiteral("exit")) {
      return {0, true};
    } else {
      dispatchLine(line, out, err);
    }
  }
}

} // namespace Artifact
