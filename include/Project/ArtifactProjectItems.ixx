module;
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QVector>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QUuid>
export module Artifact.Project.Items;




import Utils.Id;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;

 enum class eProjectItemType {
  Unknown,
  Folder,
  Composition,
  Footage,
  Solid
 };

 enum class ProjectAssetUsage {
  Production,
  RenderInput
 };

 enum class ProjectRenderInputRole {
  Generic,
  AlphaMatte,
  LumaMatte,
  DisplacementMap,
  DepthMap,
  NormalMap,
  Texture
 };



 class ProjectItem {
 public:
  virtual ~ProjectItem() = default;
  virtual eProjectItemType type() const = 0;

  Id id;
  UniString name;
  QStringList tags;
  ProjectItem* parent = nullptr;
  QVector<ProjectItem*> children; // non-owning pointers; ownership managed by project
 };

 // 2. ۃNXiKvȃf[^̂ݕێj
 class FootageItem : public ProjectItem {
 public:
 eProjectItemType type() const override { return eProjectItemType::Footage; }
  QString filePath;
  // Stable logical asset identity; filePath remains the resolved local source.
  QUuid assetId;
  QStringList sequencePaths;
  double frameRate = 0.0;
  bool isSequence = false;
  // OIIO subimage index for layered formats such as PSD; -1 means flattened.
  int subimageIndex = -1;
 QString inputColorSpace;
   QString inputTransferFunction;
   ProjectAssetUsage assetUsage = ProjectAssetUsage::Production;
   ProjectRenderInputRole renderInputRole = ProjectRenderInputRole::Generic;
   // Proxy generation settings authored from the Project View. These used to
   // live in a process-local static map, so a chosen quality or the enabled
   // toggle was lost on every project reload.
   int proxyQuality = 2; // ProxyServiceQuality::Half
   bool proxyEnabled = true;
   QString proxyQualityLabel;
 };

class FolderItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Folder; }
  FolderItem* addChildFolder(const UniString& name);
};

class SolidItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Solid; }
  QColor color;
  int width = 1920;
  int height = 1080;
  double pixelAspectRatio = 1.0;

 };

class CompositionItem : public ProjectItem {
 public:
  eProjectItemType type() const override { return eProjectItemType::Composition; }
  ArtifactCore::CompositionID compositionId;
};

 // Render input role の表示名。UI 側で複数箇所にコピペされていたため、
 // enum の定義と同時にここへ正本を用意する。
 inline QString projectRenderInputRoleLabel(ProjectRenderInputRole role)
 {
   switch (role) {
   case ProjectRenderInputRole::AlphaMatte: return QStringLiteral("Alpha Matte");
   case ProjectRenderInputRole::LumaMatte: return QStringLiteral("Luma Matte");
   case ProjectRenderInputRole::DisplacementMap: return QStringLiteral("Displacement");
   case ProjectRenderInputRole::DepthMap: return QStringLiteral("Depth");
   case ProjectRenderInputRole::NormalMap: return QStringLiteral("Normal");
   case ProjectRenderInputRole::Texture: return QStringLiteral("Texture");
default: return QStringLiteral("Render Input");
   }
  }

 // Project View の行アイコンとインスペクタ表示が別々の拡張子表を持っていたため、
 // 判定と表示名の正本をここへ集約する。
 enum class ProjectAssetKind { Image, Video, Audio, Font, Model, Other };

 inline ProjectAssetKind projectAssetKindFromPath(const QString& path)
 {
   const QString lower = path.toLower();
   if (lower.endsWith(".png") || lower.endsWith(".jpg") ||
       lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
       lower.endsWith(".gif") || lower.endsWith(".tga") ||
       lower.endsWith(".tiff") || lower.endsWith(".tif") ||
       lower.endsWith(".exr") || lower.endsWith(".svg") ||
       lower.endsWith(".webp") || lower.endsWith(".hdr") ||
       lower.endsWith(".dds") || lower.endsWith(".ktx") ||
       lower.endsWith(".ktx2") || lower.endsWith(".avif") ||
       lower.endsWith(".heic") || lower.endsWith(".jxl") ||
       lower.endsWith(".jp2") || lower.endsWith(".psd") ||
       lower.endsWith(".psb")) {
     return ProjectAssetKind::Image;
   }
   if (lower.endsWith(".mp4") || lower.endsWith(".mov") ||
       lower.endsWith(".avi") || lower.endsWith(".mkv") ||
       lower.endsWith(".webm") || lower.endsWith(".m4v") ||
       lower.endsWith(".flv") || lower.endsWith(".m2ts") ||
       lower.endsWith(".ts") || lower.endsWith(".mpg") ||
       lower.endsWith(".mpeg") || lower.endsWith(".wmv") ||
       lower.endsWith(".3gp") || lower.endsWith(".3g2") ||
       lower.endsWith(".ogv") || lower.endsWith(".ogm") ||
       lower.endsWith(".mts") || lower.endsWith(".mxf") ||
       lower.endsWith(".vob") || lower.endsWith(".asf")) {
     return ProjectAssetKind::Video;
   }
   if (lower.endsWith(".mp3") || lower.endsWith(".wav") ||
       lower.endsWith(".ogg") || lower.endsWith(".flac") ||
       lower.endsWith(".aac") || lower.endsWith(".m4a")) {
     return ProjectAssetKind::Audio;
   }
   if (lower.endsWith(".ttf") || lower.endsWith(".otf") ||
       lower.endsWith(".ttc") || lower.endsWith(".woff") ||
       lower.endsWith(".woff2")) {
     return ProjectAssetKind::Font;
   }
   if (lower.endsWith(".obj") || lower.endsWith(".fbx") ||
       lower.endsWith(".gltf") || lower.endsWith(".glb") ||
       lower.endsWith(".pmd") || lower.endsWith(".ply") ||
       lower.endsWith(".las") || lower.endsWith(".usd") ||
       lower.endsWith(".usda") || lower.endsWith(".usdc") ||
       lower.endsWith(".usdz") || lower.endsWith(".abc") ||
       lower.endsWith(".blend") || lower.endsWith(".dae") ||
       lower.endsWith(".pmx") || lower.endsWith(".stl")) {
     return ProjectAssetKind::Model;
   }
   return ProjectAssetKind::Other;
 }

 inline QString projectAssetKindLabel(ProjectAssetKind kind)
 {
   switch (kind) {
   case ProjectAssetKind::Image: return QStringLiteral("Image");
   case ProjectAssetKind::Video: return QStringLiteral("Video");
   case ProjectAssetKind::Audio: return QStringLiteral("Audio");
   case ProjectAssetKind::Font: return QStringLiteral("Font");
   case ProjectAssetKind::Model: return QStringLiteral("3D");
   default: return QStringLiteral("Footage");
   }
 }

};
