module;
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QTextStream>
#include <QVariant>


export module Artifact.AI.FileAutomation;

import std;
import Core.AI.Describable;

export namespace Artifact {

class FileAutomation : public ArtifactCore::IDescribable {
public:
  static void ensureRegistered() {
    static const bool registered = []() {
      ArtifactCore::DescriptionRegistry::instance().registerDescribable(
          QStringLiteral("FileAutomation"),
          []() -> const ArtifactCore::IDescribable * {
            return &FileAutomation::instance();
          });
      return true;
    }();
    (void)registered;
  }

  static FileAutomation &instance() {
    static FileAutomation automation;
    return automation;
  }

  QString className() const override {
    return QStringLiteral("FileAutomation");
  }

  ArtifactCore::LocalizedText briefDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "Provides basic file and directory operations.",
        "Provides basic file and directory operations.", {});
  }

  ArtifactCore::LocalizedText detailedDescription() const override {
    return ArtifactCore::IDescribable::loc(
        "This tool enables AI to perform basic file system operations like "
        "reading, writing, "
        "listing directories, and searching for files. Operations are "
        "sandboxed for security.",
        "This tool enables AI to perform basic file system operations like "
        "reading, writing, "
        "listing directories, and searching for files. Operations are "
        "sandboxed for security.",
        {});
  }

  QList<ArtifactCore::MethodDescription> methodDescriptions() const override {
    using ArtifactCore::IDescribable;
    return {
        {"readTextFile",
         IDescribable::loc("Read a text file and return its contents.",
                           "Read a text file and return its contents.", {}),
         "QString",
         {QStringLiteral("QString")},
         {QStringLiteral("filePath")}},
        {"writeTextFile",
         IDescribable::loc("Write text content to a file.",
                           "Write text content to a file.", {}),
         "bool",
         {QStringLiteral("QString"), QStringLiteral("QString")},
         {QStringLiteral("filePath"), QStringLiteral("content")}},
        {"listDirectory",
         IDescribable::loc("List files and directories in a path.",
                           "List files and directories in a path.", {}),
         "QVariantList",
         {QStringLiteral("QString")},
         {QStringLiteral("dirPath")}},
        {"fileExists",
         IDescribable::loc("Check if a file exists.", "Check if a file exists.",
                           {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("filePath")}},
        {"createDirectory",
         IDescribable::loc("Create a directory.", "Create a directory.", {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("dirPath")}},
        {"deleteFile",
         IDescribable::loc("Delete a file.", "Delete a file.", {}),
         "bool",
         {QStringLiteral("QString")},
         {QStringLiteral("filePath")}},
        {"getFileInfo",
         IDescribable::loc("Get file information (size, modified date, etc.).",
                           "Get file information (size, modified date, etc.).",
                           {}),
         "QVariantMap",
         {QStringLiteral("QString")},
         {QStringLiteral("filePath")}},
    };
  }

  QVariant invokeMethod(QStringView methodName,
                        const QVariantList &args) override {
    if (methodName == "readTextFile") {
      return readTextFile(args);
    } else if (methodName == "writeTextFile") {
      return writeTextFile(args);
    } else if (methodName == "listDirectory") {
      return listDirectory(args);
    } else if (methodName == "fileExists") {
      return fileExists(args);
    } else if (methodName == "createDirectory") {
      return createDirectory(args);
    } else if (methodName == "deleteFile") {
      return deleteFile(args);
    } else if (methodName == "getFileInfo") {
      return getFileInfo(args);
    }
    return QVariant();
  }

private:
  // Basic sandboxing - restrict to project directory and common safe
  // directories
  bool isPathAllowed(const QString &path) const {
    if (path.isEmpty())
      return false;

    // Allow project directory (would need to get from app manager)
    // For now, allow common directories
    QString canonicalPath = QFileInfo(path).canonicalFilePath();
    if (canonicalPath.startsWith(QDir::homePath() + "/Documents") ||
        canonicalPath.startsWith(QDir::homePath() + "/Desktop") ||
        canonicalPath.startsWith("/tmp") ||
        canonicalPath.startsWith(QDir::tempPath())) {
      return true;
    }
    return false;
  }

  QVariant readTextFile(const QVariantList &args) {
    if (args.isEmpty())
      return QVariant();
    QString filePath = args[0].toString();

    if (!isPathAllowed(filePath)) {
      return QVariant("Access denied: Path not allowed");
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QVariant("Failed to open file");
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    return content;
  }

  QVariant writeTextFile(const QVariantList &args) {
    if (args.size() < 2)
      return false;
    QString filePath = args[0].toString();
    QString content = args[1].toString();

    if (!isPathAllowed(filePath)) {
      return false; // Access denied
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();
    return true;
  }

  QVariant listDirectory(const QVariantList &args) {
    if (args.isEmpty())
      return QVariantList();
    QString dirPath = args[0].toString();

    if (!isPathAllowed(dirPath)) {
      return QVariantList(); // Empty list for access denied
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
      return QVariantList();
    }

    QVariantList result;
    QStringList entries =
        dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
      QVariantMap entryInfo;
      entryInfo["name"] = entry;
      QFileInfo info(dir.absoluteFilePath(entry));
      entryInfo["isDir"] = info.isDir();
      entryInfo["size"] = info.size();
      result.append(entryInfo);
    }
    return result;
  }

  QVariant fileExists(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    QString filePath = args[0].toString();
    return QFile::exists(filePath);
  }

  QVariant createDirectory(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    QString dirPath = args[0].toString();

    if (!isPathAllowed(dirPath)) {
      return false;
    }

    QDir dir;
    return dir.mkpath(dirPath);
  }

  QVariant deleteFile(const QVariantList &args) {
    if (args.isEmpty())
      return false;
    QString filePath = args[0].toString();

    if (!isPathAllowed(filePath)) {
      return false;
    }

    QFile file(filePath);
    return file.remove();
  }

  QVariant getFileInfo(const QVariantList &args) {
    if (args.isEmpty())
      return QVariantMap();
    QString filePath = args[0].toString();

    QFileInfo info(filePath);
    if (!info.exists()) {
      return QVariantMap();
    }

    QVariantMap result;
    result["exists"] = true;
    result["isFile"] = info.isFile();
    result["isDir"] = info.isDir();
    result["size"] = info.size();
    result["lastModified"] = info.lastModified().toString();
    result["canonicalPath"] = info.canonicalFilePath();
    return result;
  }
};

} // namespace Artifact
