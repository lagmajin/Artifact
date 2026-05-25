module;
#include <utility>
#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QUrl>
#include <wobjectimpl.h>

module Menu.Test2;

import Utils.Path;

namespace Artifact {

namespace {
QDir findShaderAssetsDir()
{
 QDir dir(QCoreApplication::applicationDirPath());
 for (int depth = 0; depth < 8; ++depth) {
  const QString candidate = dir.filePath(QStringLiteral("assets/shaders/app"));
  if (QFileInfo::exists(candidate)) {
   return QDir(candidate);
  }
  if (!dir.cdUp()) {
   break;
  }
 }
 return {};
}
}

W_OBJECT_IMPL(ArtifactImageProcessingTestMenu)

class ArtifactImageProcessingTestMenuPrivate {
public:
 ArtifactImageProcessingTestMenuPrivate() = default;
 ~ArtifactImageProcessingTestMenuPrivate() = default;

 void openShaderAssetsFolder(QWidget* parent)
 {
  const QDir shaderDir = findShaderAssetsDir();
  if (!shaderDir.exists()) {
   QMessageBox::information(
       parent,
       QStringLiteral("Image Processing"),
       QStringLiteral("assets/shaders/app を見つけられませんでした。"));
   return;
  }

  QDesktopServices::openUrl(QUrl::fromLocalFile(shaderDir.absolutePath()));
 }
};

ArtifactImageProcessingTestMenu::ArtifactImageProcessingTestMenu(QWidget* parent /*= nullptr*/)
 : QMenu(parent), pImpl_(new ArtifactImageProcessingTestMenuPrivate())
{
 setTitle("Image Processing");
 setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/test.svg")));

 auto* openShaderAssetsAction = addAction("Shader Assets Folder を開く...");
 openShaderAssetsAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/folder_open.svg")));
 QObject::connect(openShaderAssetsAction, &QAction::triggered, this, [this]() {
  if (pImpl_) {
   pImpl_->openShaderAssetsFolder(this);
  }
 });
}

ArtifactImageProcessingTestMenu::~ArtifactImageProcessingTestMenu()
{
}

void ArtifactImageProcessingTestMenu::imageProcessingTest()
{
 if (pImpl_) {
  pImpl_->openShaderAssetsFolder(this);
 }
}

};
