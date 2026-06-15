module;
#include <QImage>
#include <QClipboard>
#include <QGuiApplication>
module Artifact.Service.ClipboardManager;


namespace Artifact {

 class ArtifactClipboardService::Impl {
 private:

  public:
  Impl();
  ~Impl();
  void setImage();
  void setText();
  void setPlainText(const UniString& text);
 };

 ArtifactClipboardService::Impl::Impl()
 {

 }

 ArtifactClipboardService::Impl::~Impl()
 {

 }

 void ArtifactClipboardService::Impl::setPlainText(const UniString& text)
 {

 }

 ArtifactClipboardService::ArtifactClipboardService() :impl_(new Impl())
 {

 }

 ArtifactClipboardService::~ArtifactClipboardService()
 {
  delete impl_;
 }

 void ArtifactClipboardService::copyImageToClipboard(const QImage& image)
 {
  QClipboard* clipboard = QGuiApplication::clipboard();
  if (clipboard && !image.isNull()) {
   clipboard->setImage(image);
  }
 }

 void ArtifactClipboardService::copyPlainText(const UniString& text)
 {
  QClipboard* clipboard = QGuiApplication::clipboard();
  if (clipboard) {
   clipboard->setText(text);
  }
 }

};