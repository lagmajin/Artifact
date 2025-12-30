module;
#include <QImage>
module Artifact.Service.ClipboardManager;


namespace Artifact {

 class ArtifactClipboardService::Impl {
 private:

  public:
  Impl();
  ~Impl();
 };

 ArtifactClipboardService::Impl::Impl()
 {

 }

 ArtifactClipboardService::Impl::~Impl()
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

 }

};