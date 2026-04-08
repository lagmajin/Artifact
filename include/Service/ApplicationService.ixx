module;
#include <utility>

#include <QObject>
#include <QString>
#include <QDebug>
#include <wobjectdefs.h>
export module Artifact.Service.Application;

import Artifact.Service.ClipboardManager;
import Artifact.Tool.Service;
import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;

export namespace Artifact {

  class ApplicationService : public QObject
  {
   W_OBJECT(ApplicationService)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ApplicationService(QObject* parent = nullptr);
  ~ApplicationService();
  
  static ApplicationService* instance();

  bool initialize();
  void shutdown();
  bool isInitialized() const;

  // Service accessors
  ArtifactApplicationManager* applicationManager() const;
  ArtifactProjectService* projectService() const;
  ArtifactActiveContextService* activeContextService() const;
  ArtifactClipboardService* clipboardService() const;
  ArtifactToolService* toolService() const;

 // Status
  QString applicationVersion() const;
  bool isProjectOpen() const;

 signals:
  void initialized() W_SIGNAL(initialized);
  void shutdownRequested() W_SIGNAL(shutdownRequested);
  void projectOpened(const QString& projectPath) W_SIGNAL(projectOpened, projectPath);
  void projectClosed() W_SIGNAL(projectClosed);

 private:
  void connectServices();
 };

}
