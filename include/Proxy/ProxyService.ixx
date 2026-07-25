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

 enum class ProxyServiceQuality {
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
  ProxyServiceQuality quality = ProxyServiceQuality::None;
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
  QString generateProxy(const QString& sourcePath, ProxyServiceQuality quality);

  // Clear (delete) proxy for a source file
  bool clearProxy(const QString& proxyPath);

  // Check if proxy exists for a source file
  bool hasProxy(const QString& sourcePath, ProxyServiceQuality quality) const;

  // Get proxy info for a source file
  ProxyServiceInfo getProxyInfo(const QString& sourcePath, ProxyServiceQuality quality) const;

  // Batch generate proxies
  int generateProxiesBatch(const QStringList& sourcePaths, ProxyServiceQuality quality);

  // Get proxy directory for a source file
  static QString proxyDirectory(const QString& sourcePath);

  // Get proxy file path for a source file and quality
  static QString proxyFilePath(const QString& sourcePath, ProxyServiceQuality quality);

 private:
  ArtifactProxyManager() = default;
  ~ArtifactProxyManager() = default;

  static double scaleFactor(ProxyServiceQuality quality) {
   switch (quality) {
    case ProxyServiceQuality::Half: return 0.5;
    case ProxyServiceQuality::Quarter: return 0.25;
    case ProxyServiceQuality::Eighth: return 0.125;
    default: return 1.0;
   }
  }
 };

}
