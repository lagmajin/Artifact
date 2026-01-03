module;
#include <QImage>
export module Artifact.Service.ClipboardManager;

import std;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;

 class ArtifactClipboardService {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactClipboardService();
  ~ArtifactClipboardService();
  void copyPlainText(const UniString& text);
  void copyImageToClipboard(const QImage& image);
 };

};