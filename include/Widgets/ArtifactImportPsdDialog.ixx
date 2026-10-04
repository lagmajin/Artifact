module;
#include <QDialog>
#include <QString>
#include <QVector>
#include <QWidget>

export module Artifact.Widgets.ImportPsdDialog;

import Image.PSDDocument;

export namespace Artifact {

/// 1 つの PSD レイヤーの選択結果。
/// PsdDocument の parsed layer と同じ順序で添字が対応する。
struct ArtifactPsdLayerSelection {
  int layerIndex = -1;
  QString name;
  bool selected = false;
};

/// PSD ファイルを開き、レイヤー一覧から取り込む層を選択させるダイアログ。
/// 選択されなかったレイヤーは composition に追加されない。
class ArtifactImportPsdDialog final : public QDialog {
public:
  explicit ArtifactImportPsdDialog(const QString& filePath, QWidget* parent = nullptr);

  /// true のときだけ PSD のレイヤー情報が読み込めた。
  bool documentOpened() const;

  /// PSD の元の indexing 順で layerIndex を並べた選択結果。
  QVector<ArtifactPsdLayerSelection> selectedLayers() const;

private:
  class Impl;
  Impl* impl_;
};

} // namespace Artifact