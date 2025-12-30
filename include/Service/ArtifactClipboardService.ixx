module;
#include <QImage>
export module Artifact.Service.ClipboardManager;

import std;

export namespace Artifact {

 class ArtifactClipboardService {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactClipboardService();
  ~ArtifactClipboardService();
  void copyImageToClipboard(const QImage& image);
 };

};