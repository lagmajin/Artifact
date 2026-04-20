module;
#include <utility>
#include <wobjectimpl.h>
#include <QObject>
#include <QDebug>
#include <memory>

module Artifact.Service.Application;

import Artifact.Application.Manager;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;
import Artifact.Service.ClipboardManager;
import Artifact.Tool.Service;
import Artifact.Tool.Manager;

namespace Artifact {

W_OBJECT_IMPL(ApplicationService)

class ApplicationService::Impl {
public:
 bool initialized_ = false;
 std::unique_ptr<ArtifactClipboardService> clipboardService_;
 std::unique_ptr<ArtifactToolService> toolService_;
};

ApplicationService* ApplicationService::instance()
{
 static ApplicationService s_instance(nullptr);
 return &s_instance;
}

ApplicationService::ApplicationService(QObject* parent)
 : QObject(parent), impl_(new Impl())
{
}

ApplicationService::~ApplicationService()
{
 if (impl_->initialized_) {
  shutdown();
 }
 delete impl_;
}

bool ApplicationService::initialize()
{
 if (impl_->initialized_) {
  qWarning() << "[ApplicationService] already initialized";
  return true;
 }

 // Create owned services
 impl_->clipboardService_ = std::make_unique<ArtifactClipboardService>();
 impl_->toolService_ = std::make_unique<ArtifactToolService>();

 // Bind tool service to existing tool manager if available
 if (auto* app = ArtifactApplicationManager::instance()) {
  if (auto* toolMgr = app->toolManager()) {
   impl_->toolService_->setToolManager(toolMgr);
  }
 }

 connectServices();

 impl_->initialized_ = true;
 qDebug() << "[ApplicationService] initialized";
 initialized();
 return true;
}

void ApplicationService::shutdown()
{
 if (!impl_->initialized_) return;

 shutdownRequested();

 impl_->toolService_.reset();
 impl_->clipboardService_.reset();

 impl_->initialized_ = false;
 qDebug() << "[ApplicationService] shutdown";
}

bool ApplicationService::isInitialized() const
{
 return impl_->initialized_;
}

ArtifactApplicationManager* ApplicationService::applicationManager() const
{
 return ArtifactApplicationManager::instance();
}

ArtifactProjectService* ApplicationService::projectService() const
{
 if (auto* app = ArtifactApplicationManager::instance()) {
  return app->projectService();
 }
 return nullptr;
}

ArtifactActiveContextService* ApplicationService::activeContextService() const
{
 if (auto* app = ArtifactApplicationManager::instance()) {
  return app->activeContextService();
 }
 return nullptr;
}

ArtifactClipboardService* ApplicationService::clipboardService() const
{
 return impl_->clipboardService_.get();
}

ArtifactToolService* ApplicationService::toolService() const
{
 return impl_->toolService_.get();
}

QString ApplicationService::applicationVersion() const
{
 return QStringLiteral(ARTIFACT_VERSION_STRING);
}

bool ApplicationService::isProjectOpen() const
{
 return projectService() != nullptr;
}

void ApplicationService::connectServices()
{
 // Tool changes now flow through EventBus directly from ArtifactToolManager.
}

}
