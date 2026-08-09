module;
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStringList>
#include <QWidget>

module Artifact.Export.Dialog;

import Artifact.Export.LottieWriter;
import Artifact.Export.Session;
import Artifact.Export.RmlUiWriter;
import Artifact.Export.GamefaceWriter;
import Artifact.Export.UnityUxmlWriter;
import Artifact.Export.NoesisXamlWriter;

namespace Artifact {

bool ArtifactExportDialog::run(QWidget* parent,
                               const ArtifactCompositionPtr& composition,
                               QString* errorMessage) {
    const QStringList formats = {
        QStringLiteral("Lottie JSON"),
        QStringLiteral("RmlUi (.rml + .rcss)"),
        QStringLiteral("Coherent Gameface (HTML/CSS/JS)"),
        QStringLiteral("Unity UI Toolkit (.uxml + .uss)"),
        QStringLiteral("NoesisGUI XAML")};
    bool accepted = false;
    const QString format = QInputDialog::getItem(
        parent, QStringLiteral("Composition Export"),
        QStringLiteral("出力形式:"), formats, 0, false, &accepted);
    if (!accepted || format.isEmpty()) return false;

    QString path;
    const bool lottie = format == formats.at(0);
    if (lottie) {
        path = QFileDialog::getSaveFileName(
            parent, QStringLiteral("Lottie JSONを書き出す"), QString(),
            QStringLiteral("Lottie JSON (*.json);;All Files (*.*)"));
    } else {
        path = QFileDialog::getExistingDirectory(
            parent, QStringLiteral("Composition Exportの出力先"), QString(),
            QFileDialog::ShowDirsOnly);
    }
    if (path.isEmpty()) return false;
    if (lottie && !path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".json");
    }

    ArtifactExportSession preview(composition);
    if (!preview.build(errorMessage)) {
        if (parent) {
            QMessageBox::warning(
                parent, QStringLiteral("Composition Export"),
                errorMessage && !errorMessage->isEmpty()
                    ? *errorMessage
                    : QStringLiteral("エクスポート対象を準備できませんでした。"));
        }
        return false;
    }
    QStringList warnings = preview.snapshot().warnings;
    if (!warnings.isEmpty() && parent) {
        const QString warningText =
            QStringLiteral("一部のレイヤーはネイティブ変換できないため、フレーム列へベイクされます。\n\n%1\n\n続行しますか?")
                .arg(warnings.join(QStringLiteral("\n")));
        if (QMessageBox::warning(parent, QStringLiteral("Composition Export"),
                                 warningText, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) != QMessageBox::Yes) {
            return false;
        }
    }

    bool scaleAccepted = false;
    const int scalePercent = QInputDialog::getInt(
        parent, QStringLiteral("Composition Export"),
        QStringLiteral("プリレンダー解像度倍率 (%):"),
        100, 100, 400, 25, &scaleAccepted);
    if (!scaleAccepted) return false;
    const double preRenderScale = static_cast<double>(scalePercent) / 100.0;

    bool success = false;
    if (lottie) {
        ArtifactLottieExportOptions options;
        options.preRenderScale = preRenderScale;
        success = ArtifactExportLottieWriter::write(composition, path, options, errorMessage);
    } else if (format == formats.at(1)) {
        ArtifactRmlUiExportOptions options;
        options.preRenderScale = preRenderScale;
        success = ArtifactExportRmlUiWriter::write(composition, path, options, errorMessage);
    } else if (format == formats.at(2)) {
        ArtifactGamefaceExportOptions options;
        options.preRenderScale = preRenderScale;
        success = ArtifactExportGamefaceWriter::write(composition, path, options, errorMessage);
    } else if (format == formats.at(3)) {
        ArtifactUnityUxmlExportOptions options;
        options.preRenderScale = preRenderScale;
        success = ArtifactExportUnityUxmlWriter::write(composition, path, options, errorMessage);
    } else {
        ArtifactNoesisXamlExportOptions options;
        options.preRenderScale = preRenderScale;
        success = ArtifactExportNoesisXamlWriter::write(composition, path, options, errorMessage);
    }
    if (!success && parent) {
        QMessageBox::warning(
            parent, QStringLiteral("Composition Export"),
            errorMessage && !errorMessage->isEmpty()
                ? *errorMessage
                : QStringLiteral("エクスポートに失敗しました。"));
    }
    return success;
}

} // namespace Artifact
