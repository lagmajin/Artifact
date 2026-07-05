module;

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QProcess>
#include <QString>
#include <QStringList>

export module Artifact.Plugin.Loader;

import ArtifactCore.Plugin.Common;
import ArtifactCore.Plugin.Registry;

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactPluginLoader {
public:
    ArtifactPluginLoader();
    ~ArtifactPluginLoader();

    void discoverAndLoad(const QStringList& searchPaths,
                         PluginLoadMode mode = PluginLoadMode::Auto);
    LoadResult loadPlugin(const QString& path,
                          PluginLoadMode mode = PluginLoadMode::Auto);
    void unloadAll();
    std::vector<LoadResult> loadResults() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
