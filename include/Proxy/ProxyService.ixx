module;
#include <utility>
#include <memory>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QStringList>

export module Proxy.Service;

import Utils.String.UniString;

export namespace Artifact
{
 using namespace ArtifactCore;

 // ============================================================
 // Proxy Quality Enum
 // ============================================================

 enum class ProxyQuality {
  None = 0,
  Half = 1,
  Quarter = 2,
  Eighth = 3,
 };

 // ============================================================
 // Proxy Info (returned by service)
 // ============================================================

 struct ProxyServiceInfo {
  bool hasProxy = false;
  QString proxyPath;
  QString sourcePath;
  ProxyQuality quality = ProxyQuality::None;
  int proxyWidth = 0;
  int proxyHeight = 0;
  float scaleFactor = 1.0f;
 };

 // ============================================================
 // Proxy Manager Service
 // ============================================================

 class ArtifactProxyManager
 {
 public:
  static ArtifactProxyManager* instance();

  // Generate proxy for a source file
  // Returns the generated proxy path, or empty string on failure
  QString generateProxy(const QString& sourcePath, ProxyQuality quality);

  // Clear (delete) proxy for a source file
  bool clearProxy(const QString& proxyPath);

  // Check if proxy exists for a source file
  bool hasProxy(const QString& sourcePath, ProxyQuality quality) const;

  // Get proxy info for a source file
  ProxyServiceInfo getProxyInfo(const QString& sourcePath, ProxyQuality quality) const;

  // Batch generate proxies
  int generateProxiesBatch(const QStringList& sourcePaths, ProxyQuality quality);

  // Get proxy directory for a source file
  static QString proxyDirectory(const QString& sourcePath);

  // Get proxy file path for a source file and quality
  static QString proxyFilePath(const QString& sourcePath, ProxyQuality quality);

 private:
  ArtifactProxyManager() = default;
  ~ArtifactProxyManager() = default;

  static double scaleFactor(ProxyQuality quality) {
   switch (quality) {
    case ProxyQuality::Half: return 0.5;
    case ProxyQuality::Quarter: return 0.25;
    case ProxyQuality::Eighth: return 0.125;
    default: return 1.0;
   }
  }
 };

}
