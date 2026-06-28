module;
#include <QVector>
#include <QWidget>
#include <QFormLayout>
#include <wobjectimpl.h>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>
#include <QBoxLayout>
#include <QCryptographicHash>
#include <QLabel>
#include <QClipboard>
#include <QEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDropEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QVariant>
#include <QStandardItem>
#include <QModelIndex>
#include <QItemSelection>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QPixmap>
#include <QIcon>
#include <QSizePolicy>
#include <QBrush>
#include <QPointF>
#include <QRectF>
#include <QPainter>
#include <QColor>
#include <QFont>
#include <QStringList>
#include <QTimer>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QFile>
#include <QFocusEvent>
#include <QScreen>
#include <QShortcut>
#include <QRegularExpression>
#include <QDirIterator>
#include <QMessageBox>
#include <QHash>
#include <QFileInfo>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDateTime>
#include <QCheckBox>
#include <QProgressBar>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QPointer>
#include <QSet>
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTreeWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QStyle>
#include <QPainter>
#include <QPalette>
#include <QStringView>
#include <QImageReader>

#include <QScrollBar>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtSVG/QSvgRenderer>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Widgets.ProjectManagerWidget;

import std;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.SoftwareRenderInspectors;
import FloatColorPickerDialog;
import Widgets.Utils.CSS;


import Utils.String.UniString;
import Utils.Id;
import Artifact.Project.Manager;
import Artifact.Service.Project;
import Artifact.Application.ProjectBundleIpc;
import Artifact.Service.Playback;
import Artifact.Project.Model;
import Artifact.Project.Items;
import Artifact.Project.Roles;
import Artifact.Project.Cleanup;
import Input.Operator;
import Artifact.Composition.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Composition;
import Artifact.Layer.Search.Query;
import Artifact.Event.Types;
import Event.Bus;
import Artifact.Layer.InitParams;
import Clipboard.ClipboardManager;
import Composition.ParametricComposition;
import Artifact.Widgets.LayerPanelWidget;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Menu.Layer;
import Artifact.Widgets.AppDialogs;
import Dialog.Composition;
import Geometry.ResolutionRemap;
import Artifact.Widgets.ResolutionRemapDialog;
import Utils.Path;
import Undo.UndoManager;
import UI.ShortcutBindings;

namespace Artifact {

using namespace ArtifactCore;

// Forward declarations
struct FootageImpactRow;
QVector<FootageImpactRow> assessFootageFrameRateChange(
    ArtifactProjectService* svc, const QString& footagePath);

// ---------------------------------------------------------------------------
// Footage Interpret — pre-flight assessment helper
// ---------------------------------------------------------------------------
struct FootageImpactRow {
    QString compositionName;
    QString layerName;
    bool hasTimeRemap = false;
    bool hasKeyframes = false;
};

QVector<FootageImpactRow> assessFootageFrameRateChange(
    ArtifactProjectService* svc, const QString& footagePath)
{
    QVector<FootageImpactRow> result;
    if (!svc) return result;
    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) return result;

    std::function<void(ProjectItem*)> walkItems;
    walkItems = [&](ProjectItem* item) {
        if (!item) return;
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto compResult = project->findComposition(compItem->compositionId);
            auto comp = compResult.ptr.lock();
            if (!comp) return;
            const auto& layers = comp->allLayerRef();
            for (const auto& layerPtr : layers) {
                if (!layerPtr) continue;
                auto* videoLayer = dynamic_cast<ArtifactVideoLayer*>(layerPtr.get());
                if (!videoLayer) continue;
                if (videoLayer->sourceFile() != footagePath) continue;

                FootageImpactRow row;
                row.compositionName = compItem->name.toQString();
                row.layerName = layerPtr->layerName();
                row.hasTimeRemap = layerPtr->isTimeRemapEnabled();
                row.hasKeyframes = layerPtr->isTimeRemapEnabled();
                result.append(row);
            }
        }
        for (auto* child : item->children) {
            walkItems(child);
        }
    };

    const auto roots = project->projectItems();
    for (auto* root : roots) walkItems(root);
    return result;
}

namespace {
constexpr auto kProjectContext = "Workspace.Project";
}

namespace {

class ProjectActionLabel final : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton && activated) {
            activated();
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }
};

void updateCompositionColorButtonPreview(QPushButton* button, const QColor& color)
{
    if (!button) {
        return;
    }
    QPixmap pix(button->size().isEmpty() ? QSize(40, 24) : button->size());
    pix.fill(Qt::transparent);
    {
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(85, 85, 85), 1));
        painter.setBrush(color);
        painter.drawRoundedRect(pix.rect().adjusted(1, 1, -2, -2), 3, 3);
    }
    button->setIcon(QIcon(pix));
    button->setIconSize(pix.size());
    button->setToolTip(QStringLiteral("Background Color: %1").arg(color.name(QColor::HexArgb)));
    button->setText(QString());
}

class CompositionBackgroundColorButton final : public QPushButton
{
public:
    std::function<void(const QColor&)> previewChanged;

    explicit CompositionBackgroundColorButton(const QColor& initialColor,
                                              QWidget* parent = nullptr)
        : QPushButton(parent), color_(initialColor)
    {
        updateCompositionColorButtonPreview(this, color_);
    }

    QColor selectedColor() const
    {
        return color_;
    }

    void setSelectedColor(const QColor& color)
    {
        if (!color.isValid()) {
            return;
        }
        color_ = color;
        updateCompositionColorButtonPreview(this, color_);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton) {
            const QColor originalColor = color_;
            ArtifactWidgets::FloatColorPicker picker(this);
            picker.setWindowTitle(QStringLiteral("Background Color"));
            picker.setInitialColor(ArtifactCore::FloatColor(
                color_.redF(), color_.greenF(), color_.blueF(), color_.alphaF()));
            picker.setColor(ArtifactCore::FloatColor(
                color_.redF(), color_.greenF(), color_.blueF(), color_.alphaF()));
            QObject::connect(&picker, &ArtifactWidgets::FloatColorPicker::colorChanged,
                             this, [this](const ArtifactCore::FloatColor& picked) {
                const QColor liveColor = QColor::fromRgbF(
                    picked.r(), picked.g(), picked.b(), picked.a());
                setSelectedColor(liveColor);
                if (previewChanged) {
                    previewChanged(liveColor);
                }
            });
            if (picker.exec() == QDialog::Accepted) {
                const ArtifactCore::FloatColor picked = picker.getColor();
                const QColor acceptedColor = QColor::fromRgbF(
                    picked.r(), picked.g(), picked.b(), picked.a());
                setSelectedColor(acceptedColor);
                if (previewChanged) {
                    previewChanged(acceptedColor);
                }
            } else {
                setSelectedColor(originalColor);
                if (previewChanged) {
                    previewChanged(originalColor);
                }
            }
            event->accept();
            return;
        }
        QPushButton::mousePressEvent(event);
    }

private:
    QColor color_;
};

enum class AssetKind { Image, Video, Audio, Font, Other };

AssetKind assetKindFromPath(const QString& path) {
    const QString lower = path.toLower();
    if (lower.endsWith(".png") || lower.endsWith(".jpg") ||
        lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
        lower.endsWith(".gif") || lower.endsWith(".tga") ||
        lower.endsWith(".tiff") || lower.endsWith(".exr")) {
        return AssetKind::Image;
    }
    if (lower.endsWith(".mp4") || lower.endsWith(".mov") ||
        lower.endsWith(".avi") || lower.endsWith(".mkv") ||
        lower.endsWith(".webm") || lower.endsWith(".flv")) {
        return AssetKind::Video;
    }
    if (lower.endsWith(".mp3") || lower.endsWith(".wav") ||
        lower.endsWith(".ogg") || lower.endsWith(".flac") ||
        lower.endsWith(".aac") || lower.endsWith(".m4a")) {
        return AssetKind::Audio;
    }
    if (lower.endsWith(".ttf") || lower.endsWith(".otf") ||
        lower.endsWith(".ttc") || lower.endsWith(".woff") ||
        lower.endsWith(".woff2")) {
        return AssetKind::Font;
    }
    return AssetKind::Other;
}

bool isImportableAssetFile(const QString& path) {
    return assetKindFromPath(path) != AssetKind::Other;
}

QString projectItemFootageKindLabel(const QString& path) {
    switch (assetKindFromPath(path)) {
    case AssetKind::Image:
        return QStringLiteral("Image");
    case AssetKind::Video:
        return QStringLiteral("Video");
    case AssetKind::Audio:
        return QStringLiteral("Audio");
    case AssetKind::Font:
        return QStringLiteral("Font");
    default:
        return QStringLiteral("Footage");
    }
}

int projectItemUsageCount(ProjectItem* item)
{
    if (!item) {
        return 0;
    }

    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        return 0;
    }

    auto project = svc->getCurrentProjectSharedPtr();
    if (!project) {
        return 0;
    }

    const auto normalizePath = [](const QString& path) {
        return QDir::cleanPath(path.trimmed());
    };

    const auto matchesFootagePath = [&](const FootageItem* footage, const QString& candidatePath) {
        if (!footage) {
            return false;
        }
        const QString normalizedCandidate = normalizePath(candidatePath);
        if (normalizedCandidate.isEmpty()) {
            return false;
        }
        if (normalizedCandidate == normalizePath(footage->filePath)) {
            return true;
        }
        for (const QString& sequencePath : footage->sequencePaths) {
            if (normalizedCandidate == normalizePath(sequencePath)) {
                return true;
            }
        }
        return false;
    };

    int usageCount = 0;
    std::function<void(ProjectItem*)> walkItems;
    walkItems = [&](ProjectItem* current) {
        if (!current) {
            return;
        }
        if (current->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(current);
            const auto found = project->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (comp) {
                for (const auto& layer : comp->allLayerRef()) {
                    if (!layer) {
                        continue;
                    }
                    if (item->type() == eProjectItemType::Composition) {
                        auto* compositionLayer = dynamic_cast<ArtifactCompositionLayer*>(layer.get());
                        if (compositionLayer && compositionLayer->sourceCompositionId() ==
                                                    static_cast<CompositionItem*>(item)->compositionId) {
                            ++usageCount;
                        }
                    } else if (item->type() == eProjectItemType::Footage) {
                        auto* footageLayer = dynamic_cast<ArtifactVideoLayer*>(layer.get());
                        if (footageLayer && matchesFootagePath(static_cast<FootageItem*>(item),
                                                               footageLayer->sourceFile())) {
                            ++usageCount;
                        }
                    }
                }
            }
        }
        for (auto* child : current->children) {
            walkItems(child);
        }
    };

    for (auto* root : project->projectItems()) {
        walkItems(root);
    }

    return usageCount;
}
}

QString projectItemTileBadgeText(ProjectItem* item)
{
    if (!item) {
        return QStringLiteral("Item");
    }

    switch (item->type()) {
    case eProjectItemType::Composition:
        return QStringLiteral("Composition");
    case eProjectItemType::Folder:
        return QStringLiteral("Folder");
    case eProjectItemType::Solid:
        return QStringLiteral("Solid");
    case eProjectItemType::Footage: {
        const auto* footage = static_cast<FootageItem*>(item);
        const QFileInfo info(footage ? footage->filePath : QString());
        if (!info.exists()) {
            return QStringLiteral("Missing");
        }
        return projectItemFootageKindLabel(info.filePath());
    }
    default:
        return QStringLiteral("Item");
    }
}

QString responsiveLayoutVariantSummary(const ResponsiveLayoutVariant& variant)
{
    const QString name = variant.displayName.isEmpty() ? variant.variantId
                                                      : variant.displayName;
    const QString sizeLabel = variant.baseSize.isValid()
        ? QStringLiteral("%1x%2").arg(variant.baseSize.width()).arg(variant.baseSize.height())
        : QStringLiteral("custom");
    return QStringLiteral("%1 • %2").arg(name, sizeLabel);
}

QString responsiveLayoutActiveSummary(const ResponsiveLayoutSet& layout)
{
    const QString activeVariantId = layout.activeVariantId;
    for (const auto& variant : layout.variants) {
        if (variant.variantId == activeVariantId) {
            return responsiveLayoutVariantSummary(variant);
        }
    }
    return QStringLiteral("Manual");
}

QString uniqueResponsiveVariantId(const ResponsiveLayoutSet& layout, const QString& baseId)
{
    QString trimmed = baseId.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = QStringLiteral("layout");
    }
    if (!layout.hasVariant(trimmed)) {
        return trimmed;
    }
    for (int i = 2; i < 1000; ++i) {
        const QString candidate = QStringLiteral("%1_%2").arg(trimmed).arg(i);
        if (!layout.hasVariant(candidate)) {
            return candidate;
        }
    }
    return QStringLiteral("%1_%2").arg(trimmed, QString::number(layout.variants.size() + 1));
}

ResponsiveLayoutVariant responsiveLayoutVariantTemplate(const ResponsiveLayoutVariant* source,
                                                        const QSize& fallbackSize)
{
    ResponsiveLayoutVariant variant;
    if (source) {
        variant = *source;
    } else {
        variant.variantId = QStringLiteral("layout");
        variant.displayName = QStringLiteral("Layout");
        variant.baseSize = fallbackSize.isValid() ? fallbackSize : QSize(1920, 1080);
        variant.aspectRatio = variant.baseSize.height() > 0
            ? static_cast<qreal>(variant.baseSize.width()) /
              static_cast<qreal>(variant.baseSize.height())
            : 0.0;
        variant.safeArea = QRectF(0.0, 0.0, 1.0, 1.0);
        variant.contentAnchor = QPointF(0.5, 0.5);
        variant.layoutRules.insert(QStringLiteral("scaleMode"), QStringLiteral("fit"));
        variant.layoutRules.insert(QStringLiteral("cropMode"), QStringLiteral("none"));
        variant.enabled = true;
    }
    return variant;
}

bool editResponsiveLayoutVariantDialog(QWidget* parent,
                                      ResponsiveLayoutSet* layoutSet,
                                      const QString& variantId)
{
    if (!layoutSet) {
        return false;
    }
    auto* variant = [&]() -> ResponsiveLayoutVariant* {
        for (auto& candidate : layoutSet->variants) {
            if (candidate.variantId == variantId) {
                return &candidate;
            }
        }
        return nullptr;
    }();
    if (!variant) {
        return false;
    }
    const QString originalVariantId = variant->variantId;

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Edit Responsive Variant"));
    dialog.setModal(true);
    dialog.resize(420, 220);

    auto* dialogLayout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(variant->displayName.isEmpty() ? variant->variantId : variant->displayName);
    form->addRow(QStringLiteral("Name"), nameEdit);

    auto* idEdit = new QLineEdit(&dialog);
    idEdit->setText(variant->variantId);
    form->addRow(QStringLiteral("Variant ID"), idEdit);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 32768);
    widthSpin->setValue(std::max(1, variant->baseSize.width()));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 32768);
    heightSpin->setValue(std::max(1, variant->baseSize.height()));
    form->addRow(QStringLiteral("Width"), widthSpin);
    form->addRow(QStringLiteral("Height"), heightSpin);

    auto* enabledCheck = new QCheckBox(QStringLiteral("Enabled"), &dialog);
    enabledCheck->setChecked(variant->enabled);
    form->addRow(QString(), enabledCheck);

    dialogLayout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal,
                                         &dialog);
    dialogLayout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString trimmedId = idEdit->text().trimmed();
        if (trimmedId.isEmpty()) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("Edit Responsive Variant"),
                                 QStringLiteral("Variant ID must not be empty."));
            return;
        }

        for (const auto& candidate : layoutSet->variants) {
            if (&candidate != variant && candidate.variantId == trimmedId) {
                QMessageBox::warning(&dialog,
                                     QStringLiteral("Edit Responsive Variant"),
                                     QStringLiteral("Variant ID must be unique."));
                return;
            }
        }

        const QString trimmedName = nameEdit->text().trimmed();
        if (trimmedName.isEmpty()) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("Edit Responsive Variant"),
                                 QStringLiteral("Name must not be empty."));
            return;
        }

        const bool wasActive = (layoutSet->activeVariantId == originalVariantId);
        const QString newVariantId = trimmedId;
        variant->variantId = newVariantId;
        variant->displayName = trimmedName;
        variant->baseSize = QSize(std::max(1, widthSpin->value()),
                                  std::max(1, heightSpin->value()));
        variant->aspectRatio = variant->baseSize.height() > 0
            ? static_cast<qreal>(variant->baseSize.width()) /
              static_cast<qreal>(variant->baseSize.height())
            : 0.0;
        variant->enabled = enabledCheck->isChecked();
        if (wasActive) {
            layoutSet->activeVariantId = newVariantId;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

bool addResponsiveLayoutVariantDialog(QWidget* parent,
                                     ResponsiveLayoutSet* layoutSet,
                                     const ResponsiveLayoutVariant* templateVariant,
                                     const QSize& fallbackSize,
                                     const QString& title,
                                     const bool activateNewVariant)
{
    if (!layoutSet) {
        return false;
    }

    ResponsiveLayoutVariant draft = responsiveLayoutVariantTemplate(templateVariant, fallbackSize);
    draft.variantId = uniqueResponsiveVariantId(*layoutSet, draft.variantId);
    if (draft.displayName.trimmed().isEmpty()) {
        draft.displayName = QStringLiteral("Layout");
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(420, 220);

    auto* dialogLayout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(draft.displayName);
    form->addRow(QStringLiteral("Name"), nameEdit);

    auto* idEdit = new QLineEdit(&dialog);
    idEdit->setText(draft.variantId);
    form->addRow(QStringLiteral("Variant ID"), idEdit);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(1, 32768);
    widthSpin->setValue(std::max(1, draft.baseSize.width()));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(1, 32768);
    heightSpin->setValue(std::max(1, draft.baseSize.height()));
    form->addRow(QStringLiteral("Width"), widthSpin);
    form->addRow(QStringLiteral("Height"), heightSpin);

    auto* enabledCheck = new QCheckBox(QStringLiteral("Enabled"), &dialog);
    enabledCheck->setChecked(draft.enabled);
    form->addRow(QString(), enabledCheck);

    dialogLayout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal,
                                         &dialog);
    dialogLayout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString trimmedId = idEdit->text().trimmed();
        if (trimmedId.isEmpty()) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("Responsive Layout"),
                                 QStringLiteral("Variant ID must not be empty."));
            return;
        }
        for (const auto& candidate : layoutSet->variants) {
            if (candidate.variantId == trimmedId) {
                QMessageBox::warning(&dialog,
                                     QStringLiteral("Responsive Layout"),
                                     QStringLiteral("Variant ID must be unique."));
                return;
            }
        }

        const QString trimmedName = nameEdit->text().trimmed();
        if (trimmedName.isEmpty()) {
            QMessageBox::warning(&dialog,
                                 QStringLiteral("Responsive Layout"),
                                 QStringLiteral("Name must not be empty."));
            return;
        }

        draft.variantId = trimmedId;
        draft.displayName = trimmedName;
        draft.baseSize = QSize(std::max(1, widthSpin->value()),
                               std::max(1, heightSpin->value()));
        draft.aspectRatio = draft.baseSize.height() > 0
            ? static_cast<qreal>(draft.baseSize.width()) /
              static_cast<qreal>(draft.baseSize.height())
            : 0.0;
        draft.enabled = enabledCheck->isChecked();
        layoutSet->variants.append(draft);
        if (activateNewVariant) {
            layoutSet->activeVariantId = draft.variantId;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

QString projectItemTypeLabel(eProjectItemType type)
{
    switch (type) {
    case eProjectItemType::Folder:
        return QStringLiteral("Folder");
    case eProjectItemType::Composition:
        return QStringLiteral("Composition");
    case eProjectItemType::Footage:
        return QStringLiteral("Footage");
    case eProjectItemType::Solid:
        return QStringLiteral("Solid");
    default:
        return QStringLiteral("Item");
    }
}

bool isDescendantOf(const ProjectItem* item, const ProjectItem* ancestor)
{
    if (!item || !ancestor) {
        return false;
    }
    for (const ProjectItem* current = item->parent; current; current = current->parent) {
        if (current == ancestor) {
            return true;
        }
    }
    return false;
}

QString folderDisplayPath(FolderItem* folder)
{
    if (!folder) {
        return {};
    }
    QStringList parts;
    for (ProjectItem* current = folder; current; current = current->parent) {
        if (!current->name.toQString().isEmpty()) {
            parts.prepend(current->name.toQString());
        }
    }
    return parts.join(QStringLiteral("/"));
}

void collectFolders(ProjectItem* item, QVector<FolderItem*>& out)
{
    if (!item) {
        return;
    }
    if (item->type() == eProjectItemType::Folder) {
        out.append(static_cast<FolderItem*>(item));
    }
    for (auto* child : item->children) {
        collectFolders(child, out);
    }
}

void collectImportablePaths(const QString& input, QStringList& out)
{
    auto addPath = [&out](const QString& path) {
        const QFileInfo info(path.trimmed());
        if (!info.exists()) {
            return;
        }
        if (info.isDir()) {
            QDirIterator it(info.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                out.append(it.next());
            }
            return;
        }
        if (isImportableAssetFile(info.absoluteFilePath())) {
            out.append(info.absoluteFilePath());
        }
    };

    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (trimmed.contains('\n') || trimmed.contains('\r') || trimmed.contains(';')) {
        const QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("[\\r\\n;]+")),
                                                Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            addPath(part);
        }
        return;
    }
    addPath(trimmed);
}

enum class ProjectProxyQuality { Quarter, Half, Full };
struct ProxyMeta {
    ProjectProxyQuality quality = ProjectProxyQuality::Half;
    bool enabled = true;
    QDateTime sourceLastModified;
    QString qualityLabel;
};

QHash<QString, ProxyMeta>& proxyMetadata() {
    static QHash<QString, ProxyMeta> meta;
    return meta;
}

QString proxyFilePathForFootage(const QString& sourceFilePath)
{
    const QFileInfo src(sourceFilePath);
    if (src.filePath().isEmpty() || src.completeBaseName().isEmpty()) {
        return {};
    }
    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataRoot.isEmpty()) {
        return {};
    }
    QDir proxyDir(appDataRoot);
    const QString proxyRoot = proxyDir.filePath(QStringLiteral("ProxyCache"));
    const QByteArray digest = QCryptographicHash::hash(src.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString proxyName = QStringLiteral("%1_%2.proxy.jpg")
                                  .arg(src.completeBaseName(),
                                       QString::fromLatin1(digest.left(10)));
    return QDir(proxyRoot).filePath(proxyName);
}

QString projectItemStatusChipText(ProjectItem* item)
{
    if (!item) {
        return QStringLiteral("Unknown");
    }

    switch (item->type()) {
    case eProjectItemType::Footage: {
        const auto* footage = static_cast<FootageItem*>(item);
        const QFileInfo info(footage ? footage->filePath : QString());
        if (!info.exists()) {
            return QStringLiteral("Missing");
        }
        const QString proxyPath = proxyFilePathForFootage(footage->filePath);
        if (!proxyPath.isEmpty() && QFileInfo(proxyPath).exists()) {
            const auto it = proxyMetadata().constFind(footage->filePath);
            if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                const QFileInfo src(footage->filePath);
                if (src.exists() && src.lastModified() > it->sourceLastModified) {
                    return QStringLiteral("Proxy Stale");
                }
            }
            return QStringLiteral("Proxy Ready");
        }
        return QStringLiteral("Ready");
    }
    case eProjectItemType::Composition:
        return QStringLiteral("Live");
    case eProjectItemType::Folder:
        return QStringLiteral("Container");
    case eProjectItemType::Solid:
        return QStringLiteral("Static");
    default:
        return QStringLiteral("Item");
    }
}

QColor projectItemStatusChipColor(const QString& statusText)
{
    if (statusText.contains(QStringLiteral("Missing"), Qt::CaseInsensitive)) {
        return QColor(215, 84, 84);
    }
    if (statusText.contains(QStringLiteral("Stale"), Qt::CaseInsensitive)) {
        return QColor(218, 166, 72);
    }
    if (statusText.contains(QStringLiteral("Proxy Ready"), Qt::CaseInsensitive)) {
        return QColor(94, 178, 126);
    }
    if (statusText.contains(QStringLiteral("Ready"), Qt::CaseInsensitive) ||
        statusText.contains(QStringLiteral("Live"), Qt::CaseInsensitive)) {
        return QColor(88, 148, 205);
    }
    if (statusText.contains(QStringLiteral("Container"), Qt::CaseInsensitive)) {
        return QColor(188, 151, 73);
    }
    return QColor(150, 160, 174);
}

void syncProxyPathToProject(const QString& sourceFilePath, const QString& proxyPath,
                            bool enabled, bool globalEnabled)
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    auto project = service->getCurrentProjectSharedPtr();
    if (!project) {
        return;
    }
    const QString targetPath = QFileInfo(sourceFilePath).absoluteFilePath();
    if (targetPath.isEmpty() || proxyPath.isEmpty()) {
        return;
    }
    const bool shouldUseProxy = globalEnabled && enabled;
    const auto roots = project->projectItems();
    std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
        if (!item) {
            return;
        }
        if (item->type() == eProjectItemType::Composition) {
            auto* compItem = static_cast<CompositionItem*>(item);
            auto found = service->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (!found.success || !comp) {
                return;
            }
            const auto layers = comp->allLayer();
            for (const auto& layer : layers) {
                auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
                if (!videoLayer) {
                    continue;
                }
                const QString layerSourcePath = videoLayer->sourcePath().trimmed();
                if (layerSourcePath.isEmpty() || QFileInfo(layerSourcePath).absoluteFilePath() != targetPath) {
                    continue;
                }
                if (shouldUseProxy) {
                    if (videoLayer->proxyPath() == proxyPath) continue;
                    videoLayer->setProxyPath(proxyPath);
                } else {
                    videoLayer->clearProxy();
                }
                videoLayer->changed();
                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                                      LayerChangedEvent::ChangeType::Modified});
            }
        }
        for (auto* child : item->children) {
            visit(child);
        }
    };
    for (auto* root : roots) {
        visit(root);
    }
}

QPixmap projectItemPreviewPixmap(ProjectItem* item, const QSize& targetSize)
{
    if (!item) {
        return {};
    }

    const auto isImageFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".png") || lowerPath.endsWith(".jpg") ||
               lowerPath.endsWith(".jpeg") || lowerPath.endsWith(".bmp") ||
               lowerPath.endsWith(".gif") || lowerPath.endsWith(".tga") ||
               lowerPath.endsWith(".tiff") || lowerPath.endsWith(".exr");
    };
    const auto isVideoFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".mp4") || lowerPath.endsWith(".mov") ||
               lowerPath.endsWith(".avi") || lowerPath.endsWith(".mkv") ||
               lowerPath.endsWith(".webm");
    };

    if (item->type() == eProjectItemType::Footage) {
        const QString path = static_cast<FootageItem*>(item)->filePath;
        const QFileInfo info(path);
        if (!info.exists()) {
            return {};
        }

        QString lowerPath = path.toLower();
        if (isImageFile(lowerPath)) {
            QPixmap pix(path);
            if (pix.isNull()) {
                return {};
            }
            return pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        if (isVideoFile(lowerPath)) {
            QPixmap pix(targetSize);
            pix.fill(Qt::transparent);
            QPainter painter(&pix);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(QColor(90, 90, 90), 1.0));
            painter.setBrush(QColor(66, 148, 98));
            painter.drawRoundedRect(QRectF(1.0, 1.0, targetSize.width() - 2.0, targetSize.height() - 2.0), 3.0, 3.0);
            painter.setPen(Qt::white);
            painter.drawText(pix.rect(), Qt::AlignCenter, QStringLiteral("V"));
            return pix;
        }

        return {};
    }

    if (item->type() == eProjectItemType::Composition) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return {};
        }
        const auto found = svc->findComposition(static_cast<CompositionItem*>(item)->compositionId);
        if (!found.success) {
            return {};
        }
        if (auto composition = found.ptr.lock()) {
           const QImage thumb = generateCompositionThumbnail(composition, targetSize);
            if (!thumb.isNull()) {
                return QPixmap::fromImage(thumb);
            }
        }
    }

    return {};
}

QStringList projectItemMetadataLines(const QModelIndex& sourceIndex, ProjectItem* item)
{
    if (!item) {
        return {};
    }
    Q_UNUSED(sourceIndex);
    QStringList lines;
    const auto isImageFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".png") || lowerPath.endsWith(".jpg") ||
               lowerPath.endsWith(".jpeg") || lowerPath.endsWith(".bmp") ||
               lowerPath.endsWith(".gif") || lowerPath.endsWith(".tga") ||
               lowerPath.endsWith(".tiff") || lowerPath.endsWith(".exr");
    };
    const auto isVideoFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".mp4") || lowerPath.endsWith(".mov") ||
               lowerPath.endsWith(".avi") || lowerPath.endsWith(".mkv") ||
               lowerPath.endsWith(".webm");
    };
    const auto isAudioFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".wav") || lowerPath.endsWith(".mp3") ||
               lowerPath.endsWith(".flac") || lowerPath.endsWith(".ogg") ||
               lowerPath.endsWith(".m4a") || lowerPath.endsWith(".aac");
    };
    const auto isFontFile = [](const QString& lowerPath) {
        return lowerPath.endsWith(".ttf") || lowerPath.endsWith(".otf") ||
               lowerPath.endsWith(".ttc") || lowerPath.endsWith(".woff") ||
               lowerPath.endsWith(".woff2");
    };

    // Composition metadata
    if (item->type() == eProjectItemType::Composition) {
        auto* composition = static_cast<CompositionItem*>(item);
        const QString itemName = composition->name.toQString().trimmed();
        lines << QStringLiteral("Type: Composition");
        lines << QStringLiteral("Name: %1").arg(itemName.isEmpty()
                                                    ? QStringLiteral("Composition")
                                                    : itemName);
        if (auto* svc = ArtifactProjectService::instance()) {
            const auto found = svc->findComposition(composition->compositionId);
            if (auto comp = found.ptr.lock()) {
                const QSize compSize = comp->settings().compositionSize();
                const auto frameRange = comp->frameRange().normalized();
                const auto workAreaRange = comp->workAreaRange().normalized();
                const ResponsiveLayoutSet responsiveLayout = comp->responsiveLayout();
                lines << QStringLiteral("Status: Ready");
                lines << QStringLiteral("Resolution: %1 x %2")
                             .arg(compSize.width())
                             .arg(compSize.height());
                lines << QStringLiteral("Layout: %1 • %2 variants")
                             .arg(responsiveLayoutActiveSummary(responsiveLayout))
                             .arg(responsiveLayout.variants.size());
                lines << QStringLiteral("Timing: %1 fps • %2 frames")
                             .arg(QString::number(comp->frameRate().framerate(), 'f', 2))
                             .arg(frameRange.duration());
                lines << QStringLiteral("Work Area: %1 frames • %2 layers")
                             .arg(workAreaRange.duration())
                            .arg(comp->allLayer().size());
                const QColor bgColor = QColor::fromRgbF(
                    comp->backgroundColor().r(),
                    comp->backgroundColor().g(),
                    comp->backgroundColor().b(),
                    comp->backgroundColor().a());
                lines << QStringLiteral("Background: %1")
                             .arg(bgColor.name(QColor::HexArgb).toUpper());
                lines << QStringLiteral("Composition ID: %1")
                             .arg(composition->compositionId.toString());
                lines << QStringLiteral("Used In: %1")
                             .arg(projectItemUsageCount(item));
            } else {
                lines << QStringLiteral("Status: Composition data unavailable");
            }
        }
    }

    // Footage metadata
    if (item->type() == eProjectItemType::Footage) {
        auto* footage = static_cast<FootageItem*>(item);
        const QString path = footage->filePath;
        QFileInfo info(path);
        const bool exists = info.exists();
        lines << QStringLiteral("Type: %1").arg(projectItemFootageKindLabel(path));
        if (exists) {
            lines << QStringLiteral("Status: Ready");
            lines << QStringLiteral("File Size: %1 KB").arg(info.size() / 1024);
            lines << QStringLiteral("Modified: %1").arg(info.lastModified().toString("yyyy-MM-dd hh:mm"));
        } else {
            lines << QStringLiteral("Status: Missing");
        }
        lines << QStringLiteral("Used In: %1")
                     .arg(projectItemUsageCount(item));

        QString lowerPath = path.toLower();
        if (exists) {
            if (isImageFile(lowerPath)) {
                QImageReader imageReader(path);
                const QSize imageSize = imageReader.size();
                if (imageSize.isValid()) {
                    lines << QStringLiteral("Resolution: %1 x %2").arg(imageSize.width()).arg(imageSize.height());
                    const QByteArray format = imageReader.format();
                    if (!format.isEmpty()) {
                        lines << QStringLiteral("Format: %1").arg(QString::fromLatin1(format).toUpper());
                    }
                }
            } else if (isVideoFile(lowerPath)) {
                lines << QStringLiteral("Kind: Video");
                lines << QStringLiteral("Preview: Generated thumbnail");
            } else if (isAudioFile(lowerPath)) {
                lines << QStringLiteral("Kind: Audio");
            } else if (isFontFile(lowerPath)) {
                lines << QStringLiteral("Kind: Font");
            }
        }
    }

    // Folder metadata
    if (item->type() == eProjectItemType::Folder) {
        int childCount = 0;
        if (!item->children.isEmpty()) {
            childCount = item->children.size();
        }
        lines << QStringLiteral("Type: Folder");
        lines << QStringLiteral("Contents: %1 items").arg(childCount);
    }

    // Solid metadata
    if (item->type() == eProjectItemType::Solid) {
        const auto* solid = static_cast<const SolidItem*>(item);
        if (solid) {
            lines << QStringLiteral("Type: Solid");
            lines << QStringLiteral("Color: %1")
                          .arg(solid->color.name(QColor::HexArgb).toUpper());
        }
    }

    if (item->type() == eProjectItemType::Composition) {
        auto* composition = static_cast<CompositionItem*>(item);
        if (composition) {
            if (auto* svc = ArtifactProjectService::instance()) {
                const auto found = svc->findComposition(composition->compositionId);
                if (auto comp = found.ptr.lock()) {
                    const QSize compSize = comp->settings().compositionSize();
                    const auto frameRange = comp->frameRange().normalized();
                    lines << QStringLiteral("Size: %1x%2")
                                  .arg(compSize.width())
                                  .arg(compSize.height());
                    lines << QStringLiteral("Timing: %1 fps • %2 frames")
                                  .arg(QString::number(comp->frameRate().framerate(), 'f', 2))
                                  .arg(frameRange.duration());
                }
            }
        }
    }

    return lines;
}

QIcon loadSvgAsQIcon(const QString& path, int size = 16)
{
    if (path.isEmpty()) return QIcon();
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            if (!pixmap.isNull()) return QIcon(pixmap);
        }
        return QIcon();
    }
    return QIcon(path);
}

QIcon loadProjectViewIcon(const QString& resourceRelativePath, const QString& fallbackFileName = {})
{
    QIcon icon = loadSvgAsQIcon(ArtifactCore::resolveIconResourcePath(resourceRelativePath));
    if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
        return icon;
    }
    if (!fallbackFileName.isEmpty()) {
        icon = loadSvgAsQIcon(ArtifactCore::resolveIconPath(fallbackFileName));
        if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
            return icon;
        }
    }
    if (auto* appStyle = QApplication::style()) {
        return appStyle->standardIcon(QStyle::SP_FileIcon);
    }
    return icon;
}

QString projectViewIllustrationPath(const QString& relativePath)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appRelative = QDir(appDir).filePath(QStringLiteral("Illustration/%1").arg(relativePath));
    if (QFileInfo::exists(appRelative)) {
        return appRelative;
    }
    const QString workspaceRelative = QDir(QDir::currentPath()).filePath(QStringLiteral("Artifact/App/Illustration/%1").arg(relativePath));
    if (QFileInfo::exists(workspaceRelative)) {
        return workspaceRelative;
    }
    const QString sourceRelative = QDir(appDir).filePath(QStringLiteral("../Artifact/App/Illustration/%1").arg(relativePath));
    if (QFileInfo::exists(sourceRelative)) {
        return QDir::cleanPath(sourceRelative);
    }
    return {};
}

QPixmap projectViewIllustration(const QString& relativePath, const QSize& targetSize)
{
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1|%2x%3")
                            .arg(relativePath)
                            .arg(targetSize.width())
                            .arg(targetSize.height());
    if (auto it = cache.constFind(key); it != cache.constEnd()) {
        return *it;
    }
    const QString path = projectViewIllustrationPath(relativePath);
    if (path.isEmpty()) {
        return {};
    }
    QPixmap pix(path);
    if (pix.isNull()) {
        return {};
    }
    pix = pix.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    cache.insert(key, pix);
    return pix;
}

void drawProjectViewEmptyState(QPainter& painter, const QRect& contentRect)
{
    if (contentRect.width() < 120 || contentRect.height() < 120) {
        return;
    }

    const int imageSide = std::clamp(std::min(contentRect.width(), contentRect.height()) / 3, 112, 220);
    const QPixmap illustration = projectViewIllustration(
        QStringLiteral("Studio/project_view_empty.png"),
        QSize(imageSide, imageSide));

    const QPoint center = contentRect.center();
    int y = center.y() - imageSide / 2 - 22;
    if (!illustration.isNull()) {
        const QRect imageRect(center.x() - illustration.width() / 2,
                              y,
                              illustration.width(),
                              illustration.height());
        painter.save();
        painter.setOpacity(0.72);
        painter.drawPixmap(imageRect.topLeft(), illustration);
        painter.restore();
        y = imageRect.bottom() + 12;
    }

    QFont titleFont = painter.font();
    titleFont.setPointSize(std::max(10, titleFont.pointSize() + 1));
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(218, 224, 232, 210));
    const QRect titleRect(contentRect.left() + 28, y, contentRect.width() - 56, 24);
    painter.drawText(titleRect, Qt::AlignCenter, QStringLiteral("No project items yet"));

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(std::max(8, bodyFont.pointSize() - 1));
    painter.setFont(bodyFont);
    painter.setPen(QColor(160, 170, 182, 185));
    const QRect bodyRect(contentRect.left() + 28, titleRect.bottom() + 4, contentRect.width() - 56, 22);
    painter.drawText(bodyRect, Qt::AlignCenter, QStringLiteral("Import assets or create a composition to start."));
}

constexpr int kHeaderResizeHitRadius = 7;
constexpr int kHeaderGripWidth = 9;
constexpr int kHeaderGripHeight = 11;

struct HeaderResizeHit {
    int column = -1;
    int boundaryX = 0;
};

HeaderResizeHit headerResizeHit(const QVector<int>& columnWidths, const QPoint& mousePos, const int headerHeight)
{
    if (mousePos.y() < 0 || mousePos.y() >= headerHeight) {
        return {};
    }

    int x = 0;
    for (int i = 0; i < columnWidths.size(); ++i) {
        const int width = columnWidths[i];
        const int left = x;
        const int right = x + width;
        if (std::abs(mousePos.x() - right) <= kHeaderResizeHitRadius) {
            return {i, right};
        }
        if (i > 0 && std::abs(mousePos.x() - left) <= kHeaderResizeHitRadius) {
            return {i - 1, left};
        }
        x += width;
    }
    return {};
}

} // namespace

namespace Artifact {
 W_OBJECT_IMPL(Artifact::ArtifactProjectManagerWidget)
 W_OBJECT_IMPL(Artifact::ArtifactProjectView)
 W_OBJECT_IMPL(Artifact::ArtifactProjectManagerToolBox)

// --- Preview/Info Panel at top ---
class ProjectInfoPanel : public QWidget {
public:
    QLabel* thumbnail;
    QLabel* titleLabel;
    QLabel* detailsLabel;
    QHash<QString, QPixmap> previewCache;

    ProjectInfoPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("projectInfoPanel"));
        setAutoFillBackground(true);
        setFixedHeight(96);
        const QColor background = QColor(ArtifactCore::currentDCCTheme().backgroundColor);
        const QColor surface = QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor);
        const QColor text = QColor(ArtifactCore::currentDCCTheme().textColor);
        const QColor muted = text.darker(130);
        const QColor border = QColor(ArtifactCore::currentDCCTheme().borderColor);
        QPalette widgetPalette = palette();
        widgetPalette.setColor(QPalette::Window, background);
        widgetPalette.setColor(QPalette::WindowText, text);
        setPalette(widgetPalette);
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 6, 10, 6);
        layout->setSpacing(10);

        thumbnail = new QLabel();
        thumbnail->setFixedSize(150, 84);
        thumbnail->setAlignment(Qt::AlignCenter);
        thumbnail->setText("PREVIEW");
        thumbnail->setAutoFillBackground(true);
        {
            QPalette pal = thumbnail->palette();
            pal.setColor(QPalette::Window, surface);
            pal.setColor(QPalette::WindowText, muted);
            pal.setColor(QPalette::Base, surface);
            pal.setColor(QPalette::Mid, border);
            thumbnail->setPalette(pal);
        }

        auto infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(1);
        infoLayout->setContentsMargins(0, 2, 0, 2);

        titleLabel = new QLabel("Project");
        {
            QFont f = titleLabel->font();
            f.setBold(true);
            f.setPointSize(13);
            titleLabel->setFont(f);
            QPalette pal = titleLabel->palette();
            pal.setColor(QPalette::WindowText, text);
            titleLabel->setPalette(pal);
        }

        detailsLabel = new QLabel("Select an item to inspect details");
        detailsLabel->setWordWrap(false);
        detailsLabel->setMinimumHeight(52);
        {
            QPalette pal = detailsLabel->palette();
            pal.setColor(QPalette::WindowText, muted);
            detailsLabel->setPalette(pal);
        }

        infoLayout->addWidget(titleLabel);
        infoLayout->addWidget(detailsLabel);
        infoLayout->addStretch();

        layout->addWidget(thumbnail);
        layout->addLayout(infoLayout);
        layout->addStretch();
    }

    void updateInfo(const QModelIndex& index) {
        if (!index.isValid()) {
            titleLabel->setText("Project");
            detailsLabel->setText("Open a project or search to inspect details");
            thumbnail->setText("PREVIEW");
            thumbnail->setPixmap(QPixmap());
            return;
        }
        const QModelIndex source0 = index.siblingAtColumn(0);
        QString name = source0.data(Qt::DisplayRole).toString();
        titleLabel->setText(name);

        // Lazy preview generation: only decode imagery when the selected row needs it.
        thumbnail->setText("PREVIEW");
        thumbnail->setPixmap(QPixmap());
        QVariant ptrVar = source0.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        const QStringList metadata = projectItemMetadataLines(index, item);
        detailsLabel->setText(metadata.mid(1).join(QStringLiteral("\n")));

        if (!item) {
            return;
        }

        const QString cacheKey = item->type() == eProjectItemType::Composition
            ? QStringLiteral("comp:%1").arg(static_cast<CompositionItem*>(item)->compositionId.toString())
            : (item->type() == eProjectItemType::Footage
                ? QStringLiteral("footage:%1").arg(static_cast<FootageItem*>(item)->filePath)
                : QStringLiteral("%1:%2").arg(static_cast<int>(item->type())).arg(name));

        auto cacheIt = previewCache.constFind(cacheKey);
        if (cacheIt != previewCache.constEnd()) {
            thumbnail->setPixmap(*cacheIt);
            thumbnail->setText(QString());
            return;
        }

        const QPixmap pix = projectItemPreviewPixmap(item, thumbnail->size());
        if (!pix.isNull()) {
            previewCache.insert(cacheKey, pix);
            thumbnail->setPixmap(pix);
            thumbnail->setText(QString());
            return;
        }

        if (item->type() == eProjectItemType::Footage &&
            !QFileInfo(static_cast<FootageItem*>(item)->filePath).exists()) {
            thumbnail->setText("MISSING");
            return;
        }

        thumbnail->setText(projectItemTypeLabel(item->type()).toUpper());
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.fillRect(event->rect(), QColor(ArtifactCore::currentDCCTheme().backgroundColor));
        QWidget::paintEvent(event);
    }
};

// --- Hover Popup ---
class HoverThumbnailPopupWidget::Impl {
 public:
  Impl() : thumbnailLabel(nullptr) {}
  QLabel* thumbnailLabel;
  QVector<QLabel*> infoLabels;
  QVBoxLayout* layout;
};

HoverThumbnailPopupWidget::HoverThumbnailPopupWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setAutoFillBackground(false);

  impl_->layout = new QVBoxLayout(this);
  impl_->layout->setContentsMargins(10, 10, 10, 10);
  impl_->layout->setSpacing(6);

  impl_->thumbnailLabel = new QLabel(this);
  impl_->thumbnailLabel->setFixedSize(200, 112);
  impl_->thumbnailLabel->setScaledContents(true);
  {
    QPalette pal = impl_->thumbnailLabel->palette();
    pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    impl_->thumbnailLabel->setAutoFillBackground(true);
    impl_->thumbnailLabel->setPalette(pal);
  }
  impl_->layout->addWidget(impl_->thumbnailLabel, 0, Qt::AlignCenter);

  for (int i = 0; i < 3; ++i) {
    QLabel* l = new QLabel(this);
    {
      QPalette pal = l->palette();
      pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(115));
      l->setPalette(pal);
    }
    impl_->infoLabels.append(l);
    impl_->layout->addWidget(l);
  }
}

void HoverThumbnailPopupWidget::paintEvent(QPaintEvent* event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setBrush(QColor(30, 30, 30, 240));
  painter.setPen(QPen(QColor(ArtifactCore::currentDCCTheme().borderColor)));
  painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
  QWidget::paintEvent(event);
}

HoverThumbnailPopupWidget::~HoverThumbnailPopupWidget() { delete impl_; }
void HoverThumbnailPopupWidget::setThumbnail(const QPixmap& px) { if(impl_->thumbnailLabel) impl_->thumbnailLabel->setPixmap(px); }
void HoverThumbnailPopupWidget::setLabels(const QStringList& ls) {
  for (int i = 0; i < impl_->infoLabels.size(); ++i) {
    impl_->infoLabels[i]->setText(i < ls.size() ? ls[i] : QString());
  }
}
void HoverThumbnailPopupWidget::setLabel(int idx, const QString& t) { if(idx>=0 && idx<impl_->infoLabels.size()) impl_->infoLabels[idx]->setText(t); }
void HoverThumbnailPopupWidget::showAt(const QPoint& p) {
  QPoint pos = p;
  const QSize popupSize = sizeHint().expandedTo(QSize(240, 140));
  if (QScreen* screen = QGuiApplication::screenAt(p)) {
    const QRect avail = screen->availableGeometry();
    if (pos.x() + popupSize.width() > avail.right() - 8) {
      pos.setX(avail.right() - popupSize.width() - 8);
    }
    if (pos.y() + popupSize.height() > avail.bottom() - 8) {
      pos.setY(avail.bottom() - popupSize.height() - 8);
    }
    pos.setX(std::max(avail.left() + 8, pos.x()));
    pos.setY(std::max(avail.top() + 8, pos.y()));
  }
  move(pos);
  show();
  raise();
  QTimer::singleShot(4500, this, &QWidget::hide);
}

class ProjectFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit ProjectFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setSearchQuery(const ArtifactLayerSearchQuery& query) {
        query_ = query;
        invalidateFilter();
    }

    void setUnusedAssetPaths(const QSet<QString>& unusedPaths) {
        unusedAssetPaths_ = unusedPaths;
        invalidateFilter();
    }

    const QSet<QString>& unusedAssetPaths() const {
        return unusedAssetPaths_;
    }

    void setAdvancedFilter(const QString& expression, const QString& typeFilter, const bool unusedOnly) {
        rawExpression_ = expression.trimmed();
        typeFilter_ = typeFilter.trimmed().toLower();
        unusedOnly_ = unusedOnly;
        parseExpression();
        invalidateFilter();
    }

    int visibleRowCount() const {
        return countAcceptedRows(QModelIndex());
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        const QModelIndex rowIdx0 = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!rowIdx0.isValid()) {
            return false;
        }

        const QVariant typeVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        const eProjectItemType itemType = typeVar.isValid()
            ? static_cast<eProjectItemType>(typeVar.toInt())
            : eProjectItemType::Unknown;

        const QVariant ptrVar = sourceModel()->data(
            rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;

        const bool rowMatch = matchesAdvanced(rowIdx0, itemType, item) && matchesLegacyQuery(sourceRow, sourceParent);
        if (rowMatch) {
            return true;
        }

        // Keep parent folders visible if any child matches.
        const int childCount = sourceModel() ? sourceModel()->rowCount(rowIdx0) : 0;
        for (int r = 0; r < childCount; ++r) {
            if (filterAcceptsRow(r, rowIdx0)) {
                return true;
            }
        }
        return false;
    }

private:
    int countAcceptedRows(const QModelIndex& sourceParent) const {
        if (!sourceModel()) {
            return 0;
        }

        int count = 0;
        const int rowCount = sourceModel()->rowCount(sourceParent);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex rowIdx0 = sourceModel()->index(row, 0, sourceParent);
            if (!rowIdx0.isValid()) {
                continue;
            }

            const QVariant typeVar = sourceModel()->data(
                rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            const eProjectItemType itemType = typeVar.isValid()
                ? static_cast<eProjectItemType>(typeVar.toInt())
                : eProjectItemType::Unknown;

            const QVariant ptrVar = sourceModel()->data(
                rowIdx0, Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;

            if (matchesAdvanced(rowIdx0, itemType, item) && matchesLegacyQuery(row, sourceParent)) {
                ++count;
            }
            count += countAcceptedRows(rowIdx0);
        }
        return count;
    }

    bool matchesLegacyQuery(const int sourceRow, const QModelIndex& sourceParent) const {
        if (query_.isSearchTextEmpty()) {
            return true;
        }

        const int cols = sourceModel() ? sourceModel()->columnCount(sourceParent) : 0;
        for (int c = 0; c < cols; ++c) {
            const QModelIndex idx = sourceModel()->index(sourceRow, c, sourceParent);
            const QString text = sourceModel()->data(idx, Qt::DisplayRole).toString();
            if (query_.matches(text.toUtf8().constData(), LayerSearchType::Any, true, false, false)) {
                return true;
            }
        }
        return false;
    }

    void parseExpression() {
        plainTerms_.clear();
        tagTerms_.clear();
        regexPattern_.clear();
        regexEnabled_ = false;

        const QStringList tokens = rawExpression_.split(' ', Qt::SkipEmptyParts);
        for (const QString& token : tokens) {
            if (token.startsWith("tag:", Qt::CaseInsensitive)) {
                const QString value = token.mid(4).trimmed();
                if (!value.isEmpty()) tagTerms_.append(value);
                continue;
            }
            if (token.startsWith("regex:", Qt::CaseInsensitive)) {
                regexPattern_ = token.mid(6).trimmed();
                regexEnabled_ = !regexPattern_.isEmpty();
                continue;
            }
            if (token.startsWith("type:", Qt::CaseInsensitive)) {
                const QString v = token.mid(5).trimmed().toLower();
                if (!v.isEmpty()) typeFilter_ = v;
                continue;
            }
            if (token.compare("unused:true", Qt::CaseInsensitive) == 0 ||
                token.compare("is:unused", Qt::CaseInsensitive) == 0) {
                unusedOnly_ = true;
                continue;
            }
            plainTerms_.append(token);
        }
    }

    bool typeMatches(const eProjectItemType itemType) const {
        if (typeFilter_.isEmpty() || typeFilter_ == "all") {
            return true;
        }
        if (typeFilter_ == "composition") return itemType == eProjectItemType::Composition;
        if (typeFilter_ == "footage") return itemType == eProjectItemType::Footage;
        if (typeFilter_ == "folder") return itemType == eProjectItemType::Folder;
        if (typeFilter_ == "solid") return itemType == eProjectItemType::Solid;
        return true;
    }

    bool matchesAdvanced(const QModelIndex& idx0, const eProjectItemType itemType, ProjectItem* item) const {
        if (!typeMatches(itemType)) return false;

        const QString name = sourceModel()->data(idx0, Qt::DisplayRole).toString();
        QString searchBlob = name;
        if (item && itemType == eProjectItemType::Footage) {
            const QString path = static_cast<FootageItem*>(item)->filePath;
            searchBlob += QStringLiteral(" ") + path;
            if (unusedOnly_ && !unusedAssetPaths_.contains(path)) {
                return false;
            }
        } else if (unusedOnly_) {
            return false;
        }

        for (const QString& term : plainTerms_) {
            if (!searchBlob.contains(term, Qt::CaseInsensitive)) {
                return false;
            }
        }

        for (const QString& tag : tagTerms_) {
            if (!searchBlob.contains(tag, Qt::CaseInsensitive)) {
                return false;
            }
        }

        if (regexEnabled_) {
            const QRegularExpression rx(regexPattern_, QRegularExpression::CaseInsensitiveOption);
            if (!rx.isValid()) return false;
            if (!rx.match(searchBlob).hasMatch()) return false;
        }

        return true;
    }

private:
    ArtifactLayerSearchQuery query_;
    QSet<QString> unusedAssetPaths_;
    QString rawExpression_;
    QString typeFilter_;
    bool unusedOnly_ = false;
    bool regexEnabled_ = false;
    QString regexPattern_;
    QStringList plainTerms_;
    QStringList tagTerms_;
};

bool renameProjectItem(ProjectItem* item, const QString& newName) {
    auto* svc = ArtifactProjectService::instance();
    if (!svc || !item) {
        return false;
    }
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (item->type() == eProjectItemType::Composition) {
        auto* compItem = static_cast<CompositionItem*>(item);
        return svc->renameComposition(compItem->compositionId, UniString::fromQString(trimmed));
    }
    auto shared = svc->getCurrentProjectSharedPtr();
    if (!shared) {
        return false;
    }
    item->name = UniString::fromQString(trimmed);
    shared->projectChanged();
    return true;
}

void scheduleProjectViewRefresh(ArtifactProjectView* view)
{
    if (!view) {
        return;
    }
    static constexpr auto kRefreshQueuedProperty = "artifactProjectViewRefreshQueued";
    if (view->property(kRefreshQueuedProperty).toBool()) {
        return;
    }
    view->setProperty(kRefreshQueuedProperty, true);
    QPointer<ArtifactProjectView> viewPtr(view);
    QTimer::singleShot(0, view, [viewPtr]() {
        if (!viewPtr) {
            return;
        }
        viewPtr->setProperty(kRefreshQueuedProperty, false);
        viewPtr->refreshVisibleContent();
    });
}

// --- Project View (Tree) ---
class ArtifactProjectView::Impl {
public:
    struct VisibleRow {
        QModelIndex index0;
        int depth = 0;
    };

    struct Colors {
        static inline const QColor Background = QColor(0x28, 0x28, 0x28);
        static inline const QColor HeaderBackground = QColor(0x25, 0x25, 0x26);
        static inline const QColor HeaderText = QColor(0x8D, 0x99, 0xA6);
        static inline const QColor HeaderSeparator = QColor(0x3E, 0x3E, 0x42);
        static inline const QColor HeaderHover = QColor(0x2D, 0x2D, 0x30);
        static inline const QColor RowHover = QColor(0x2A, 0x2A, 0x2C);
        static inline const QColor RowSelected = QColor(0x09, 0x47, 0x71);
        static inline const QColor RowSelectedText = QColor(0xF5, 0xF7, 0xFA);
        static inline const QColor RowText = QColor(0xCC, 0xCC, 0xCC);
        static inline const QColor RowBorder = QColor(0x28, 0x28, 0x28);
        static inline const QColor BranchNormal = QColor(0x8D, 0x99, 0xA6);
        static inline const QColor BranchHover = QColor(0xCC, 0xCC, 0xCC);
    };

    QAbstractItemModel* model = nullptr;
    QItemSelectionModel* selectionModel = nullptr;
    QTimer* hoverTimer = nullptr;
    QModelIndex hoverIndex;
    HoverThumbnailPopupWidget* hoverPopup = nullptr;
    QPoint hoverStartPos;
    QPoint dragStartPos;
    QString lastContextCommandId;
    QString lastContextCommandLabel;
    QString lastNewCommandId;
    QString lastNewCommandLabel;
    QVector<int> columnWidths = {260, 120, 120, 100, 140, 180};
    QVector<VisibleRow> visibleRows;
    QSet<QString> expandedKeys;
    QVector<QMetaObject::Connection> modelConnections;
    bool sortingEnabled = false;
    int sortColumn = 0;
    Qt::SortOrder sortOrder = Qt::AscendingOrder;
    int resizingColumn = -1;
    int resizeStartX = 0;
    int hoverHeaderColumn = -1;
    QModelIndex hoverBranchIndex;
    QLineEdit* nameEditor = nullptr;
    QModelIndex editingIndex;
    int headerHeight = 24;
    int rowHeight = 28;
    int indentWidth = 16;
    ArtifactProjectView::PresentationMode presentationMode = ArtifactProjectView::PresentationMode::List;
    QHash<QString, QPixmap> tilePreviewCache;
    int tileMargin = 14;
    int tileSpacing = 16;
    int tileWidth = 214;
    int tileHeight = 226;
    int tilePreviewHeight = 120;
    int tileContentTop = 34;
    int tileContentBottom = 10;
    int tileTextLines = 4;
    int minTileWidth = 176;
    int maxTileWidth = 320;
    int minTileHeight = 192;
    int maxTileHeight = 336;

    QString keyForIndex(QModelIndex index) const {
        index = index.siblingAtColumn(0);
        if (!index.isValid()) {
            return QString();
        }
        const QVariant compId = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
        if (compId.isValid() && !compId.toString().isEmpty()) {
            return QStringLiteral("comp:%1").arg(compId.toString());
        }
        const QVariant assetId = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::AssetId));
        if (assetId.isValid() && !assetId.toString().isEmpty()) {
            return QStringLiteral("asset:%1").arg(assetId.toString());
        }
        const QVariant ptrVar = index.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        if (ptrVar.isValid()) {
            return QStringLiteral("ptr:%1").arg(ptrVar.value<quintptr>());
        }
        QStringList path;
        while (index.isValid()) {
            path.prepend(index.data(Qt::DisplayRole).toString());
            index = index.parent();
        }
        return path.join(QStringLiteral("/"));
    }

    bool isExpanded(const QModelIndex& index) const {
        return expandedKeys.contains(keyForIndex(index));
    }

    void setExpandedState(const QModelIndex& index, const bool expanded) {
        const QString key = keyForIndex(index);
        if (key.isEmpty()) {
            return;
        }
        if (expanded) {
            expandedKeys.insert(key);
        } else {
            expandedKeys.remove(key);
        }
    }

    bool hasChildren(const QModelIndex& index) const {
        return model && model->rowCount(index.siblingAtColumn(0)) > 0;
    }

    int totalColumnWidth() const {
        int total = 0;
        for (const int width : columnWidths) {
            total += width;
        }
        return total;
    }

    bool isTileMode() const {
        return presentationMode == ArtifactProjectView::PresentationMode::Tile;
    }

    int tileColumns(const int viewWidth) const {
        const int usable = std::max(1, viewWidth - tileMargin * 2 + tileSpacing);
        return std::max(1, usable / std::max(1, tileWidth + tileSpacing));
    }

    QRect tileRectForRow(const int row, const int viewWidth) const {
        if (row < 0) {
            return {};
        }
        const int columns = tileColumns(viewWidth);
        const int col = row % columns;
        const int gridRow = row / columns;
        const int x = tileMargin + col * (tileWidth + tileSpacing);
        const int y = tileContentTop + gridRow * (tileHeight + tileSpacing);
        return QRect(x, y, tileWidth, tileHeight);
    }

    int tileRowForPoint(const QPoint& point, const int viewWidth, const int scrollOffsetY) const {
        const QPoint contentPoint(point.x(), point.y() + scrollOffsetY);
        if (contentPoint.x() < tileMargin || contentPoint.y() < tileContentTop) {
            return -1;
        }
        const int columns = tileColumns(viewWidth);
        const int col = (contentPoint.x() - tileMargin) / std::max(1, tileWidth + tileSpacing);
        const int gridRow = (contentPoint.y() - tileContentTop) / std::max(1, tileHeight + tileSpacing);
        if (col < 0 || col >= columns || gridRow < 0) {
            return -1;
        }
        const int row = gridRow * columns + col;
        if (row < 0 || row >= visibleRows.size()) {
            return -1;
        }
        return row;
    }

    QRect tilePreviewRect(const QRect& tileRect) const {
        return QRect(tileRect.left() + 10, tileRect.top() + 10,
                     tileRect.width() - 20, std::max(72, tilePreviewHeight));
    }

    QRect tileTitleRect(const QRect& tileRect) const {
        return QRect(tileRect.left() + 10,
                     tileRect.top() + 10 + std::max(72, tilePreviewHeight) + 6,
                     tileRect.width() - 20, 20);
    }

    QRect tileMetadataRect(const QRect& tileRect) const {
        return QRect(tileRect.left() + 10,
                     tileRect.top() + 10 + std::max(72, tilePreviewHeight) + 28,
                     tileRect.width() - 20,
                     tileRect.height() - (10 + std::max(72, tilePreviewHeight) + 36 + tileContentBottom));
    }

    void setTileDensityStep(const int deltaSteps) {
        if (deltaSteps == 0) {
            return;
        }
        const int step = deltaSteps > 0 ? 12 : -12;
        tileWidth = std::clamp(tileWidth + step, minTileWidth, maxTileWidth);
        tileHeight = std::clamp(tileHeight + step, minTileHeight, maxTileHeight);
        tilePreviewHeight = std::clamp(tilePreviewHeight + step / 2, 88, tileHeight - 76);
        tileTextLines = std::clamp(tileTextLines + (deltaSteps > 0 ? -1 : 1), 2, 5);
    }

    QPixmap previewForIndex(const QModelIndex& index, const QSize& targetSize) {
        QModelIndex sourceIdx = index;
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
            sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
        }
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        if (!item) {
            return {};
        }

        QString itemKey = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::AssetId)).toString();
        if (itemKey.isEmpty()) {
            itemKey = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId)).toString();
        }
        if (itemKey.isEmpty()) {
            itemKey = QStringLiteral("ptr:%1").arg(QString::number(reinterpret_cast<quintptr>(item)));
        }
        const QString cacheKey = QStringLiteral("%1|%2x%3")
                                     .arg(itemKey,
                                          QString::number(targetSize.width()),
                                          QString::number(targetSize.height()));

        if (auto it = tilePreviewCache.constFind(cacheKey); it != tilePreviewCache.constEnd()) {
            return *it;
        }
        const QPixmap pix = projectItemPreviewPixmap(item, targetSize);
        if (!pix.isNull()) {
            tilePreviewCache.insert(cacheKey, pix);
        }
        return pix;
    }

    void rebuildVisibleRows() {
        visibleRows.clear();
        if (!model) {
            return;
        }
        std::function<void(const QModelIndex&, int)> appendRows = [&](const QModelIndex& parent, const int depth) {
            const int childCount = model->rowCount(parent);
            for (int row = 0; row < childCount; ++row) {
                const QModelIndex index0 = model->index(row, 0, parent);
                if (!index0.isValid()) {
                    continue;
                }
                visibleRows.push_back({index0, depth});
                if (hasChildren(index0) && isExpanded(index0)) {
                    appendRows(index0, depth + 1);
                }
            }
        };
        appendRows({}, 0);
    }

    int rowForIndex(const QModelIndex& index) const {
        const QModelIndex index0 = index.siblingAtColumn(0);
        for (int i = 0; i < visibleRows.size(); ++i) {
            if (visibleRows[i].index0 == index0) {
                return i;
            }
        }
        return -1;
    }

    FolderItem* currentFolderTarget(const ArtifactProjectView* view) const {
        if (!view || !view->selectionModel()) {
            return nullptr;
        }
        const auto rows = view->selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            return nullptr;
        }
        QModelIndex sourceIdx = rows.first();
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
            sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
        }
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        if (!item) {
            return nullptr;
        }
        if (item->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item);
        }
        if (item->parent && item->parent->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item->parent);
        }
        return nullptr;
    }

    void createFolderAtSelection(ArtifactProjectView* view) const {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            return;
        }

        bool ok;
        QString name = QInputDialog::getText(view, QStringLiteral("New Folder"),
            QStringLiteral("Folder Name:"), QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
        
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        project->createFolder(UniString::fromQString(name), currentFolderTarget(view));
        // createFolder notifies projectChanged() internally
    }

    void handleFileDrop(const QString& str) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) return;
        QStringList importTargets;
        collectImportablePaths(str, importTargets);
        importTargets.removeDuplicates();
        if (importTargets.isEmpty()) return;
        svc->importAssetsFromPaths(importTargets);
    }

    static void collectFootage(ProjectItem* item, QVector<FootageItem*>& out) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            out.append(static_cast<FootageItem*>(item));
        }
        for (auto* child : item->children) {
            collectFootage(child, out);
        }
    }

    static QString findByFileName(const QString& rootDir, const QString& fileName) {
        if (rootDir.isEmpty() || fileName.isEmpty()) return QString();
        QDirIterator it(rootDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString candidate = it.next();
            if (QFileInfo(candidate).fileName().compare(fileName, Qt::CaseInsensitive) == 0) {
                return candidate;
            }
        }
        return QString();
    }

    static int relinkMissingFootage(const QString& rootDir, const QVector<FootageItem*>& targets) {
        int relinked = 0;
        for (auto* footage : targets) {
            if (!footage) continue;
            const QFileInfo currentInfo(footage->filePath);
            if (currentInfo.exists()) continue;
            const QString replacement = findByFileName(rootDir, currentInfo.fileName());
            if (!replacement.isEmpty()) {
                footage->filePath = QFileInfo(replacement).absoluteFilePath();
                ++relinked;
            }
        }
        return relinked;
    }

    static void showDependencyGraphDialog(QWidget* parent, ArtifactProjectService* svc) {
        if (!svc) return;
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) return;

        auto* dialog = new QDialog(parent);
        dialog->setWindowTitle("Composition Dependency Graph");
        dialog->resize(720, 520);
        auto* layout = new QVBoxLayout(dialog);
        auto* tree = new QTreeWidget(dialog);
        tree->setColumnCount(3);
        tree->setHeaderLabels(QStringList() << "Node" << "Depends On" << "Type");
        layout->addWidget(tree);

        QHash<QString, QString> footageByPath;
        const auto roots = project->projectItems();
        std::function<void(ProjectItem*)> gather = [&](ProjectItem* item) {
            if (!item) return;
            if (item->type() == eProjectItemType::Footage) {
                auto* f = static_cast<FootageItem*>(item);
                footageByPath.insert(f->filePath, f->name.toQString());
            }
            for (auto* c : item->children) gather(c);
        };
        for (auto* r : roots) gather(r);

        std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
            if (!item) return;
            if (item->type() == eProjectItemType::Composition) {
                auto* compItem = static_cast<CompositionItem*>(item);
                auto rootNode = new QTreeWidgetItem(tree);
                rootNode->setText(0, compItem->name.toQString());
                rootNode->setText(1, "-");
                rootNode->setText(2, "Composition");

                auto findRes = project->findComposition(compItem->compositionId);
                if (findRes.success) {
                    if (auto comp = findRes.ptr.lock()) {
                        for (const auto& layer : comp->allLayer()) {
                            if (!layer) continue;
                            auto* layerNode = new QTreeWidgetItem(rootNode);
                            layerNode->setText(0, layer->layerName());
                            layerNode->setText(1, "-");
                            layerNode->setText(2, "Layer");

                            const auto groups = layer->getLayerPropertyGroups();
                            for (const auto& g : groups) {
                                for (const auto& p : g.allProperties()) {
                                    if (!p) continue;
                                    const QString propName = p->getName().toLower();
                                    if (!propName.contains("path") &&
                                        !propName.contains("source") &&
                                        !propName.contains("composition")) {
                                        continue;
                                    }
                                    const QString value = p->getValue().toString().trimmed();
                                    if (value.isEmpty()) continue;
                                    auto* depNode = new QTreeWidgetItem(layerNode);
                                    depNode->setText(0, p->getName());
                                    depNode->setText(1, value);
                                    if (footageByPath.contains(value)) {
                                        depNode->setText(2, "Footage");
                                    } else if (value.contains('-') || value.contains('{')) {
                                        depNode->setText(2, "Composition?");
                                    } else {
                                        depNode->setText(2, "PropertyRef");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (auto* c : item->children) visit(c);
        };
        for (auto* r : roots) visit(r);

        tree->expandToDepth(1);
        dialog->exec();
        dialog->deleteLater();
    }
};

ArtifactProjectView::ArtifactProjectView(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    setMouseTracking(true);
    setAcceptDrops(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);
    impl_->hoverTimer = new QTimer(this);
    impl_->hoverTimer->setSingleShot(true);
    
    verticalScrollBar_ = new QScrollBar(Qt::Vertical, this);
    scrollY_ = 0;

    connect(verticalScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
        scrollY_ = value;
        update();
    });
}

ArtifactProjectView::~ArtifactProjectView() { delete impl_; }

void ArtifactProjectView::setModel(QAbstractItemModel* model)
{
    if (!impl_) {
        return;
    }
    if (impl_->model == model) {
        refreshVisibleContent();
        return;
    }

    for (const auto& connection : impl_->modelConnections) {
        QObject::disconnect(connection);
    }
    impl_->modelConnections.clear();

    if (impl_->selectionModel) {
        delete impl_->selectionModel;
        impl_->selectionModel = nullptr;
    }

    impl_->model = model;

    if (impl_->model) {
        impl_->selectionModel = new QItemSelectionModel(impl_->model, this);
        QObject::connect(impl_->selectionModel, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) {
                update();
            });
        QObject::connect(impl_->selectionModel, &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) {
                update();
                if (impl_->selectionModel) {
                    itemSelected(impl_->selectionModel->currentIndex());
                }
            });
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::modelReset, this,
            [this]() { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model,
            qOverload<const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint>(&QAbstractItemModel::layoutChanged),
            this,
            [this](const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex&, int, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex&, int, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::rowsMoved, this,
            [this](const QModelIndex&, int, int, const QModelIndex&, int) { refreshVisibleContent(); }));
        impl_->modelConnections.push_back(QObject::connect(impl_->model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
                update();
            }));
    }

    refreshVisibleContent();
}

void ArtifactProjectView::setPresentationMode(const PresentationMode mode)
{
    if (!impl_) {
        return;
    }
    if (impl_->presentationMode == mode) {
        return;
    }
    impl_->presentationMode = mode;
    impl_->hoverHeaderColumn = -1;
    impl_->hoverBranchIndex = QModelIndex();
    impl_->hoverIndex = QModelIndex();
    refreshVisibleContent();
}

ArtifactProjectView::PresentationMode ArtifactProjectView::presentationMode() const
{
    return impl_ ? impl_->presentationMode : PresentationMode::List;
}

QAbstractItemModel* ArtifactProjectView::model() const
{
    return impl_ ? impl_->model : nullptr;
}

QItemSelectionModel* ArtifactProjectView::selectionModel() const
{
    return impl_ ? impl_->selectionModel : nullptr;
}

QModelIndex ArtifactProjectView::currentIndex() const
{
    if (!selectionModel()) {
        return {};
    }
    return selectionModel()->currentIndex();
}

void ArtifactProjectView::setCurrentIndex(const QModelIndex& index)
{
    if (!selectionModel()) {
        return;
    }
    selectionModel()->setCurrentIndex(
        index,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
    ensureIndexVisible(index);
    refreshVisibleContent();
}

void ArtifactProjectView::setSortingEnabled(const bool enabled)
{
    if (!impl_) {
        return;
    }
    impl_->sortingEnabled = enabled;
}

void ArtifactProjectView::sortByColumn(const int column, const Qt::SortOrder order)
{
    if (impl_ && impl_->sortingEnabled && impl_->model) {
        impl_->model->sort(column, order);
        refreshVisibleContent();
    }
}

void ArtifactProjectView::setColumnWidth(const int column, const int width)
{
    if (!impl_ || column < 0) {
        return;
    }
    if (column >= impl_->columnWidths.size()) {
        impl_->columnWidths.resize(column + 1);
    }
    impl_->columnWidths[column] = std::max(40, width);
    refreshVisibleContent();
}

void ArtifactProjectView::expand(const QModelIndex& index)
{
    setExpanded(index, true);
}

void ArtifactProjectView::collapse(const QModelIndex& index)
{
    setExpanded(index, false);
}

void ArtifactProjectView::setExpanded(const QModelIndex& index, const bool expanded)
{
    if (!impl_) {
        return;
    }
    const QModelIndex index0 = index.siblingAtColumn(0);
    if (!index0.isValid() || !impl_->hasChildren(index0)) {
        return;
    }
    impl_->setExpandedState(index0, expanded);
    refreshVisibleContent();
}

void ArtifactProjectView::expandAll()
{
    if (!impl_ || !impl_->model) {
        return;
    }
    std::function<void(const QModelIndex&)> expandRecursive = [&](const QModelIndex& parent) {
        const int childCount = impl_->model->rowCount(parent);
        for (int row = 0; row < childCount; ++row) {
            const QModelIndex child = impl_->model->index(row, 0, parent);
            if (!child.isValid()) {
                continue;
            }
            if (impl_->hasChildren(child)) {
                impl_->setExpandedState(child, true);
                expandRecursive(child);
            }
        }
    };
    expandRecursive({});
    refreshVisibleContent();
}

void ArtifactProjectView::collapseAll()
{
    if (!impl_) {
        return;
    }
    impl_->expandedKeys.clear();
    refreshVisibleContent();
}

void ArtifactProjectView::expandToDepth(const int depth)
{
    if (!impl_ || !impl_->model || depth < 0) {
        return;
    }
    std::function<void(const QModelIndex&, int)> expandRecursive = [&](const QModelIndex& parent, const int currentDepth) {
        const int childCount = impl_->model->rowCount(parent);
        for (int row = 0; row < childCount; ++row) {
            const QModelIndex child = impl_->model->index(row, 0, parent);
            if (!child.isValid()) {
                continue;
            }
            if (impl_->hasChildren(child) && currentDepth < depth) {
                impl_->setExpandedState(child, true);
                expandRecursive(child, currentDepth + 1);
            }
        }
    };
    expandRecursive({}, 0);
    refreshVisibleContent();
}

QModelIndex ArtifactProjectView::indexAt(const QPoint& pos) const
{
    if (!impl_) {
        return {};
    }
    if (impl_->isTileMode()) {
        const int row = impl_->tileRowForPoint(pos, width(), scrollY_);
        if (row < 0 || row >= impl_->visibleRows.size()) {
            return {};
        }
        return impl_->visibleRows[row].index0;
    }
    const int y = pos.y() + scrollY_;
    if (y < impl_->headerHeight) {
        return {};
    }
    const int row = (y - impl_->headerHeight) / impl_->rowHeight;
    if (row < 0 || row >= impl_->visibleRows.size()) {
        return {};
    }
    return impl_->visibleRows[row].index0;
}

QRect ArtifactProjectView::visualRect(const QModelIndex& index) const
{
    if (!impl_) {
        return {};
    }
    if (impl_->isTileMode()) {
        const int row = impl_->rowForIndex(index);
        if (row < 0) {
            return {};
        }
        return impl_->tileRectForRow(row, width()).translated(0, -scrollY_);
    }
    const int row = impl_->rowForIndex(index);
    if (row < 0) {
        return {};
    }
    const int y = impl_->headerHeight + row * impl_->rowHeight - scrollY_;
    const int x = 0;
    return QRect(x, y, std::max(width(), impl_->totalColumnWidth()), impl_->rowHeight);
}

void ArtifactProjectView::ensureIndexVisible(const QModelIndex& index)
{
    if (!impl_) {
        return;
    }
    if (impl_->isTileMode()) {
        const int row = impl_->rowForIndex(index);
        if (row < 0) {
            return;
        }
        const QRect tileRect = impl_->tileRectForRow(row, width());
        const int viewTop = scrollY_;
        const int viewBottom = scrollY_ + height();
        if (tileRect.top() < viewTop) {
            verticalScrollBar_->setValue(std::max(0, tileRect.top()));
        } else if (tileRect.bottom() > viewBottom) {
            verticalScrollBar_->setValue(std::max(0, tileRect.bottom() - height()));
        }
        return;
    }
    const int row = impl_->rowForIndex(index);
    if (row < 0) {
        return;
    }

    const int rowTop = impl_->headerHeight + row * impl_->rowHeight;
    const int rowBottom = rowTop + impl_->rowHeight;
    const int viewTop = scrollY_ + impl_->headerHeight;
    const int viewBottom = scrollY_ + height();

    if (rowTop < viewTop) {
        verticalScrollBar_->setValue(std::max(0, rowTop - impl_->headerHeight));
    } else if (rowBottom > viewBottom) {
        verticalScrollBar_->setValue(std::max(0, rowBottom - height()));
    }
}

void ArtifactProjectView::paintEvent(QPaintEvent* event)
{
    if (!impl_) {
        return;
    }

    if (impl_->isTileMode()) {
        paintTileMode(event);
    } else {
        paintListMode(event);
    }
}

void ArtifactProjectView::paintListMode(QPaintEvent* event)
{
    if (!impl_) {
        return;
    }
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Sync background fill (Modo style)
    painter.fillRect(rect(), QColor(40, 40, 40));

    const int contentWidth = std::max(width(), impl_->totalColumnWidth());

    // Draw Rows
    painter.save();
    // We want the rows to be clipped to the area below the header.
    painter.setClipRect(0, impl_->headerHeight, width(), height() - impl_->headerHeight);
    painter.translate(0, -scrollY_);

    const int firstRow = std::max(0, (scrollY_ - impl_->headerHeight) / impl_->rowHeight);
    const int visibleRowCount = height() / impl_->rowHeight + 3;
    const int totalVisibleRows = static_cast<int>(impl_->visibleRows.size());
    const int lastRow = std::min(totalVisibleRows, firstRow + visibleRowCount);

    for (int row = firstRow; row < lastRow; ++row) {
        const auto& visibleRow = impl_->visibleRows[row];
        const QModelIndex index0 = visibleRow.index0;
        const QRect rowRect(
            0,
            impl_->headerHeight + row * impl_->rowHeight,
            contentWidth,
            impl_->rowHeight);

        const bool selected = selectionModel() && selectionModel()->isRowSelected(index0.row(), index0.parent());
        const bool hovered = impl_->hoverIndex.isValid() && impl_->hoverIndex == index0;

        const QColor rowFill = selected ? Impl::Colors::RowSelected : (hovered ? Impl::Colors::RowHover : Impl::Colors::Background);
        painter.fillRect(rowRect, rowFill);

        painter.setPen(Impl::Colors::RowBorder);
        painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());

        int cellX = 0;
        const int configuredColumnCount = static_cast<int>(impl_->columnWidths.size());
        const int modelColumnCount = impl_->model ? impl_->model->columnCount({}) : 0;
        const int columnCount = std::max(configuredColumnCount, modelColumnCount);

        for (int column = 0; column < columnCount; ++column) {
            const int width = column < impl_->columnWidths.size() ? impl_->columnWidths[column] : 120;
            const QRect cellRect(cellX, rowRect.top(), width, rowRect.height());

            const QModelIndex cellIndex = index0.siblingAtColumn(column);
            painter.setPen(selected ? Impl::Colors::RowSelectedText : Impl::Colors::RowText);

            if (column == 0) {
                if (impl_->editingIndex.isValid() && impl_->editingIndex == index0 && impl_->nameEditor) {
                    const int indent = visibleRow.depth * impl_->indentWidth;
                    int textLeft = cellRect.left() + 8 + indent + (impl_->hasChildren(index0) ? 18 : 0);
                    const QVariant iconVar = cellIndex.data(Qt::DecorationRole);
                    if (iconVar.canConvert<QIcon>()) { textLeft += 22; }
                    const QRect editorRect(textLeft, cellRect.top() + 2, std::max(50, cellRect.right() - textLeft - 8), cellRect.height() - 4);
                    // Name editor needs to be moved accounting for scrollY_
                    const QRect screenEditorRect = editorRect.translated(0, -scrollY_);
                    if (impl_->nameEditor->geometry() != screenEditorRect) impl_->nameEditor->setGeometry(screenEditorRect);
                    if (!impl_->nameEditor->isVisible()) { impl_->nameEditor->show(); impl_->nameEditor->setFocus(); }
                } else {
                    const int indent = visibleRow.depth * impl_->indentWidth;
                    const QRect contentRect = cellRect.adjusted(8 + indent, 0, -8, 0);
                    if (impl_->hasChildren(index0)) {
                        const QRect branchRect(contentRect.left(), contentRect.center().y() - 6, 12, 12);
                        const bool branchHovered = (impl_->hoverBranchIndex == index0);
                        QPainterPath branchPath;
                        if (impl_->isExpanded(index0)) {
                            branchPath.moveTo(branchRect.left() + 2, branchRect.top() + 4);
                            branchPath.lineTo(branchRect.right() - 2, branchRect.top() + 4);
                            branchPath.lineTo(branchRect.center().x(), branchRect.bottom() - 2);
                        } else {
                            branchPath.moveTo(branchRect.left() + 4, branchRect.top() + 2);
                            branchPath.lineTo(branchRect.left() + 4, branchRect.bottom() - 2);
                            branchPath.lineTo(branchRect.right() - 2, branchRect.center().y());
                        }
                        painter.fillPath(branchPath, (selected || branchHovered) ? Impl::Colors::RowSelectedText : Impl::Colors::BranchNormal);
                    }
                    int textLeft = contentRect.left() + (impl_->hasChildren(index0) ? 18 : 0);
                    const QVariant iconVar = cellIndex.data(Qt::DecorationRole);
                    if (iconVar.canConvert<QIcon>()) {
                        const QIcon icon = qvariant_cast<QIcon>(iconVar);
                        const QRect iconRect(textLeft, rowRect.top() + (rowRect.height() - 16) / 2, 16, 16);
                        icon.paint(&painter, iconRect);
                        textLeft += 22;
                    }
                    painter.drawText(QRect(textLeft, rowRect.top(), std::max(0, cellRect.right() - textLeft - 8), rowRect.height()),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        painter.fontMetrics().elidedText(cellIndex.data(Qt::DisplayRole).toString(), Qt::ElideRight, std::max(0, cellRect.width() - (textLeft - cellRect.left()) - 12)));
                }
            } else {
                const QString text = cellIndex.data(Qt::DisplayRole).toString();
                Qt::Alignment alignment = Qt::AlignVCenter | Qt::AlignLeft;
                if (column == 1 || column == 2 || column == 3) alignment = Qt::AlignVCenter | Qt::AlignRight;
                painter.drawText(cellRect.adjusted(8, 0, -8, 0), alignment, painter.fontMetrics().elidedText(text, Qt::ElideRight, cellRect.width() - 16));
            }
            painter.setPen(Impl::Colors::RowBorder);
            painter.drawLine(cellRect.topRight(), cellRect.bottomRight());
            cellX += width;
        }
    }
    painter.restore();

    // Draw Header
    painter.fillRect(QRect(0, 0, width(), impl_->headerHeight), Impl::Colors::HeaderBackground);

    int headerX = 0;
    const int configuredColumnCount = static_cast<int>(impl_->columnWidths.size());
    const int modelColumnCount = impl_->model ? impl_->model->columnCount({}) : 0;
    const int columnCount = std::max(configuredColumnCount, modelColumnCount);

    for (int column = 0; column < columnCount; ++column) {
        const int width = column < impl_->columnWidths.size() ? impl_->columnWidths[column] : 120;
        const QRect headerRect(headerX, 0, width, impl_->headerHeight);

        if (headerRect.right() >= 0 && headerRect.left() <= this->width()) {
            if (impl_->hoverHeaderColumn == column && impl_->resizingColumn == -1) {
                painter.fillRect(headerRect.adjusted(0, 0, -1, -1), Impl::Colors::HeaderHover);
            }

            painter.setPen(Impl::Colors::HeaderText);
            const QString label = impl_->model ? impl_->model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString() : QString();
            painter.drawText(headerRect.adjusted(10, 0, -20, 0), Qt::AlignVCenter | Qt::AlignLeft, label);

            if (impl_->sortingEnabled && impl_->sortColumn == column) {
                const int arrowSize = 8;
                const int arrowX = headerRect.right() - 15;
                const int arrowY = headerRect.center().y();
                QPainterPath arrowPath;
                if (impl_->sortOrder == Qt::AscendingOrder) {
                    arrowPath.moveTo(arrowX - arrowSize/2, arrowY + arrowSize/4);
                    arrowPath.lineTo(arrowX + arrowSize/2, arrowY + arrowSize/4);
                    arrowPath.lineTo(arrowX, arrowY - arrowSize/4);
                } else {
                    arrowPath.moveTo(arrowX - arrowSize/2, arrowY - arrowSize/4);
                    arrowPath.lineTo(arrowX + arrowSize/2, arrowY - arrowSize/4);
                    arrowPath.lineTo(arrowX, arrowY + arrowSize/4);
                }
                painter.fillPath(arrowPath, Impl::Colors::HeaderText);
            }

            painter.setPen(Impl::Colors::HeaderSeparator);
            painter.drawLine(headerRect.topRight() + QPoint(0, 4), headerRect.bottomRight() - QPoint(0, 4));

            if (column + 1 < columnCount) {
                const bool isResizeHot = (impl_->hoverHeaderColumn == column || impl_->resizingColumn == column);
                const QColor gripColor = isResizeHot
                    ? QColor(214, 222, 231, 210)
                    : QColor(143, 153, 166, 150);
                painter.setPen(QPen(gripColor, 1.4, Qt::SolidLine, Qt::RoundCap));
                const int gripX = headerRect.right() - (kHeaderGripWidth / 2);
                const int centerY = headerRect.center().y();
                const int startY = centerY - (kHeaderGripHeight / 2);
                for (int i = 0; i < 3; ++i) {
                    const int y = startY + i * 3;
                    painter.drawLine(QPointF(gripX - 1.5, y), QPointF(gripX + 1.5, y));
                }
            }
        }
        headerX += width;
    }
    painter.setPen(Impl::Colors::HeaderSeparator);
    painter.drawLine(0, impl_->headerHeight - 1, width(), impl_->headerHeight - 1);

    if (impl_->visibleRows.isEmpty()) {
        drawProjectViewEmptyState(
            painter,
            QRect(0, impl_->headerHeight, width(), std::max(0, height() - impl_->headerHeight)));
    }
}

void ArtifactProjectView::paintTileMode(QPaintEvent* event)
{
    if (!impl_) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), QColor(0x28, 0x28, 0x28));

    const QRect dirtyRect = event ? event->rect() : rect();
    const int viewWidth = std::max(1, width());
    const QFontMetrics fm = painter.fontMetrics();

    if (impl_->visibleRows.isEmpty()) {
        drawProjectViewEmptyState(painter, rect().adjusted(0, 8, 0, 0));
        return;
    }

    for (int row = 0; row < impl_->visibleRows.size(); ++row) {
        const auto& visibleRow = impl_->visibleRows[row];
        const QModelIndex index0 = visibleRow.index0;
        const QRect tileRect = impl_->tileRectForRow(row, viewWidth).translated(0, -scrollY_);
        if (!dirtyRect.adjusted(-16, -16, 16, 16).intersects(tileRect)) {
            continue;
        }

        QModelIndex sourceIdx = index0;
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index0.model())) {
            sourceIdx = proxy->mapToSource(index0).siblingAtColumn(0);
        }

        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        const eProjectItemType type = typeVar.isValid()
                                          ? static_cast<eProjectItemType>(typeVar.toInt())
                                          : eProjectItemType::Footage;
        const bool selected = selectionModel() &&
                              selectionModel()->isRowSelected(index0.row(), index0.parent());
        const bool hovered = impl_->hoverIndex.isValid() && impl_->hoverIndex == index0;
        const bool hasChildren = impl_->hasChildren(index0);

        QColor tileFill = selected ? Impl::Colors::RowSelected
                                   : (hovered ? Impl::Colors::RowHover : QColor(0x2B, 0x2B, 0x2D));
        if (type == eProjectItemType::Folder) {
            tileFill = tileFill.lighter(selected ? 110 : 104);
        }

        QColor border = selected ? Impl::Colors::RowSelectedText
                                 : QColor(0x44, 0x44, 0x49);
        border.setAlpha(hovered ? 220 : 170);

        painter.setPen(QPen(border, selected ? 2.0 : 1.0));
        painter.setBrush(tileFill);
        painter.drawRoundedRect(tileRect.adjusted(0, 0, -1, -1), 8, 8);

        QColor accent = QColor(88, 140, 198);
        if (type == eProjectItemType::Folder) {
            accent = QColor(186, 146, 58);
        } else if (type == eProjectItemType::Footage) {
            accent = QColor(74, 165, 104);
        } else if (type == eProjectItemType::Solid) {
            accent = QColor(132, 104, 194);
        }
        if (selected) {
            accent = accent.lighter(115);
        }
        painter.fillRect(QRect(tileRect.left() + 1, tileRect.top() + 1,
                               tileRect.width() - 2, 3), accent);

        const QRect previewRect = impl_->tilePreviewRect(tileRect);
        const QRect titleRect = impl_->tileTitleRect(tileRect);
        const QRect metadataRect = impl_->tileMetadataRect(tileRect);

        QPixmap preview = impl_->previewForIndex(index0, previewRect.size());
        if (!preview.isNull()) {
            painter.save();
            painter.setClipPath([&]() {
                QPainterPath clip;
                clip.addRoundedRect(previewRect, 6, 6);
                return clip;
            }());
            const QPoint previewTopLeft =
                previewRect.center() -
                QPoint(preview.width() / 2, preview.height() / 2);
            painter.drawPixmap(previewTopLeft, preview);
            painter.restore();
            painter.setPen(QPen(QColor(18, 18, 18, 160), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(previewRect.adjusted(0, 0, -1, -1), 6, 6);
        } else {
            QColor placeholder = QColor(70, 74, 84);
            if (type == eProjectItemType::Composition) {
                placeholder = QColor(74, 128, 191);
            } else if (type == eProjectItemType::Folder) {
                placeholder = QColor(176, 138, 46);
            } else if (type == eProjectItemType::Solid) {
                placeholder = QColor(110, 88, 170);
            } else if (type == eProjectItemType::Footage) {
                placeholder = QColor(66, 148, 98);
            }
            const QPixmap placeholderTexture = projectViewIllustration(
                QStringLiteral("Studio/project_tile_placeholder.png"),
                previewRect.size());
            painter.save();
            QPainterPath placeholderClip;
            placeholderClip.addRoundedRect(previewRect, 6, 6);
            painter.setClipPath(placeholderClip);
            painter.fillRect(previewRect, placeholder.darker(118));
            if (!placeholderTexture.isNull()) {
                const QPoint textureTopLeft =
                    previewRect.center() -
                    QPoint(placeholderTexture.width() / 2, placeholderTexture.height() / 2);
                painter.setOpacity(selected ? 0.86 : 0.72);
                painter.drawPixmap(textureTopLeft, placeholderTexture);
                painter.setOpacity(1.0);
            }
            painter.fillRect(previewRect, QColor(placeholder.red(), placeholder.green(), placeholder.blue(), 62));
            painter.restore();
            painter.setPen(QPen(QColor(18, 18, 18, 170), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(previewRect.adjusted(0, 0, -1, -1), 6, 6);
            painter.setPen(QColor(245, 245, 245));
            QFont badgeFont = painter.font();
            badgeFont.setBold(true);
            badgeFont.setPointSize(std::max(8, badgeFont.pointSize()));
            painter.setFont(badgeFont);
            painter.drawText(previewRect, Qt::AlignCenter,
                             projectItemTypeLabel(type).left(1).toUpper());
        }

        const QString title = sourceIdx.data(Qt::DisplayRole).toString();
        painter.setPen(selected ? Impl::Colors::RowSelectedText : QColor(240, 240, 240));
        QFont titleFont = painter.font();
        titleFont.setPointSize(std::max(10, titleFont.pointSize()));
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                         fm.elidedText(title, Qt::ElideRight, titleRect.width()));

        const QStringList metadata = projectItemMetadataLines(sourceIdx, item);
        painter.setFont(font());
        painter.setPen(selected ? QColor(235, 238, 242) : QColor(185, 188, 196));

        const int maxLines = std::min(impl_->tileTextLines,
                                      static_cast<int>(metadata.size()));
        for (int i = 0; i < maxLines; ++i) {
            const QRect lineRect(metadataRect.left(),
                                 metadataRect.top() + i * fm.height(),
                                 metadataRect.width(),
                                 fm.height());
            painter.drawText(lineRect, Qt::AlignLeft | Qt::AlignVCenter,
                             fm.elidedText(metadata[i], Qt::ElideRight, lineRect.width()));
        }

        const QString badgeText = projectItemTileBadgeText(item);
        const QFontMetrics badgeFm(painter.font());
        const int badgeW = std::min(tileRect.width() - 20,
                                    badgeFm.horizontalAdvance(badgeText) + 10);
        const QRect badgeRect(tileRect.right() - badgeW - 10,
                              tileRect.top() + 8,
                              badgeW,
                              std::max(18, badgeFm.height() + 4));
        QColor badgeBg = selected ? QColor(255, 255, 255, 26)
                                  : (hovered ? QColor(0, 0, 0, 46)
                                             : QColor(0, 0, 0, 34));
        QColor badgePen = selected ? QColor(245, 247, 250)
                                    : (hovered ? QColor(232, 236, 242)
                                               : QColor(210, 214, 220));
        painter.setBrush(badgeBg);
        painter.setPen(QPen(badgePen, hovered ? 1.2 : 1.0));
        painter.drawRoundedRect(badgeRect, 6, 6);
        painter.setPen(badgePen);
        painter.drawText(badgeRect.adjusted(6, 0, -6, 0),
                         Qt::AlignCenter, badgeText);

        const QString statusText = projectItemStatusChipText(item);
        const QFontMetrics statusFm(painter.font());
        const int statusW = std::min(tileRect.width() - 20,
                                     statusFm.horizontalAdvance(statusText) + 10);
        const QRect statusRect(tileRect.left() + 10,
                               badgeRect.bottom() + 4,
                               statusW,
                               std::max(18, statusFm.height() + 4));
        const QColor statusAccent = projectItemStatusChipColor(statusText);
        QColor statusBg = statusAccent;
        statusBg.setAlpha(selected ? 70 : 46);
        QColor statusPen = statusAccent.lighter(selected ? 145 : 128);
        painter.setBrush(statusBg);
        painter.setPen(QPen(statusPen, 1.0));
        painter.drawRoundedRect(statusRect, 6, 6);
        painter.setPen(statusPen);
        painter.drawText(statusRect.adjusted(6, 0, -6, 0),
                         Qt::AlignCenter, statusText);

        if (item && item->type() == eProjectItemType::Composition) {
            auto* svc = ArtifactProjectService::instance();
            if (svc) {
                const auto found = svc->findComposition(static_cast<CompositionItem*>(item)->compositionId);
                if (auto comp = found.ptr.lock()) {
                    const auto responsiveLayout = comp->responsiveLayout();
                    const QString layoutSummary = QStringLiteral("Layout: %1")
                        .arg(responsiveLayoutActiveSummary(responsiveLayout));
                    const QFontMetrics layoutFm(painter.font());
                    const int layoutW = std::min(tileRect.width() - 20,
                                                 layoutFm.horizontalAdvance(layoutSummary) + 10);
                    const QRect layoutRect(tileRect.right() - layoutW - 10,
                                           statusRect.bottom() + 4,
                                           layoutW,
                                           std::max(18, layoutFm.height() + 4));
                    painter.setBrush(selected ? QColor(255, 255, 255, 16)
                                              : QColor(0, 0, 0, 28));
                    painter.setPen(QPen(selected ? QColor(220, 226, 235)
                                                 : QColor(180, 188, 198),
                                        1.0));
                    painter.drawRoundedRect(layoutRect, 6, 6);
                    painter.setPen(selected ? QColor(240, 243, 246)
                                            : QColor(200, 206, 214));
                    painter.drawText(layoutRect.adjusted(6, 0, -6, 0),
                                     Qt::AlignCenter,
                                     layoutFm.elidedText(layoutSummary, Qt::ElideRight, layoutRect.width() - 12));
                }
            }
        }

        // Proxy status badge for footage items
        if (item && item->type() == eProjectItemType::Footage) {
            const QString proxyPath = proxyFilePathForFootage(static_cast<FootageItem*>(item)->filePath);
            const bool hasProxy = !proxyPath.isEmpty() && QFileInfo(proxyPath).exists();
            if (hasProxy) {
                const bool isStale = [&]() {
                    const auto it = proxyMetadata().constFind(static_cast<FootageItem*>(item)->filePath);
                    if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                        const QFileInfo src(static_cast<FootageItem*>(item)->filePath);
                        return src.exists() && src.lastModified() > it->sourceLastModified;
                    }
                    return false;
                }();
                const QString proxyBadgeText = isStale ? QStringLiteral("Proxy ⚠") : QStringLiteral("Proxy");
                const QFontMetrics pf(painter.font());
                const int pw = std::min(tileRect.width() - 20, pf.horizontalAdvance(proxyBadgeText) + 10);
                const QRect proxyRect(tileRect.right() - pw - 10,
                                      badgeRect.bottom() + 4,
                                      pw, std::max(18, pf.height() + 4));
                painter.setBrush(isStale ? QColor(255, 200, 50, 50) : QColor(50, 200, 100, 40));
                painter.setPen(QPen(isStale ? QColor(255, 200, 50) : QColor(100, 220, 140), 1.0));
                painter.drawRoundedRect(proxyRect, 6, 6);
                painter.setPen(isStale ? QColor(255, 200, 50) : QColor(100, 220, 140));
                painter.drawText(proxyRect.adjusted(6, 0, -6, 0), Qt::AlignCenter, proxyBadgeText);
            }
        }
    }
}

void ArtifactProjectView::handleItemDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    QModelIndex actualIdx = index;
    if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
        actualIdx = proxy->mapToSource(index);
    }

    QVariant typeVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
    if (typeVar.isValid()) {
        if (typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
             QVariant idVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
             if (idVar.isValid()) {
                 CompositionID cid(idVar.toString());
                 ArtifactProjectService::instance()->changeCurrentComposition(cid);
             }
        } else if (typeVar.toInt() == static_cast<int>(eProjectItemType::Folder)) {
            if (impl_->hasChildren(actualIdx)) {
                setExpanded(actualIdx, !impl_->isExpanded(actualIdx));
            }
        } else if (typeVar.toInt() == static_cast<int>(eProjectItemType::Footage)) {
            QVariant ptrVar = actualIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item && item->type() == eProjectItemType::Footage) {
                Q_UNUSED(item);
            }
        }
    }
}

void ArtifactProjectView::mouseDoubleClickEvent(QMouseEvent* event) {
    const QPoint mousePos = event->position().toPoint();
    if (!impl_->isTileMode() && mousePos.y() < impl_->headerHeight) {
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        if (resizeHit.column >= 0 && std::abs(mousePos.x() - resizeHit.boundaryX) <= kHeaderResizeHitRadius) {
            const QFontMetrics fm(font());
            int widest = fm.horizontalAdvance(impl_->model
                ? impl_->model->headerData(resizeHit.column, Qt::Horizontal, Qt::DisplayRole).toString()
                : QString());
            for (const auto& row : impl_->visibleRows) {
                if (!row.index0.isValid()) {
                    continue;
                }
                const QModelIndex cellIndex = row.index0.siblingAtColumn(resizeHit.column);
                if (!cellIndex.isValid()) {
                    continue;
                }
                int width = fm.horizontalAdvance(cellIndex.data(Qt::DisplayRole).toString());
                if (resizeHit.column == 0) {
                    width += 36;
                } else {
                    width += 20;
                }
                widest = std::max(widest, width);
            }
            setColumnWidth(resizeHit.column, std::clamp(widest + 24, 56, 420));
            refreshVisibleContent();
            event->accept();
            return;
        }
    }

    const QModelIndex idx = indexAt(mousePos);
    if (idx.isValid()) {
        itemDoubleClicked(idx);
        handleItemDoubleClicked(idx);
        event->accept();
        return;
    }
    event->ignore();
}

void ArtifactProjectView::editIndex(const QModelIndex& index) {
    if (!index.isValid() || !impl_) return;
    impl_->editingIndex = index.siblingAtColumn(0);
    
    if (!impl_->nameEditor) {
        QLineEdit* editor = new QLineEdit(this);
        impl_->nameEditor = editor;
        {
            QPalette pal = impl_->nameEditor->palette();
            pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
            impl_->nameEditor->setPalette(pal);
        }
        connect(impl_->nameEditor, &QLineEdit::editingFinished, this, [this]() {
            if (!impl_ || !impl_->editingIndex.isValid() || !impl_->nameEditor) return;
            const QString newName = impl_->nameEditor->text().trimmed();
            if (!newName.isEmpty()) {
                QModelIndex sourceIdx = impl_->editingIndex;
                if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                    sourceIdx = proxy->mapToSource(sourceIdx);
                }
                QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                if (item) {
                    renameProjectItem(item, newName);
                }
            }
            impl_->editingIndex = QModelIndex();
            impl_->nameEditor->hide();
            this->update();
        });
    }
    
    impl_->nameEditor->setText(index.data(Qt::DisplayRole).toString());
    impl_->nameEditor->selectAll();
    this->update();
}

void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event) {
    if (!impl_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint mousePos = event->position().toPoint();

    if (impl_->resizingColumn != -1) {
        const int deltaX = mousePos.x() - impl_->resizeStartX;
        impl_->columnWidths[impl_->resizingColumn] = std::max(40, impl_->columnWidths[impl_->resizingColumn] + deltaX);
        impl_->resizeStartX = mousePos.x();
        refreshVisibleContent();
        event->accept();
        return;
    }

    if (!impl_->isTileMode() && mousePos.y() < impl_->headerHeight) {
        int x = 0;
        int hoveredCol = -1;
        for (int i = 0; i < impl_->columnWidths.size(); ++i) {
            const int width = impl_->columnWidths[i];
            if (mousePos.x() >= x && mousePos.x() < x + width) {
                hoveredCol = i;
                break;
            }
            x += width;
        }
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        setCursor(resizeHit.column >= 0 ? Qt::SplitHCursor : Qt::ArrowCursor);
        if (hoveredCol != impl_->hoverHeaderColumn) { impl_->hoverHeaderColumn = hoveredCol; update(); }
    } else {
        setCursor(Qt::ArrowCursor);
        if (impl_->hoverHeaderColumn != -1) { impl_->hoverHeaderColumn = -1; update(); }
    }

    const QModelIndex idx = indexAt(mousePos);
    QModelIndex branchIdx;
    if (!impl_->isTileMode() && idx.isValid() && mousePos.y() >= impl_->headerHeight) {
        const QRect rowRect = visualRect(idx);
        const int row = impl_->rowForIndex(idx);
        const int depth = (row >= 0 && row < impl_->visibleRows.size()) ? impl_->visibleRows[row].depth : 0;
        const QRect branchRect(8 + depth * impl_->indentWidth, rowRect.top(), 20, rowRect.height());
        if (impl_->hasChildren(idx) && branchRect.contains(mousePos)) branchIdx = idx;
    }
    if (branchIdx != impl_->hoverBranchIndex) { impl_->hoverBranchIndex = branchIdx; update(); }

    if (!impl_->hoverTimer) {
        impl_->hoverTimer = new QTimer(this);
        impl_->hoverTimer->setSingleShot(true);
        connect(impl_->hoverTimer, &QTimer::timeout, this, [this]() {
            const QPoint localPos = mapFromGlobal(QCursor::pos());
            if (!rect().contains(localPos)) return;
            const QModelIndex currentIndex = indexAt(localPos);
            if (!currentIndex.isValid() || currentIndex != impl_->hoverIndex) return;
            QModelIndex sourceIdx = currentIndex;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(currentIndex.model())) sourceIdx = proxy->mapToSource(currentIndex).siblingAtColumn(0);
            const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            if (!typeVar.isValid()) return;
            const auto type = static_cast<eProjectItemType>(typeVar.toInt());
            if (type != eProjectItemType::Footage && type != eProjectItemType::Composition) return;
            if (!impl_->hoverPopup) impl_->hoverPopup = new HoverThumbnailPopupWidget();
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            impl_->hoverPopup->setThumbnail(projectItemPreviewPixmap(item, QSize(200, 112)));
            impl_->hoverPopup->setLabels(projectItemMetadataLines(sourceIdx, item));
            impl_->hoverPopup->showAt(mapToGlobal(visualRect(currentIndex).topRight() + QPoint(14, 6)));
        });
    }

    if (event->buttons() != Qt::NoButton && impl_->resizingColumn == -1) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverIndex = QModelIndex();
        if ((mousePos - impl_->dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            const QModelIndex dragIdx = indexAt(impl_->dragStartPos);
            if (dragIdx.isValid() && selectionModel()) {
                auto* mime = new QMimeData();
                QList<QUrl> urls;
                QStringList filePaths;
                const auto selectedRows = selectionModel()->selectedRows(0);
                for (const QModelIndex& proxyIdx : selectedRows) {
                    QModelIndex sourceIdx = proxyIdx;
                    if (auto* proxy = qobject_cast<const QSortFilterProxyModel*>(proxyIdx.model()))
                        sourceIdx = proxy->mapToSource(proxyIdx).siblingAtColumn(0);
                    const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                    ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                    if (item && item->type() == eProjectItemType::Footage) {
                        const QString path = static_cast<FootageItem*>(item)->filePath;
                        if (!path.isEmpty()) {
                            urls.append(QUrl::fromLocalFile(path));
                            filePaths.append(path);
                        }
                    }
                }
                if (!urls.isEmpty()) {
                    mime->setUrls(urls);
                    mime->setText(filePaths.join(QStringLiteral("\n")));
                } else {
                    delete mime;
                    mime = impl_->model->mimeData(selectedRows);
                }
                auto* drag = new QDrag(this);
                drag->setMimeData(mime);
                drag->exec(Qt::CopyAction | Qt::MoveAction);
            }
        }
        event->accept();
        return;
    }

    if (idx != impl_->hoverIndex) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverIndex = idx;
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->stop();
        if (idx.isValid()) impl_->hoverTimer->start(1100);
        update();
    } else if (idx.isValid() && (mousePos - impl_->hoverStartPos).manhattanLength() > 6) {
        if (impl_->hoverPopup) impl_->hoverPopup->hide();
        impl_->hoverTimer->stop();
        impl_->hoverStartPos = mousePos;
        impl_->hoverTimer->start(1100);
    }
    event->accept();
}

void ArtifactProjectView::wheelEvent(QWheelEvent* event)
{
    if (impl_ && impl_->isTileMode() && (event->modifiers() & Qt::ControlModifier)) {
        const int delta = event->angleDelta().y();
        if (delta != 0) {
            impl_->setTileDensityStep(delta > 0 ? 1 : -1);
            refreshVisibleContent();
            event->accept();
            return;
        }
    }
    if (verticalScrollBar_) {
        verticalScrollBar_->setValue(verticalScrollBar_->value() - event->angleDelta().y());
    }
    event->accept();
}

void ArtifactProjectView::leaveEvent(QEvent* event) {
    if (!impl_) {
        QWidget::leaveEvent(event);
        return;
    }
    if (impl_->hoverTimer) {
        impl_->hoverTimer->stop();
    }
    if (impl_->hoverPopup) {
        impl_->hoverPopup->hide();
    }
    impl_->hoverIndex = QModelIndex();
    impl_->hoverHeaderColumn = -1;
    impl_->hoverBranchIndex = QModelIndex();
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void ArtifactProjectView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (verticalScrollBar_) {
        verticalScrollBar_->setGeometry(width() - 8, 0, 8, height());
    }
    updateScrollRange();
    update();
}

void ArtifactProjectView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleProjectViewRefresh(this);
}

bool ArtifactProjectView::event(QEvent* event)
{
    const bool handled = QWidget::event(event);
    if (event && (event->type() == QEvent::WindowActivate ||
                  event->type() == QEvent::ActivationChange ||
                  event->type() == QEvent::PolishRequest)) {
        scheduleProjectViewRefresh(this);
    }
    return handled;
}

void ArtifactProjectView::updateScrollRange()
{
    if (!impl_ || !verticalScrollBar_) return;
    int contentHeight = impl_->headerHeight + static_cast<int>(impl_->visibleRows.size()) * impl_->rowHeight;
    if (impl_->isTileMode()) {
        const int columns = impl_->tileColumns(width());
        const int rowCount = columns > 0
                                 ? static_cast<int>((impl_->visibleRows.size() + columns - 1) / columns)
                                 : 0;
        contentHeight = impl_->tileContentTop +
                        rowCount * (impl_->tileHeight + impl_->tileSpacing) +
                        impl_->tileContentBottom;
    }
    verticalScrollBar_->setPageStep(height());
    verticalScrollBar_->setRange(0, std::max(0, contentHeight - height()));
    verticalScrollBar_->setVisible(contentHeight > height());
}

void ArtifactProjectView::refreshVisibleContent()
{
    if (!impl_) {
        return;
    }
    const int savedScroll = verticalScrollBar_ ? verticalScrollBar_->value() : 0;

    impl_->tilePreviewCache.clear();
    impl_->rebuildVisibleRows();
    updateScrollRange();

    if (verticalScrollBar_) {
        verticalScrollBar_->setValue(std::min(savedScroll, verticalScrollBar_->maximum()));
    }

    update();
}

void ArtifactProjectView::contextMenuEvent(QContextMenuEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    QMenu menu(this);
    auto svc = ArtifactProjectService::instance();
    QHash<QString, std::function<void()>> availableContextCommands;
    QHash<QString, QString> availableContextLabels;
    QHash<QString, std::function<void()>> availableNewCommands;
    QHash<QString, QString> availableNewLabels;
    ProjectItem* contextItem = nullptr;
    QModelIndex sourceIdx;
    eProjectItemType contextType = eProjectItemType::Footage;

    auto addTrackedAction = [this, &menu, &availableContextCommands, &availableContextLabels](const QString& id, const QString& label, std::function<void()> run, const QIcon& icon = QIcon()) {
        availableContextCommands.insert(id, run);
        availableContextLabels.insert(id, label);
        QAction* action = menu.addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastContextCommandId = id;
            impl_->lastContextCommandLabel = label;
            run();
        });
        if (!icon.isNull()) {
            action->setIcon(icon);
        }
    };

    auto addTrackedNewAction = [this, &availableNewCommands, &availableNewLabels](QMenu* targetMenu, const QString& id, const QString& label, std::function<void()> run, const QIcon& icon = QIcon()) {
        availableNewCommands.insert(id, run);
        availableNewLabels.insert(id, label);
        QAction* action = targetMenu->addAction(label, [this, id, label, run = std::move(run)]() mutable {
            impl_->lastNewCommandId = id;
            impl_->lastNewCommandLabel = label;
            run();
        });
        if (!icon.isNull()) {
            action->setIcon(icon);
        }
    };

    if (idx.isValid()) {
        sourceIdx = idx;
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
            sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
        }

        QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        contextItem = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
        
        QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
        contextType = typeVar.isValid() ? static_cast<eProjectItemType>(typeVar.toInt()) : eProjectItemType::Footage;

        addTrackedAction(QStringLiteral("open"), QStringLiteral("Open"), [this, idx, contextType]() {
            if (contextType == eProjectItemType::Footage) {
                itemDoubleClicked(idx);
            }
            handleItemDoubleClicked(idx);
        }, loadProjectViewIcon(QStringLiteral("Studio/file_open.svg")));
        addTrackedAction(QStringLiteral("copy_name"), QStringLiteral("Copy Name"), [sourceIdx]() {
            QApplication::clipboard()->setText(sourceIdx.data(Qt::DisplayRole).toString());
        }, loadProjectViewIcon(QStringLiteral("Studio/content_copy.svg")));
        const auto buildProjectItemBundle = [this, contextItem, svc]() -> QJsonObject {
            QJsonObject bundle;
            if (!contextItem) {
                return bundle;
            }

            auto serializeItem = [&](const ProjectItem* item, const auto& self) -> QJsonObject {
                QJsonObject obj;
                if (!item) {
                    return obj;
                }

                obj[QStringLiteral("name")] = item->name.toQString();
                obj[QStringLiteral("id")] = item->id.toString();

                switch (item->type()) {
                case eProjectItemType::Folder: {
                    obj[QStringLiteral("type")] = QStringLiteral("folder");
                    QJsonArray children;
                    for (const auto* child : item->children) {
                        children.append(self(child, self));
                    }
                    obj[QStringLiteral("children")] = children;
                    break;
                }
                case eProjectItemType::Footage: {
                    obj[QStringLiteral("type")] = QStringLiteral("footage");
                    const auto* footage = static_cast<const FootageItem*>(item);
                    obj[QStringLiteral("filePath")] = footage->filePath;
                    obj[QStringLiteral("filePathExists")] = QFileInfo(footage->filePath).exists();
                    obj[QStringLiteral("isSequence")] = footage->isSequence;
                    if (!footage->sequencePaths.isEmpty()) {
                        QJsonArray sequenceArray;
                        for (const QString& sequencePath : footage->sequencePaths) {
                            sequenceArray.append(sequencePath);
                        }
                        obj[QStringLiteral("sequencePaths")] = sequenceArray;
                    }
                    if (footage->frameRate > 0.0) {
                        obj[QStringLiteral("frameRate")] = footage->frameRate;
                    }
                    break;
                }
                case eProjectItemType::Solid: {
                    obj[QStringLiteral("type")] = QStringLiteral("solid");
                    const auto* solid = static_cast<const SolidItem*>(item);
                    obj[QStringLiteral("color")] = solid->color.name(QColor::HexArgb);
                    break;
                }
                case eProjectItemType::Composition: {
                    obj[QStringLiteral("type")] = QStringLiteral("composition");
                    const auto* compItem = static_cast<const CompositionItem*>(item);
                    obj[QStringLiteral("compositionId")] = compItem->compositionId.toString();
                    if (svc) {
                        const auto found = svc->findComposition(compItem->compositionId);
                        auto composition = found.ptr.lock();
                        if (found.success && composition) {
                            obj[QStringLiteral("compositionJson")] = composition->toJson().object();
                        }
                    }
                    break;
                }
                default:
                    obj[QStringLiteral("type")] = QStringLiteral("unknown");
                    break;
                }
                return obj;
            };

            QJsonArray items;
            items.append(serializeItem(contextItem, serializeItem));
            bundle[QStringLiteral("bundleKind")] = QStringLiteral("project-items");
            bundle[QStringLiteral("bundleTitle")] = contextItem->name.toQString();
            bundle[QStringLiteral("sourceProjectName")] = svc ? svc->projectName().toQString() : QString();
            bundle[QStringLiteral("sourceProjectPath")] = ArtifactProjectManager::getInstance().currentProjectPath();
            bundle[QStringLiteral("sourceItemType")] = items.isEmpty()
                ? QStringLiteral("unknown")
                : items.first().toObject().value(QStringLiteral("type")).toString(QStringLiteral("unknown"));
            bundle[QStringLiteral("items")] = items;
            return bundle;
        };
        addTrackedAction(QStringLiteral("copy_item_snapshot"), QStringLiteral("Copy Item Snapshot"), [this, contextItem, svc, buildProjectItemBundle]() {
            const QJsonObject bundle = buildProjectItemBundle();
            if (bundle.isEmpty()) {
                return;
            }
            ClipboardManager::instance().copyProjectBundle(bundle, contextItem->name.toQString());
        }, loadProjectViewIcon(QStringLiteral("Studio/content_copy.svg")));
        addTrackedAction(QStringLiteral("send_item_snapshot"), QStringLiteral("Send Bundle to Main Project"), [this, buildProjectItemBundle]() {
            const QJsonObject bundle = buildProjectItemBundle();
            if (bundle.isEmpty()) {
                return;
            }
            QString error;
            if (!sendProjectBundleToMainProject(bundle, &error)) {
                QMessageBox::warning(this, QStringLiteral("Send Bundle"),
                    error.isEmpty() ? QStringLiteral("Failed to send bundle to the main project.")
                                    : error);
            }
        }, loadProjectViewIcon(QStringLiteral("Studio/upload.svg")));

        QStringList selectedFootagePaths;
        
        if (contextType == eProjectItemType::Composition) {
            addTrackedAction(QStringLiteral("set_active_composition"), QStringLiteral("Set as Active Composition"), [sourceIdx]() {
                QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (idVar.isValid()) {
                    ArtifactProjectService::instance()->changeCurrentComposition(CompositionID(idVar.toString()));
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/composition.svg")));

            {
                auto* responsiveMenu = menu.addMenu(QStringLiteral("Responsive Layout"));
                responsiveMenu->setIcon(loadProjectViewIcon(QStringLiteral("Studio/aspect_ratio.svg")));
                const QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (idVar.isValid() && svc) {
                    const CompositionID compositionId(idVar.toString());
                    const auto found = svc->findComposition(compositionId);
                    if (auto composition = found.ptr.lock()) {
                        const ResponsiveLayoutSet responsiveLayout = composition->responsiveLayout();
                        const QString activeVariantId = composition->activeResponsiveLayoutVariantId();
                        const QSize fallbackSize = composition->effectiveCompositionSize();
                        const ResponsiveLayoutVariant* activeVariant = nullptr;
                        for (const auto& variant : responsiveLayout.variants) {
                            if (variant.variantId == activeVariantId) {
                                activeVariant = &variant;
                                break;
                            }
                        }

                        responsiveMenu->addAction(QStringLiteral("Add Variant..."), [this, compositionId, fallbackSize]() {
                            auto* service = ArtifactProjectService::instance();
                            if (!service) {
                                return;
                            }
                            const auto foundComp = service->findComposition(compositionId);
                            auto comp = foundComp.ptr.lock();
                            if (!foundComp.success || !comp) {
                                return;
                            }
                            ResponsiveLayoutSet layout = comp->responsiveLayout();
                            const ResponsiveLayoutVariant* templateVariant = layout.variants.isEmpty() ? nullptr
                                : &layout.variants.back();
                            if (comp->activeResponsiveLayoutVariantId().isEmpty()) {
                                templateVariant = layout.variants.isEmpty() ? nullptr : &layout.variants.front();
                            } else {
                                for (const auto& candidate : layout.variants) {
                                    if (candidate.variantId == comp->activeResponsiveLayoutVariantId()) {
                                        templateVariant = &candidate;
                                        break;
                                    }
                                }
                            }
                            if (!addResponsiveLayoutVariantDialog(this,
                                                                  &layout,
                                                                  templateVariant,
                                                                  fallbackSize,
                                                                  QStringLiteral("Add Responsive Variant"),
                                                                  false)) {
                                return;
                            }
                            comp->setResponsiveLayout(layout);
                            if (auto project = service->getCurrentProjectSharedPtr()) {
                                project->projectChanged();
                            }
                        });

                        QAction* duplicateAction = responsiveMenu->addAction(QStringLiteral("Duplicate Active Variant..."), [this, compositionId, fallbackSize]() {
                            auto* service = ArtifactProjectService::instance();
                            if (!service) {
                                return;
                            }
                            const auto foundComp = service->findComposition(compositionId);
                            auto comp = foundComp.ptr.lock();
                            if (!foundComp.success || !comp) {
                                return;
                            }
                            ResponsiveLayoutSet layout = comp->responsiveLayout();
                            const QString activeId = comp->activeResponsiveLayoutVariantId();
                            const ResponsiveLayoutVariant* templateVariant = nullptr;
                            for (const auto& candidate : layout.variants) {
                                if (candidate.variantId == activeId) {
                                    templateVariant = &candidate;
                                    break;
                                }
                            }
                            if (!templateVariant) {
                                QMessageBox::information(this,
                                                         QStringLiteral("Responsive Layout"),
                                                         QStringLiteral("There is no active variant to duplicate."));
                                return;
                            }
                            if (!addResponsiveLayoutVariantDialog(this,
                                                                  &layout,
                                                                  templateVariant,
                                                                  fallbackSize,
                                                                  QStringLiteral("Duplicate Responsive Variant"),
                                                                  true)) {
                                return;
                            }
                            comp->setResponsiveLayout(layout);
                            if (auto project = service->getCurrentProjectSharedPtr()) {
                                project->projectChanged();
                            }
                        });
                        duplicateAction->setEnabled(activeVariant != nullptr);
                        responsiveMenu->addSeparator();
                        if (responsiveLayout.variants.isEmpty()) {
                            QAction* action = responsiveMenu->addAction(QStringLiteral("Manual"));
                            action->setEnabled(false);
                        } else {
                            for (const auto& variant : responsiveLayout.variants) {
                                const QString label = responsiveLayoutVariantSummary(variant);
                                QAction* action = responsiveMenu->addAction(label, [compositionId, variant]() {
                                    if (auto* service = ArtifactProjectService::instance()) {
                                        const auto foundComp = service->findComposition(compositionId);
                                        if (auto comp = foundComp.ptr.lock()) {
                                            comp->setActiveResponsiveLayoutVariantId(variant.variantId);
                                        }
                                    }
                                });
                                action->setCheckable(true);
                                action->setChecked(variant.variantId == activeVariantId);
                            }
                            responsiveMenu->addSeparator();
                        }
                        responsiveMenu->addAction(QStringLiteral("Edit Active Variant..."), [this, compositionId]() {
                            auto* service = ArtifactProjectService::instance();
                            if (!service) {
                                return;
                            }
                            const auto foundComp = service->findComposition(compositionId);
                            auto comp = foundComp.ptr.lock();
                            if (!foundComp.success || !comp) {
                                return;
                            }
                            ResponsiveLayoutSet layout = comp->responsiveLayout();
                            const QString activeId = comp->activeResponsiveLayoutVariantId();
                            if (layout.variants.isEmpty() || activeId.isEmpty()) {
                                QMessageBox::information(this,
                                                         QStringLiteral("Responsive Layout"),
                                                         QStringLiteral("This composition has no responsive variant to edit."));
                                return;
                            }
                            if (!editResponsiveLayoutVariantDialog(this, &layout, activeId)) {
                                return;
                            }
                            comp->setResponsiveLayout(layout);
                            if (auto project = service->getCurrentProjectSharedPtr()) {
                                project->projectChanged();
                            }
                        });
                    }
                }
            }

            addTrackedAction(QStringLiteral("composition_settings"), QStringLiteral("Composition Settings..."), [this, sourceIdx]() {
                auto* svc = ArtifactProjectService::instance();
                if (!svc) {
                    return;
                }

                const QVariant idVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                if (!idVar.isValid()) {
                    return;
                }

                const CompositionID compositionId(idVar.toString());
                const auto found = svc->findComposition(compositionId);
                auto composition = found.ptr.lock();
                if (!found.success || !composition) {
                    QMessageBox::warning(this, QStringLiteral("Composition Settings"),
                        QStringLiteral("Could not load the selected composition."));
                    return;
                }

                auto* dialog = new QDialog(this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->setWindowTitle(QStringLiteral("Composition Settings"));
                dialog->setModal(true);
                dialog->resize(360, 260);

                auto* layout = new QVBoxLayout(dialog);
                auto* nameLabel = new QLabel(QStringLiteral("Name"), dialog);
                auto* nameEdit = new QLineEdit(composition->settings().compositionName().toQString(), dialog);
                layout->addWidget(nameLabel);
                layout->addWidget(nameEdit);

                auto* sizeLayout = new QHBoxLayout();
                auto* widthSpin = new QSpinBox(dialog);
                widthSpin->setRange(1, 32768);
                widthSpin->setValue(std::max(1, composition->settings().compositionSize().width()));
                auto* heightSpin = new QSpinBox(dialog);
                heightSpin->setRange(1, 32768);
                heightSpin->setValue(std::max(1, composition->settings().compositionSize().height()));
                sizeLayout->addWidget(new QLabel(QStringLiteral("Width"), dialog));
                sizeLayout->addWidget(widthSpin);
                sizeLayout->addWidget(new QLabel(QStringLiteral("Height"), dialog));
                sizeLayout->addWidget(heightSpin);
                layout->addLayout(sizeLayout);

                auto* fpsLayout = new QHBoxLayout();
                auto* fpsSpin = new QDoubleSpinBox(dialog);
                fpsSpin->setRange(1.0, 240.0);
                fpsSpin->setDecimals(3);
                fpsSpin->setSingleStep(0.5);
                fpsSpin->setValue(std::max(1.0, static_cast<double>(composition->frameRate().framerate())));
                fpsLayout->addWidget(new QLabel(QStringLiteral("Frame Rate"), dialog));
                fpsLayout->addWidget(fpsSpin);
                layout->addLayout(fpsLayout);

                const FrameRange currentRange = composition->frameRange().normalized();
                auto* rangeLayout = new QHBoxLayout();
                auto* startSpin = new QSpinBox(dialog);
                startSpin->setRange(-1000000, 1000000);
                startSpin->setValue(static_cast<int>(currentRange.start()));
                auto* endSpin = new QSpinBox(dialog);
                endSpin->setRange(-1000000, 1000000);
                endSpin->setValue(static_cast<int>(currentRange.end()));
                rangeLayout->addWidget(new QLabel(QStringLiteral("Start"), dialog));
                rangeLayout->addWidget(startSpin);
                rangeLayout->addWidget(new QLabel(QStringLiteral("End"), dialog));
                rangeLayout->addWidget(endSpin);
                layout->addLayout(rangeLayout);

                auto* bgLayout = new QHBoxLayout();
                const QColor originalBackgroundColor = QColor::fromRgbF(
                    composition->backgroundColor().r(),
                    composition->backgroundColor().g(),
                    composition->backgroundColor().b(),
                    composition->backgroundColor().a());
                auto* bgButton = new CompositionBackgroundColorButton(
                    originalBackgroundColor, dialog);
                bgButton->previewChanged = [composition](const QColor& color) {
                    composition->setBackGroundColor(FloatColor(
                        color.redF(), color.greenF(), color.blueF(), color.alphaF()));
                };
                bgLayout->addWidget(new QLabel(QStringLiteral("Background"), dialog));
                bgLayout->addWidget(bgButton);
                bgLayout->addStretch();
                layout->addLayout(bgLayout);

                auto* infoLabel = new QLabel(
                    QStringLiteral("ID: %1").arg(compositionId.toString()), dialog);
                {
                    QPalette pal = infoLabel->palette();
                    pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
                    infoLabel->setPalette(pal);
                }
                layout->addWidget(infoLabel);

                const DialogButtonRow buttons = createWindowsDialogButtonRow(dialog);
                layout->addWidget(buttons.widget);

                QObject::connect(dialog, &QDialog::rejected, dialog, [composition, originalBackgroundColor]() {
                    composition->setBackGroundColor(FloatColor(
                        originalBackgroundColor.redF(),
                        originalBackgroundColor.greenF(),
                        originalBackgroundColor.blueF(),
                        originalBackgroundColor.alphaF()));
                });
                QObject::connect(buttons.cancelButton, &QPushButton::clicked, dialog, &QDialog::reject);
                QObject::connect(buttons.okButton, &QPushButton::clicked, dialog, [this, dialog, svc, compositionId, composition, nameEdit, widthSpin, heightSpin, fpsSpin, startSpin, endSpin, bgButton]() {
                    const QString trimmedName = nameEdit->text().trimmed();
                    if (trimmedName.isEmpty()) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Name must not be empty."));
                        return;
                    }

                    const int startFrame = startSpin->value();
                    const int endFrame = endSpin->value();
                    if (startFrame > endFrame) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Start frame must be less than or equal to end frame."));
                        return;
                    }

                    composition->setCompositionName(UniString::fromQString(trimmedName));
                    {
                        const QSize newSize(widthSpin->value(), heightSpin->value());
                        const QSize oldSize = composition->settings().compositionSize();
                        if (oldSize != newSize) {
                            bool hasMasks = false;
                            bool hasAnchors = false;
                            int maskVerts = 0;
                            int layerCount = 0;
                            for (const auto& layer : composition->allLayerRef()) {
                                if (!layer) continue;
                                ++layerCount;
                                if (layer->hasMasks()) {
                                    hasMasks = true;
                                    for (int mi = 0; mi < layer->maskCount(); ++mi) {
                                        const auto lm = layer->mask(mi);
                                        for (int pi = 0; pi < lm.maskPathCount(); ++pi) {
                                            maskVerts += lm.maskPath(pi).vertexCount();
                                        }
                                    }
                                }
                            }
                            hasAnchors = layerCount > 0;
                            auto impact = ArtifactCore::ResolutionRemap::calculateImpact(
                                oldSize, newSize, hasMasks, maskVerts > 0, hasAnchors);
                            impact.maskVertexCount = maskVerts;

                            Artifact::ArtifactResolutionRemapDialog dialog(oldSize, newSize, impact);
                            if (dialog.exec() == QDialog::Accepted && dialog.remapRequested()) {
                                // remap は Undo 可能なコマンド経由で実行する。
                                // コマンドのコンストラクタが before snapshot を採取し、
                                // push() 内の redo() で applyResolutionRemap を呼ぶ。
                                if (auto* mgr = UndoManager::instance()) {
                                    mgr->push(std::make_unique<ChangeCompositionResolutionCommand>(
                                        composition, oldSize, newSize, dialog.selectedPolicy()));
                                } else {
                                    composition->applyResolutionRemap(newSize, dialog.selectedPolicy());
                                }
                            } else {
                                composition->setCompositionSize(newSize);
                            }
                        } else {
                            composition->setCompositionSize(newSize);
                        }
                    }
                    composition->setFrameRate(FrameRate(static_cast<float>(fpsSpin->value())));
                    composition->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));
                    if (bgButton) {
                        const QColor bg = bgButton->selectedColor();
                        composition->setBackGroundColor(FloatColor(
                            bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF()));
                    }

                    if (!svc->renameComposition(compositionId, UniString::fromQString(trimmedName))) {
                        QMessageBox::warning(dialog, QStringLiteral("Composition Settings"),
                            QStringLiteral("Failed to update composition name."));
                        return;
                    }

                    if (auto project = svc->getCurrentProjectSharedPtr()) {
                        project->projectChanged();
                    }
                    if (auto current = svc->currentComposition().lock()) {
                        if (current->id() == compositionId) {
                            if (auto* playback = ArtifactPlaybackService::instance()) {
                                playback->setFrameRange(composition->frameRange());
                                playback->setFrameRate(composition->frameRate());
                            }
                        }
                    }

                    dialog->accept();
                });

                dialog->exec();
            }, loadProjectViewIcon(QStringLiteral("Studio/settings.svg")));
            
            addTrackedAction(QStringLiteral("interpret_footage"), QStringLiteral("Interpret Footage..."), [this, svc, selectedFootagePaths, contextItem]() {
                auto* projectService = svc ? svc : ArtifactProjectService::instance();
                if (!projectService) {
                    return;
                }

                QStringList targetPaths = selectedFootagePaths;
                if (targetPaths.isEmpty() && contextItem && contextItem->type() == eProjectItemType::Footage) {
                    targetPaths.append(QFileInfo(static_cast<FootageItem*>(contextItem)->filePath).absoluteFilePath());
                }
                targetPaths.removeDuplicates();
                if (targetPaths.isEmpty()) {
                    return;
                }

                const QString firstPath = targetPaths.first();
                double currentFrameRate = 24.0;
                if (auto* firstFootage = projectService->findFootageItemByPath(firstPath)) {
                    if (firstFootage->frameRate > 0.0) {
                        currentFrameRate = firstFootage->frameRate;
                    }
                }

                const auto impact = assessFootageFrameRateChange(projectService, firstPath);
                const int timeRemapCount = std::count_if(impact.begin(), impact.end(),
                    [](const FootageImpactRow& r) { return r.hasTimeRemap; });

                auto* dialog = new QDialog(this);
                dialog->setWindowTitle(QStringLiteral("Interpret Footage"));
                dialog->setModal(true);
                dialog->setMinimumWidth(420);

                auto* layout = new QVBoxLayout(dialog);

                auto* infoLabel = new QLabel(
                    QStringLiteral("Change how the selected footage is interpreted. "
                                   "This updates source frame rate metadata and may affect downstream time remap."),
                    dialog);
                infoLabel->setWordWrap(true);
                layout->addWidget(infoLabel);

                auto* fpsRow = new QHBoxLayout();
                fpsRow->addWidget(new QLabel(QStringLiteral("Source frame rate"), dialog));
                auto* fpsSpin = new QDoubleSpinBox(dialog);
                fpsSpin->setRange(1.0, 240.0);
                fpsSpin->setDecimals(3);
                fpsSpin->setSingleStep(0.5);
                fpsSpin->setValue(currentFrameRate);
                fpsRow->addWidget(fpsSpin, 1);
                layout->addLayout(fpsRow);

                auto* preserveRow = new QHBoxLayout();
                preserveRow->addWidget(new QLabel(QStringLiteral("Preserve mode"), dialog));
                auto* preserveCombo = new QComboBox(dialog);
                preserveCombo->addItem(QStringLiteral("Keep Keyframes"));
                preserveCombo->addItem(QStringLiteral("Keep Time"));
                preserveCombo->addItem(QStringLiteral("Re-sample"));
                preserveRow->addWidget(preserveCombo, 1);
                layout->addLayout(preserveRow);

                // Dynamic warning area
                auto* warningLabel = new QLabel(dialog);
                warningLabel->setWordWrap(true);
                if (impact.isEmpty()) {
                    warningLabel->setText(QStringLiteral(
                        "No downstream layers found. Frame rate change is safe."));
                } else {
                    QSet<QString> compNames;
                    for (const auto& r : impact) compNames.insert(r.compositionName);
                    QString warn = QStringLiteral("Affects %1 layer(s) in %2 comp(s).")
                        .arg(impact.size()).arg(compNames.size());
                    if (timeRemapCount > 0) {
                        warn += QStringLiteral("\n⚠ %1 layer(s) have time remap enabled — "
                            "changing frame rate may shift keyframe positions.")
                            .arg(timeRemapCount);
                    }
                    warningLabel->setText(warn);
                }
                QPalette warningPal = warningLabel->palette();
                warningPal.setColor(QPalette::WindowText, timeRemapCount > 0
                    ? QColor(QStringLiteral("#ffcc66"))
                    : QColor(QStringLiteral("#88cc88")));
                warningLabel->setPalette(warningPal);
                warningLabel->setContentsMargins(4, 4, 4, 4);
                layout->addWidget(warningLabel);

                auto* buttons = new QDialogButtonBox(
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                    Qt::Horizontal,
                    dialog);
                layout->addWidget(buttons);
                connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

                if (dialog->exec() != QDialog::Accepted) {
                    return;
                }

                const double frameRate = fpsSpin->value();
                const QString preserveMode = preserveCombo->currentText();
                const int changedCount = static_cast<int>(targetPaths.size());
                bool updatedAny = false;

                for (const QString& path : targetPaths) {
                    auto* footage = projectService->findFootageItemByPath(path);
                    if (!footage) continue;
                    footage->frameRate = frameRate;
                    footage->isSequence = footage->isSequence || footage->sequencePaths.size() > 1;
                    updatedAny = true;
                }

                // Notify downstream video layers of frame rate change
                for (const QString& path : targetPaths) {
                    const auto pathImpact = assessFootageFrameRateChange(projectService, path);
                    if (pathImpact.isEmpty()) continue;

                    auto project = projectService->getCurrentProjectSharedPtr();
                    if (!project) continue;

                    for (const auto& row : pathImpact) {
                        CompositionID compId;
                        {
                            const auto items = project->projectItems();
                            std::function<void(ProjectItem*)> findComp = [&](ProjectItem* item) {
                                if (!item || compId) return;
                                if (item->type() == eProjectItemType::Composition) {
                                    auto* ci = static_cast<CompositionItem*>(item);
                                    if (ci->name.toQString() == row.compositionName)
                                        compId = ci->compositionId;
                                }
                                for (auto* child : item->children) findComp(child);
                            };
                            for (auto* root : items) findComp(root);
                        }
                        if (!compId) continue;

                        auto compResult = project->findComposition(compId);
                        auto comp = compResult.ptr.lock();
                        if (!comp) continue;
                        const auto& layers = comp->allLayerRef();
                        for (const auto& layerPtr : layers) {
                            if (!layerPtr || layerPtr->layerName() != row.layerName) continue;
                            if (auto* vl = dynamic_cast<ArtifactVideoLayer*>(layerPtr.get())) {
                                vl->setStreamFrameRate(frameRate);
                            }
                        }
                    }
                }

                if (updatedAny) {
                    projectService->projectChanged();
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/info.svg")));
        }

        if (contextType == eProjectItemType::Footage) {
            const QString footagePath = contextItem && contextItem->type() == eProjectItemType::Footage
                ? static_cast<FootageItem*>(contextItem)->filePath
                : QString();
            if (selectionModel()) {
                const auto rows = selectionModel()->selectedRows(0);
                QSet<QString> seen;
                for (const auto& row : rows) {
                    QModelIndex sourceIdx = row;
                    if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                        sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
                    }
                    const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
                    if (!typeVar.isValid() || typeVar.toInt() != static_cast<int>(eProjectItemType::Footage)) {
                        continue;
                    }
                    const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                    auto* item2 = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                    if (!item2 || item2->type() != eProjectItemType::Footage) {
                        continue;
                    }
                    const QString path = QFileInfo(static_cast<FootageItem*>(item2)->filePath).absoluteFilePath();
                    if (path.isEmpty() || seen.contains(path)) {
                        continue;
                    }
                    seen.insert(path);
                    selectedFootagePaths.push_back(path);
                }
            }
            addTrackedAction(QStringLiteral("preview_in_contents_viewer"), QStringLiteral("Preview in Contents Viewer"), [this, idx]() {
                itemDoubleClicked(idx);
            }, loadProjectViewIcon(QStringLiteral("Studio/visibility.svg")));
            addTrackedAction(QStringLiteral("generate_proxy_selected"), QStringLiteral("Generate Proxy"), [this, footagePath]() {
                if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                    manager->generateProxyForFilePath(footagePath);
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
            addTrackedAction(QStringLiteral("reveal_proxy"), QStringLiteral("Reveal Proxy"), [this, footagePath]() {
                if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                    manager->revealProxyForFilePath(footagePath);
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/folder.svg")));
            addTrackedAction(QStringLiteral("clear_proxy"), QStringLiteral("Clear Proxy"), [this, footagePath]() {
                if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                    manager->clearProxyForFilePath(footagePath);
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/delete.svg")));
            if (selectedFootagePaths.size() > 1) {
                addTrackedAction(QStringLiteral("generate_selected_proxies"), QStringLiteral("Generate Selected Proxies"), [this, selectedFootagePaths]() {
                    if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                        for (const QString& path : selectedFootagePaths) {
                            manager->generateProxyForFilePath(path);
                        }
                    }
                }, loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
                addTrackedAction(QStringLiteral("clear_selected_proxies"), QStringLiteral("Clear Selected Proxies"), [this, selectedFootagePaths]() {
                    if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                        for (const QString& path : selectedFootagePaths) {
                            manager->clearProxyForFilePath(path);
                        }
                    }
                }, loadProjectViewIcon(QStringLiteral("Studio/delete.svg")));
                addTrackedAction(QStringLiteral("regenerate_stale_proxies"), QStringLiteral("Re-generate Stale Proxies"), [this, selectedFootagePaths]() {
                    if (auto* manager = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
                        for (const QString& path : selectedFootagePaths) {
                            const QString proxyPath = proxyFilePathForFootage(path);
                            if (proxyPath.isEmpty()) continue;
                            const bool exists = QFileInfo(proxyPath).exists();
                            bool stale = false;
                            if (exists) {
                                const auto it = proxyMetadata().constFind(path);
                                if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                                    const QFileInfo src(path);
                                    stale = src.exists() && src.lastModified() > it->sourceLastModified;
                                }
                            }
                            if (!exists || stale) {
                                manager->generateProxyForFilePath(path);
                            }
                        }
                    }
                }, loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
            }
            addTrackedAction(QStringLiteral("reveal_in_explorer"), QStringLiteral("Reveal in Explorer (R)"), [contextItem]() {
                if (contextItem && contextItem->type() == eProjectItemType::Footage) {
                    QString path = static_cast<FootageItem*>(contextItem)->filePath;
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/folder_open.svg")));
            addTrackedAction(QStringLiteral("copy_file_path"), QStringLiteral("Copy File Path"), [contextItem]() {
                if (contextItem && contextItem->type() == eProjectItemType::Footage) {
                    QApplication::clipboard()->setText(static_cast<FootageItem*>(contextItem)->filePath);
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/content_copy.svg")));
            addTrackedAction(QStringLiteral("relink_selected_footage"), QStringLiteral("Relink Selected Footage..."), [this, contextItem, svc]() {
                if (!svc || !contextItem || contextItem->type() != eProjectItemType::Footage) return;
                const QString root = QFileDialog::getExistingDirectory(this, "Relink Selected Footage - Search Root");
                if (root.isEmpty()) return;
                QVector<FootageItem*> targets;
                targets.append(static_cast<FootageItem*>(contextItem));
                const int relinked = Impl::relinkMissingFootage(root, targets);
                if (relinked > 0) {
                    svc->projectChanged();
                }
                QMessageBox::information(this, "Relink Result",
                                         QString("Relinked %1 file(s).").arg(relinked));
            }, loadProjectViewIcon(QStringLiteral("Studio/link.svg")));
        }

        auto& clipboard = ClipboardManager::instance();
        clipboard.syncFromSystemClipboard();
        if (clipboard.hasProjectItemData()) {
            auto resolvePasteParent = [svc, contextItem]() -> ProjectItem* {
                ProjectItem* target = contextItem;
                if (target && target->type() != eProjectItemType::Folder) {
                    target = target->parent;
                }
                if (target) {
                    return target;
                }
                if (!svc) {
                    return nullptr;
                }
                auto project = svc->getCurrentProjectSharedPtr();
                if (!project) {
                    return nullptr;
                }
                const auto roots = project->projectItems();
                for (auto* root : roots) {
                    if (root && root->type() == eProjectItemType::Folder) {
                        return root;
                    }
                }
                return nullptr;
            };
            addTrackedAction(QStringLiteral("paste_project_items"), QStringLiteral("Paste Items Here"), [this, svc, resolvePasteParent]() {
                if (!svc) {
                    return;
                }
                auto& clipboard = ClipboardManager::instance();
                const QJsonArray items = clipboard.pasteProjectItems();
                if (items.isEmpty()) {
                    return;
                }
                auto project = svc->getCurrentProjectSharedPtr();
                if (!project) {
                    return;
                }
                ProjectItem* targetParent = resolvePasteParent();
                if (!project->addProjectItemsFromJson(items, targetParent)) {
                    QMessageBox::warning(this, QStringLiteral("Paste Items"),
                        QStringLiteral("Could not paste the copied project items."));
                }
            }, loadProjectViewIcon(QStringLiteral("Studio/content_paste.svg")));
        }

        menu.addSeparator();
        addTrackedAction(QStringLiteral("rename"), QStringLiteral("Rename"), [this, idx]() {
            bool ok;
            QString name = QInputDialog::getText(this, "Rename", "New Name:", QLineEdit::Normal, idx.data().toString(), &ok);
            if(ok && !name.isEmpty()) {
                 QModelIndex sourceIdx = idx;
                 if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                     sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
                 }
                 QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                 ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                 if (!renameProjectItem(item, name)) {
                     QMessageBox::warning(this, QStringLiteral("Rename Failed"),
                        QStringLiteral("Could not rename the selected project item."));
                }
            }
        }, loadProjectViewIcon(QStringLiteral("Studio/edit.svg")));

        QMenu* moveToFolderMenu = menu.addMenu(QStringLiteral("Move to Folder"));
        moveToFolderMenu->setIcon(loadProjectViewIcon(QStringLiteral("Studio/folder.svg")));
        bool hasMoveTarget = false;
        if (svc && contextItem) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                QVector<FolderItem*> folders;
                const auto roots = project->projectItems();
                for (auto* root : roots) {
                    collectFolders(root, folders);
                }
                for (auto* folder : folders) {
                    if (!folder) {
                        continue;
                    }
                    QAction* moveAction = moveToFolderMenu->addAction(folderDisplayPath(folder));
                    moveAction->setIcon(loadProjectViewIcon(QStringLiteral("Studio/folder.svg")));
                    const bool canMove = (folder != contextItem) && !isDescendantOf(folder, contextItem);
                    moveAction->setEnabled(canMove);
                    if (!canMove) {
                        continue;
                    }
                    hasMoveTarget = true;
                    QObject::connect(moveAction, &QAction::triggered, this, [this, svc, contextItem, folder]() {
                        if (!svc || !contextItem || !folder) {
                            return;
                        }
                        if (!svc->moveProjectItem(contextItem, folder)) {
                            QMessageBox::warning(this, QStringLiteral("Move Failed"),
                                QStringLiteral("Could not move the selected item to the target folder."));
                        }
                    });
                }
            }
        }
        if (!hasMoveTarget) {
            QAction* emptyAction = moveToFolderMenu->addAction(QStringLiteral("(No valid target folder)"));
            emptyAction->setIcon(loadProjectViewIcon(QStringLiteral("Studio/help.svg")));
            emptyAction->setEnabled(false);
        }

        addTrackedAction(QStringLiteral("delete"), QStringLiteral("Delete"), [this, contextItem, svc]() {
            if (!svc || !contextItem) {
                return;
            }
            const QString message = svc->projectItemRemovalConfirmationMessage(contextItem);
            if (!ArtifactMessageBox::confirmDelete(this, QStringLiteral("項目削除"), message)) {
                return;
            }
            if (!svc->removeProjectItem(contextItem)) {
                QMessageBox::warning(this, QStringLiteral("削除失敗"),
                    QStringLiteral("項目の削除に失敗しました。"));
            }
        }, loadProjectViewIcon(QStringLiteral("Studio/delete.svg")));

        menu.addSeparator();
        addTrackedAction(QStringLiteral("expand_children"), QStringLiteral("Expand Children"), [this, idx]() {
            setExpanded(idx, true);
        }, loadProjectViewIcon(QStringLiteral("Studio/visibility.svg")));
        addTrackedAction(QStringLiteral("collapse_children"), QStringLiteral("Collapse Children"), [this, idx]() {
            setExpanded(idx, false);
        }, loadProjectViewIcon(QStringLiteral("Studio/visibility_off.svg")));
        
        menu.addSeparator();
    }

    // "New" menu group
    auto newMenu = menu.addMenu("New");
    newMenu->setIcon(loadProjectViewIcon(QStringLiteral("Studio/add_circle.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_composition"), QStringLiteral("Composition..."), [this]() {
         auto dialog = new CreateCompositionDialog(this);
         if (dialog->exec()) {
             const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
             if (auto* svc = ArtifactProjectService::instance()) {
                 svc->createComposition(params);
             }
         }
         dialog->deleteLater();
    }, loadProjectViewIcon(QStringLiteral("Studio/composition.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_parametric_composition"), QStringLiteral("Parametric Composition"), [this, svc]() {
        if (!svc) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            QMessageBox::warning(this, QStringLiteral("Parametric Composition"),
                                 QStringLiteral("プロジェクトが開かれていません。"));
            return;
        }

        bool accepted = false;
        QString name = QInputDialog::getText(this,
            QStringLiteral("Parametric Composition"),
            QStringLiteral("Name"),
            QLineEdit::Normal,
            QStringLiteral("Parametric Composition"),
            &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (name.isEmpty()) {
            name = QStringLiteral("Parametric Composition");
        }

        const CompositionID compositionId;
        auto definition = makeDefaultParametricCompositionDefinition(
            QStringLiteral("parametric.%1").arg(compositionId.toString()),
            name,
            QStringLiteral("input"),
            QStringLiteral("output"));
        QJsonObject compositionJson = definition.toJson();
        compositionJson.insert(QStringLiteral("id"), compositionId.toString());
        compositionJson.insert(QStringLiteral("name"), name);
        compositionJson.insert(QStringLiteral("width"), 1920);
        compositionJson.insert(QStringLiteral("height"), 1080);
        compositionJson.insert(QStringLiteral("bundleKind"), QStringLiteral("parametric-composition"));
        compositionJson.insert(QStringLiteral("parametricDefinition"), definition.toJson());

        QJsonObject item;
        item.insert(QStringLiteral("type"), QStringLiteral("composition"));
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("compositionId"), compositionId.toString());
        item.insert(QStringLiteral("compositionJson"), compositionJson);

        const bool ok = project->addProjectItemsFromJson(QJsonArray{item}, nullptr);
        if (ok) {
            project->setCurrentCompositionId(compositionId, false);
            project->projectChanged();
        } else {
            QMessageBox::warning(this, QStringLiteral("Parametric Composition"),
                                 QStringLiteral("パラメトリックコンポジションを作成できませんでした。"));
        }
    }, loadProjectViewIcon(QStringLiteral("Studio/composition.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_solid"), QStringLiteral("Solid..."), [this, svc]() {
        if (!svc) return;
        if (!svc->currentComposition().lock()) {
            // Try to create a comp first if none exists
            if (svc->hasProject()) {
                svc->createComposition(UniString(QStringLiteral("Composition")));
            }
        }
        if (!svc->currentComposition().lock()) {
            QMessageBox::warning(this, "Layer", "コンポジションが選択されていません。");
            return;
        }
        CreateSolidLayerSettingDialog dialog(this);
        dialog.setModal(true);
        if (dialog.exec() == QDialog::Accepted && svc) {
            svc->addLayerToCurrentComposition(
                dialog.submittedParams(), true,
                dialog.submittedPlacementMode() == LayerCreationPlacementMode::Playhead);
        }
    }, loadProjectViewIcon(QStringLiteral("Studio/palette.svg")));
    addTrackedNewAction(newMenu, QStringLiteral("new_folder"), QStringLiteral("Folder"), [this]() {
        impl_->createFolderAtSelection(this);
    }, loadProjectViewIcon(QStringLiteral("Studio/folder.svg")));

    menu.addSeparator();
    addTrackedAction(QStringLiteral("expand_all"), QStringLiteral("Expand All"), [this]() { expandAll(); },
                     loadProjectViewIcon(QStringLiteral("Studio/visibility.svg")));
    addTrackedAction(QStringLiteral("collapse_all"), QStringLiteral("Collapse All"), [this]() { collapseAll(); },
                     loadProjectViewIcon(QStringLiteral("Studio/visibility_off.svg")));
    addTrackedAction(QStringLiteral("refresh_view"), QStringLiteral("Refresh View"), [this]() { update(); },
                     loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
    addTrackedAction(QStringLiteral("show_dependency_graph"), QStringLiteral("Show Dependency Graph..."), [this, svc]() {
        Impl::showDependencyGraphDialog(this, svc);
    }, loadProjectViewIcon(QStringLiteral("Studio/link.svg")));
    menu.addSeparator();
    addTrackedAction(QStringLiteral("relink_missing_footage"), QStringLiteral("Relink Missing Footage..."), [this, svc]() {
        if (!svc) return;
        const QString root = QFileDialog::getExistingDirectory(this, "Relink Missing Footage - Search Root");
        if (root.isEmpty()) return;

        QVector<FootageItem*> targets;
        const auto roots = svc->projectItems();
        for (auto* r : roots) {
            Impl::collectFootage(r, targets);
        }

        const int relinked = Impl::relinkMissingFootage(root, targets);
        if (relinked > 0) {
            svc->projectChanged();
        }
        QMessageBox::information(this, "Relink Result",
                                 QString("Relinked %1 missing footage item(s).").arg(relinked));
    }, loadProjectViewIcon(QStringLiteral("Studio/link.svg")));
    menu.addSeparator();
    addTrackedAction(QStringLiteral("import_file"), QStringLiteral("Import File..."), [this, svc]() {
        Q_UNUSED(svc);
        QStringList paths = QFileDialog::getOpenFileNames(this, "Import Files", "", "All Files (*.*)");
        for (const auto& p : paths) {
             impl_->handleFileDrop(p);
        }
    }, loadProjectViewIcon(QStringLiteral("Studio/file_open.svg")));

    if (!impl_->lastContextCommandId.isEmpty() && availableContextCommands.contains(impl_->lastContextCommandId)) {
        QAction* firstAction = menu.actions().isEmpty() ? nullptr : menu.actions().first();
        QAction* separator = firstAction ? menu.insertSeparator(firstAction) : menu.addSeparator();
        const QString repeatLabel = QStringLiteral("Repeat Last Command: %1").arg(
            availableContextLabels.value(impl_->lastContextCommandId, impl_->lastContextCommandLabel));
        QAction* repeatAction = new QAction(repeatLabel, &menu);
        repeatAction->setIcon(loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
        const QString commandId = impl_->lastContextCommandId;
        const QString commandLabel = availableContextLabels.value(commandId, impl_->lastContextCommandLabel);
        QObject::connect(repeatAction, &QAction::triggered, &menu, [this, commandId, commandLabel, run = availableContextCommands.value(commandId)]() mutable {
            impl_->lastContextCommandId = commandId;
            impl_->lastContextCommandLabel = commandLabel;
            run();
        });
        if (separator) {
            menu.insertAction(separator, repeatAction);
        } else {
            menu.addAction(repeatAction);
        }
    }

    if (!impl_->lastNewCommandId.isEmpty() && availableNewCommands.contains(impl_->lastNewCommandId)) {
        QAction* firstNewAction = newMenu->actions().isEmpty() ? nullptr : newMenu->actions().first();
        QAction* separator = firstNewAction ? newMenu->insertSeparator(firstNewAction) : newMenu->addSeparator();
        const QString repeatLabel = QStringLiteral("Repeat Last New Command: %1").arg(
            availableNewLabels.value(impl_->lastNewCommandId, impl_->lastNewCommandLabel));
        QAction* repeatAction = new QAction(repeatLabel, newMenu);
        repeatAction->setIcon(loadProjectViewIcon(QStringLiteral("Studio/replay.svg")));
        const QString commandId = impl_->lastNewCommandId;
        const QString commandLabel = availableNewLabels.value(commandId, impl_->lastNewCommandLabel);
        QObject::connect(repeatAction, &QAction::triggered, newMenu, [this, commandId, commandLabel, run = availableNewCommands.value(commandId)]() mutable {
            impl_->lastNewCommandId = commandId;
            impl_->lastNewCommandLabel = commandLabel;
            run();
        });
        if (separator) {
            newMenu->insertAction(separator, repeatAction);
        } else {
            newMenu->addAction(repeatAction);
        }
    }

    menu.exec(event->globalPos());
}

void ArtifactProjectView::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    auto mapToSourceColumn0 = [](const QModelIndex& idx) -> QModelIndex {
        if (!idx.isValid()) {
            return {};
        }
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
            return proxy->mapToSource(idx).siblingAtColumn(0);
        }
        return idx.siblingAtColumn(0);
    };
    auto itemFromIndex = [&](const QModelIndex& idx) -> ProjectItem* {
        const QModelIndex src = mapToSourceColumn0(idx);
        const QVariant ptrVar = src.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        return ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
    };

    // Internal DnD move: reparent the currently selected item to a folder.
    const bool isInternalDnD = (event->source() == this)
        && mimeData->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            event->ignore();
            return;
        }

        QModelIndexList selectedRows;
        if (selectionModel()) {
            selectedRows = selectionModel()->selectedRows(0);
        }
        if (selectedRows.isEmpty()) {
            QModelIndex curr = currentIndex();
            if (curr.isValid()) {
                selectedRows.append(curr);
            }
        }

        if (selectedRows.isEmpty()) {
            event->ignore();
            return;
        }

        const QModelIndex targetIndex = indexAt(event->position().toPoint());
        ProjectItem* targetItem = itemFromIndex(targetIndex);
        ProjectItem* targetFolder = nullptr;

        if (!targetItem) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                const auto roots = project->projectItems();
                for (auto* root : roots) {
                    if (root && root->type() == eProjectItemType::Folder) {
                        targetFolder = root;
                        break;
                    }
                }
            }
        } else if (targetItem->type() == eProjectItemType::Folder) {
            targetFolder = targetItem;
        } else {
            targetFolder = targetItem->parent;
        }

        if (!targetFolder) {
            event->ignore();
            return;
        }

        bool movedAny = false;
        for (const QModelIndex& idx : selectedRows) {
            ProjectItem* item = itemFromIndex(idx);
            if (!item || item == targetFolder || isDescendantOf(targetFolder, item)) {
                continue;
            }
            if (svc->moveProjectItem(item, targetFolder)) {
                movedAny = true;
            }
        }

        if (!movedAny) {
            event->ignore();
            return;
        }

        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    if (!mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    QStringList importTargets;
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        collectImportablePaths(url.toLocalFile(), importTargets);
    }
    importTargets.removeDuplicates();
    if (importTargets.isEmpty()) {
        event->ignore();
        return;
    }

    if (auto* svc = ArtifactProjectService::instance()) {
        svc->importAssetsFromPaths(importTargets);
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ArtifactProjectView::dragEnterEvent(QDragEnterEvent* event) {
    const bool isInternalDnD = (event->source() == this)
        && event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::dragMoveEvent(QDragMoveEvent* event) {
    const bool isInternalDnD = (event->source() == this)
        && event->mimeData()->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    if (isInternalDnD) {
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else event->ignore();
}

void ArtifactProjectView::mousePressEvent(QMouseEvent* event) {
    if (!impl_) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (impl_->hoverTimer) {
        impl_->hoverTimer->stop();
    }
    if (impl_->hoverPopup) impl_->hoverPopup->hide();
    if (impl_->nameEditor && impl_->nameEditor->isVisible()) { impl_->nameEditor->hide(); impl_->editingIndex = QModelIndex(); update(); }
    const QPoint mousePos = event->position().toPoint();
    if (!impl_->isTileMode() && mousePos.y() < impl_->headerHeight) {
        const HeaderResizeHit resizeHit = headerResizeHit(impl_->columnWidths, mousePos, impl_->headerHeight);
        if (resizeHit.column >= 0) {
            impl_->resizingColumn = resizeHit.column;
            impl_->resizeStartX = resizeHit.boundaryX;
            setCursor(Qt::SplitHCursor);
            event->accept();
            return;
        }

        int x = 0;
        for (int i = 0; i < impl_->columnWidths.size(); ++i) {
            const int width = impl_->columnWidths[i];
            const QRect hr(x, 0, width, impl_->headerHeight);
            if (hr.contains(mousePos)) {
                if (impl_->sortingEnabled) { impl_->sortOrder = (impl_->sortColumn == i && impl_->sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder; impl_->sortColumn = i; sortByColumn(i, impl_->sortOrder); }
                return;
            }
            x += width;
        }
    }
    if (event->button() == Qt::LeftButton) {
        impl_->dragStartPos = mousePos;
        const QModelIndex idx = indexAt(mousePos);
        if (idx.isValid()) {
            QModelIndex sourceIdx = idx;
            if (auto* proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
            }
            const QVariant typeVar = sourceIdx.data(
                Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            const bool isComposition =
                typeVar.isValid() &&
                typeVar.toInt() == static_cast<int>(eProjectItemType::Composition);
            const bool alreadyCurrent =
                selectionModel() && selectionModel()->currentIndex().siblingAtColumn(0) == idx.siblingAtColumn(0);
            if (isComposition && alreadyCurrent && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                QRect nameRect;
                if (impl_->isTileMode()) {
                    nameRect = impl_->tileTitleRect(visualRect(idx));
                } else {
                    const QRect rowRect = visualRect(idx);
                    const int row = impl_->rowForIndex(idx);
                    const int depth = (row >= 0 && row < impl_->visibleRows.size()) ? impl_->visibleRows[row].depth : 0;
                    int textLeft = 8 + depth * impl_->indentWidth + (impl_->hasChildren(idx) ? 18 : 0);
                    if (idx.siblingAtColumn(0).data(Qt::DecorationRole).canConvert<QIcon>()) {
                        textLeft += 22;
                    }
                    nameRect = QRect(textLeft, rowRect.top(), std::max(48, impl_->columnWidths.value(0, 180) - textLeft - 8), rowRect.height());
                }
                if (nameRect.contains(mousePos)) {
                    editIndex(idx);
                    event->accept();
                    return;
                }
            }
            const QRect rowRect = visualRect(idx);
            const int row = impl_->rowForIndex(idx);
            const int depth = (row >= 0 && row < impl_->visibleRows.size()) ? impl_->visibleRows[row].depth : 0;
            const QRect branchRect(8 + depth * impl_->indentWidth, rowRect.top(), 20, rowRect.height());
            if (impl_->hasChildren(idx) && branchRect.contains(mousePos)) { setExpanded(idx, !impl_->isExpanded(idx)); return; }
            if (selectionModel()) {
                if (event->modifiers() & Qt::ControlModifier) selectionModel()->select(idx, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
                else if (event->modifiers() & Qt::ShiftModifier) {
                    QModelIndex curr = selectionModel()->currentIndex();
                    int s = impl_->rowForIndex(curr), e = impl_->rowForIndex(idx);
                    if (s != -1 && e != -1) { QItemSelection sel; for (int i = std::min(s, e); i <= std::max(s, e); ++i) sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0); selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows); }
                } else selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
            return;
        }
        if (selectionModel()) selectionModel()->clearSelection();
        update();
    }
    QWidget::mousePressEvent(event);
}

void ArtifactProjectView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_->resizingColumn != -1) { impl_->resizingColumn = -1; unsetCursor(); return; }
    QWidget::mouseReleaseEvent(event);
}

void ArtifactProjectView::focusInEvent(QFocusEvent* event)
{
    if (auto* input = InputOperator::instance()) {
        input->setActiveContext(QString::fromLatin1(kProjectContext));
    }
    QWidget::focusInEvent(event);
}

void ArtifactProjectView::focusOutEvent(QFocusEvent* event)
{
    if (auto* input = InputOperator::instance()) {
        if (input->activeContext() == QString::fromLatin1(kProjectContext)) {
            input->setActiveContext(QStringLiteral("Global"));
        }
    }
    QWidget::focusOutEvent(event);
}

 void ArtifactProjectView::keyPressEvent(QKeyEvent* event)
 {
     if (!impl_ || impl_->visibleRows.isEmpty()) { QWidget::keyPressEvent(event); return; }
     if (auto* input = InputOperator::instance()) {
         input->setActiveContext(QString::fromLatin1(kProjectContext));
         if (event && input->processKeyPress(this, event->key(), event->modifiers())) {
             event->accept();
             return;
         }
     }
     if (event->key() == Qt::Key_F2) { if (currentIndex().isValid()) editIndex(currentIndex()); return; }
     
     // Ctrl+A で全選択
     if (event->matches(QKeySequence::SelectAll)) {
         if (selectionModel()) {
             QItemSelection selection;
             for (const auto& row : impl_->visibleRows) {
                 selection.select(row.index0, row.index0);
             }
             selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
         event->accept();
         return;
     }

     const bool shift = event->modifiers() & Qt::ShiftModifier;

     if (event->key() == Qt::Key_Home && !shift) {
         if (!impl_->visibleRows.isEmpty()) {
             setCurrentIndex(impl_->visibleRows.front().index0);
             itemSelected(impl_->visibleRows.front().index0);
         }
         event->accept();
         return;
     }
     if (event->key() == Qt::Key_End && !shift) {
         if (!impl_->visibleRows.isEmpty()) {
             const auto& last = impl_->visibleRows.back();
             setCurrentIndex(last.index0);
             itemSelected(last.index0);
         }
         event->accept();
         return;
     }
     
     // R キーで選択フッテージをエクスプローラーで表示
     if (event->key() == Qt::Key_R) {
         QModelIndex idx = currentIndex();
         if (idx.isValid()) {
             QModelIndex sourceIdx = idx;
             if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(idx.model())) {
                 sourceIdx = proxy->mapToSource(idx).siblingAtColumn(0);
             }
             QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
             ProjectItem* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
             if (item && item->type() == eProjectItemType::Footage) {
                 QString path = static_cast<FootageItem*>(item)->filePath;
                 QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
                 event->accept();
                 return;
             }
         }
     }

     // * で全て展開 / Shift+* で全て折りたたみ
     if (event->key() == Qt::Key_Asterisk && !shift) {
         expandAll();
         event->accept();
         return;
     }
     if (event->key() == Qt::Key_Asterisk && shift) {
         collapseAll();
         event->accept();
         return;
     }
     
     QModelIndex target = currentIndex();
     int currRow = impl_->rowForIndex(target);
     if (currRow < 0) { currRow = 0; target = impl_->visibleRows.front().index0; }
     const int vRows = (height() - impl_->headerHeight) / impl_->rowHeight;
     const int lastRow = static_cast<int>(impl_->visibleRows.size()) - 1;
     switch (event->key()) {
     case Qt::Key_Up: {
         int next = std::max(0, currRow - 1);
         target = impl_->visibleRows[next].index0;
         if (shift && selectionModel()) {
             QItemSelection sel;
             const int from = std::min(currRow, next);
             const int to = std::max(currRow, next);
             for (int i = from; i <= to; ++i) {
                 sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0);
             }
             selectionModel()->select(sel, QItemSelectionModel::Select | QItemSelectionModel::Rows);
         } else if (selectionModel()) {
             selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
         itemSelected(target);
         event->accept();
         return;
     }
     case Qt::Key_Down: {
         int next = std::min(currRow + 1, lastRow);
         target = impl_->visibleRows[next].index0;
         if (shift && selectionModel()) {
             QItemSelection sel;
             const int from = std::min(currRow, next);
             const int to = std::max(currRow, next);
             for (int i = from; i <= to; ++i) {
                 sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0);
             }
             selectionModel()->select(sel, QItemSelectionModel::Select | QItemSelectionModel::Rows);
         } else if (selectionModel()) {
             selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
         itemSelected(target);
         event->accept();
         return;
     }
     case Qt::Key_PageUp: {
         int next = std::max(0, currRow - vRows);
         target = impl_->visibleRows[next].index0;
         if (shift && selectionModel()) {
             QItemSelection sel;
             const int from = std::min(currRow, next);
             const int to = std::max(currRow, next);
             for (int i = from; i <= to; ++i) {
                 sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0);
             }
             selectionModel()->select(sel, QItemSelectionModel::Select | QItemSelectionModel::Rows);
         } else if (selectionModel()) {
             selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
         itemSelected(target);
         event->accept();
         return;
     }
     case Qt::Key_PageDown: {
         int next = std::min(currRow + vRows, lastRow);
         target = impl_->visibleRows[next].index0;
         if (shift && selectionModel()) {
             QItemSelection sel;
             const int from = std::min(currRow, next);
             const int to = std::max(currRow, next);
             for (int i = from; i <= to; ++i) {
                 sel.select(impl_->visibleRows[i].index0, impl_->visibleRows[i].index0);
             }
             selectionModel()->select(sel, QItemSelectionModel::Select | QItemSelectionModel::Rows);
         } else if (selectionModel()) {
             selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
         itemSelected(target);
         event->accept();
         return;
     }
     case Qt::Key_Left:
         if (target.isValid() && impl_->hasChildren(target) && impl_->isExpanded(target)) {
             collapse(target);
         } else if (target.parent().isValid()) {
             target = target.parent().siblingAtColumn(0);
             if (selectionModel()) selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
             itemSelected(target);
         }
         event->accept();
         return;
     case Qt::Key_Right:
         if (target.isValid() && impl_->hasChildren(target) && !impl_->isExpanded(target)) {
             expand(target);
         } else if (target.isValid() && impl_->hasChildren(target)) {
             target = impl_->model->index(0, 0, target);
             if (selectionModel()) selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
             itemSelected(target);
         }
         event->accept();
         return;
     case Qt::Key_Return: case Qt::Key_Enter: handleItemDoubleClicked(target); event->accept(); return;
     default: QWidget::keyPressEvent(event); return;
     }
 }

QSize ArtifactProjectView::sizeHint() const { return QSize(400, 400); }

// --- Main Widget Implementation ---
class ArtifactProjectManagerWidget::Impl {
public:
    ArtifactProjectView* projectView_ = nullptr;
    ArtifactProjectModel* projectModel_ = nullptr;
    ProjectFilterProxyModel* proxyModel_ = nullptr;
    ProjectInfoPanel* infoPanel_ = nullptr;
    QLineEdit* searchBar = nullptr;
    QComboBox* typeFilterBox = nullptr;
    QComboBox* viewModeBox = nullptr;
    QCheckBox* unusedOnlyCheck = nullptr;
    QProgressBar* proxyQueueProgress = nullptr;
    ArtifactProjectManagerToolBox* toolBox = nullptr;
    QLabel* projectNameLabel = nullptr;
    QLabel* syncStateLabel = nullptr;
    QLabel* projectHealthLabel = nullptr;
    QLabel* selectionSummaryLabel = nullptr;
    QLabel* selectionDetailLabel = nullptr;
    QPushButton* openSelectionButton = nullptr;
    QPushButton* revealSelectionButton = nullptr;
    QPushButton* generateProxyButton = nullptr;
    QPushButton* revealProxyButton = nullptr;
    QPushButton* clearProxyButton = nullptr;
    QPushButton* generateSelectedProxiesButton = nullptr;
    QPushButton* clearSelectedProxiesButton = nullptr;
    QPushButton* regenerateStaleProxiesButton = nullptr;
    QPushButton* renameSelectionButton = nullptr;
    QPushButton* deleteSelectionButton = nullptr;
    QPushButton* relinkSelectionButton = nullptr;
    QPushButton* copyPathButton = nullptr;
    QWidget* compositionEditorPanel = nullptr;
    QLabel* compositionEditorTitleLabel = nullptr;
    QLabel* compositionEditorModeLabel = nullptr;
    QLineEdit* compositionNameEdit = nullptr;
    QSpinBox* compositionWidthSpin = nullptr;
    QSpinBox* compositionHeightSpin = nullptr;
    QDoubleSpinBox* compositionFrameRateSpin = nullptr;
    QSpinBox* compositionStartFrameSpin = nullptr;
    QSpinBox* compositionEndFrameSpin = nullptr;
    CompositionBackgroundColorButton* compositionBackgroundButton = nullptr;
    QPushButton* compositionApplyButton = nullptr;
    QPushButton* compositionApplyFrameRateButton = nullptr;
    bool thumbnailEnabled = true;
    QSet<QString> unusedAssetPaths_;
    struct ProxyJob {
        QString inputPath;
        QString outputPath;
        double scaleFactor = 0.5; // 0.25=1/4, 0.5=1/2, 1.0=full
    };
    std::deque<ProxyJob> proxyJobs_;
    QTimer* proxyQueueTimer_ = nullptr;
    QMetaObject::Connection currentRowChangedConnection_;
    bool headerLayoutInitialized_ = false;
    bool syncingSelectionToComposition_ = false;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
    bool projectRefreshQueued_ = false;

    QVector<CompositionItem*> selectedCompositionItems() const {
        QVector<CompositionItem*> items;
        if (!projectView_ || !projectView_->selectionModel()) {
            return items;
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        QSet<QString> seen;
        for (const auto& row : rows) {
            QModelIndex sourceIdx = row;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
            }
            const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            if (!typeVar.isValid() || typeVar.toInt() != static_cast<int>(eProjectItemType::Composition)) {
                continue;
            }
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            auto* compItem = item && item->type() == eProjectItemType::Composition ? static_cast<CompositionItem*>(item) : nullptr;
            if (!compItem) {
                continue;
            }
            const QString compKey = compItem->compositionId.toString();
            if (seen.contains(compKey)) {
                continue;
            }
            seen.insert(compKey);
            items.append(compItem);
        }
        return items;
    }

    int selectedCompositionCount() const {
        return selectedCompositionItems().size();
    }

    bool applySelectedCompositionFrameRate(const double frameRate) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return false;
        }
        const QVector<CompositionItem*> items = selectedCompositionItems();
        if (items.isEmpty()) {
            return false;
        }

        bool applied = false;
        for (auto* compItem : items) {
            if (!compItem) {
                continue;
            }
            const auto found = svc->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (!found.success || !comp) {
                continue;
            }
            comp->setFrameRate(FrameRate(static_cast<float>(std::max(1.0, frameRate))));
            applied = true;
            if (auto current = svc->currentComposition().lock(); current && current->id() == comp->id()) {
                if (auto* playback = ArtifactPlaybackService::instance()) {
                    playback->setFrameRate(comp->frameRate());
                }
            }
        }

        if (applied) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                project->projectChanged();
            }
        }
        return applied;
    }

    bool applySelectedCompositionSettings() {
        auto* svc = ArtifactProjectService::instance();
        if (!svc || !compositionNameEdit || !compositionWidthSpin || !compositionHeightSpin ||
            !compositionFrameRateSpin || !compositionStartFrameSpin || !compositionEndFrameSpin ||
            !compositionBackgroundButton) {
            return false;
        }
        const QVector<CompositionItem*> items = selectedCompositionItems();
        if (items.isEmpty()) {
            return false;
        }

        if (items.size() == 1) {
            auto* compItem = items.first();
            if (!compItem) {
                return false;
            }
            const auto found = svc->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (!found.success || !comp) {
                return false;
            }

            const QString trimmedName = compositionNameEdit->text().trimmed();
            if (trimmedName.isEmpty()) {
                return false;
            }
            const int startFrame = compositionStartFrameSpin->value();
            const int endFrame = compositionEndFrameSpin->value();
            if (startFrame > endFrame) {
                return false;
            }

            comp->setCompositionName(UniString::fromQString(trimmedName));
            {
                const QSize newSize(compositionWidthSpin->value(), compositionHeightSpin->value());
                const QSize oldSize = comp->settings().compositionSize();
                if (oldSize != newSize) {
                    bool hasMasks = false;
                    bool hasAnchors = false;
                    int maskVerts = 0;
                    int layerCount = 0;
                    for (const auto& layer : comp->allLayerRef()) {
                        if (!layer) continue;
                        ++layerCount;
                        if (layer->hasMasks()) {
                            hasMasks = true;
                            for (int mi = 0; mi < layer->maskCount(); ++mi) {
                                const auto lm = layer->mask(mi);
                                for (int pi = 0; pi < lm.maskPathCount(); ++pi) {
                                    maskVerts += lm.maskPath(pi).vertexCount();
                                }
                            }
                        }
                    }
                    hasAnchors = layerCount > 0;
                    auto impact = ArtifactCore::ResolutionRemap::calculateImpact(
                        oldSize, newSize, hasMasks, maskVerts > 0, hasAnchors);
                    impact.maskVertexCount = maskVerts;

                    Artifact::ArtifactResolutionRemapDialog dialog(oldSize, newSize, impact);
                    if (dialog.exec() == QDialog::Accepted && dialog.remapRequested()) {
                        if (auto* mgr = UndoManager::instance()) {
                            mgr->push(std::make_unique<ChangeCompositionResolutionCommand>(
                                comp, oldSize, newSize, dialog.selectedPolicy()));
                        } else {
                            comp->applyResolutionRemap(newSize, dialog.selectedPolicy());
                        }
                    } else {
                        comp->setCompositionSize(newSize);
                    }
                } else {
                    comp->setCompositionSize(newSize);
                }
            }
            comp->setFrameRate(FrameRate(static_cast<float>(std::max(1.0, compositionFrameRateSpin->value()))));
            comp->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));
            const QColor bg = compositionBackgroundButton->selectedColor();
            comp->setBackGroundColor(FloatColor(bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF()));

            if (!svc->renameComposition(compItem->compositionId, UniString::fromQString(trimmedName))) {
                return false;
            }

            if (auto project = svc->getCurrentProjectSharedPtr()) {
                project->projectChanged();
            }
            if (auto current = svc->currentComposition().lock(); current && current->id() == comp->id()) {
                if (auto* playback = ArtifactPlaybackService::instance()) {
                    playback->setFrameRange(comp->frameRange());
                    playback->setFrameRate(comp->frameRate());
                }
            }
            return true;
        }

        const int startFrame = compositionStartFrameSpin->value();
        const int endFrame = compositionEndFrameSpin->value();
        if (startFrame > endFrame) {
            return false;
        }
        const QSize newSize(compositionWidthSpin->value(), compositionHeightSpin->value());
        const float frameRate = static_cast<float>(std::max(1.0, compositionFrameRateSpin->value()));
        const QColor bg = compositionBackgroundButton->selectedColor();
        const FloatColor floatBg(bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF());

        bool applied = false;
        for (auto* compItem : items) {
            if (!compItem) {
                continue;
            }
            const auto found = svc->findComposition(compItem->compositionId);
            auto comp = found.ptr.lock();
            if (!found.success || !comp) {
                continue;
            }

            comp->setCompositionSize(newSize);
            comp->setFrameRate(FrameRate(frameRate));
            comp->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));
            comp->setBackGroundColor(floatBg);
            applied = true;

            if (auto current = svc->currentComposition().lock(); current && current->id() == comp->id()) {
                if (auto* playback = ArtifactPlaybackService::instance()) {
                    playback->setFrameRange(comp->frameRange());
                    playback->setFrameRate(comp->frameRate());
                }
            }
        }

        if (applied) {
            if (auto project = svc->getCurrentProjectSharedPtr()) {
                project->projectChanged();
            }
        }
        return applied;
    }

    void refreshCompositionEditor() {
        const QVector<CompositionItem*> items = selectedCompositionItems();
        const int count = items.size();
        const bool hasSingleComposition = count == 1;
        const bool hasAnyComposition = count > 0;
        const QString panelTitle = count > 1
            ? QStringLiteral("Batch Composition Edit")
            : QStringLiteral("Composition Edit");
        if (compositionEditorTitleLabel) {
            compositionEditorTitleLabel->setText(panelTitle);
        }
        if (compositionEditorModeLabel) {
            compositionEditorModeLabel->setText(count == 0
                ? QStringLiteral("Select a composition to edit its settings here.")
                : count == 1
                    ? QStringLiteral("Editing the selected composition in place.")
                    : QStringLiteral("Editing frame rate for %1 selected compositions.").arg(count));
        }

        if (!hasAnyComposition) {
            if (compositionNameEdit) compositionNameEdit->clear();
            if (compositionWidthSpin) compositionWidthSpin->setValue(1);
            if (compositionHeightSpin) compositionHeightSpin->setValue(1);
            if (compositionFrameRateSpin) compositionFrameRateSpin->setValue(30.0);
            if (compositionStartFrameSpin) compositionStartFrameSpin->setValue(0);
            if (compositionEndFrameSpin) compositionEndFrameSpin->setValue(0);
            if (compositionBackgroundButton) {
                compositionBackgroundButton->setSelectedColor(QColor(0, 0, 0));
            }
        } else if (hasSingleComposition) {
            auto* compItem = items.first();
            auto* svc = ArtifactProjectService::instance();
            const auto found = svc ? svc->findComposition(compItem->compositionId) : FindCompositionResult{};
            auto comp = found.ptr.lock();
            if (comp) {
                const QSize size = comp->settings().compositionSize();
                const FrameRange range = comp->frameRange().normalized();
                const QColor bg = QColor::fromRgbF(
                    comp->backgroundColor().r(),
                    comp->backgroundColor().g(),
                    comp->backgroundColor().b(),
                    comp->backgroundColor().a());

                if (compositionNameEdit) compositionNameEdit->setText(comp->settings().compositionName().toQString());
                if (compositionWidthSpin) compositionWidthSpin->setValue(std::max(1, size.width()));
                if (compositionHeightSpin) compositionHeightSpin->setValue(std::max(1, size.height()));
                if (compositionFrameRateSpin) compositionFrameRateSpin->setValue(std::max(1.0, static_cast<double>(comp->frameRate().framerate())));
                if (compositionStartFrameSpin) compositionStartFrameSpin->setValue(static_cast<int>(range.start()));
                if (compositionEndFrameSpin) compositionEndFrameSpin->setValue(static_cast<int>(range.end()));
                if (compositionBackgroundButton) {
                    compositionBackgroundButton->setSelectedColor(bg);
                }
            }
        } else {
            auto* compItem = items.first();
            auto* svc = ArtifactProjectService::instance();
            const auto found = svc ? svc->findComposition(compItem->compositionId) : FindCompositionResult{};
            auto comp = found.ptr.lock();
            if (comp && compositionFrameRateSpin) {
                compositionFrameRateSpin->setValue(std::max(1.0, static_cast<double>(comp->frameRate().framerate())));
            }
        }

        const bool editableSingle = hasSingleComposition;
        if (compositionNameEdit) compositionNameEdit->setEnabled(editableSingle);
        if (compositionWidthSpin) compositionWidthSpin->setEnabled(hasAnyComposition);
        if (compositionHeightSpin) compositionHeightSpin->setEnabled(hasAnyComposition);
        if (compositionStartFrameSpin) compositionStartFrameSpin->setEnabled(hasAnyComposition);
        if (compositionEndFrameSpin) compositionEndFrameSpin->setEnabled(hasAnyComposition);
        if (compositionBackgroundButton) compositionBackgroundButton->setEnabled(hasAnyComposition);
        if (compositionApplyButton) compositionApplyButton->setEnabled(hasAnyComposition);
        if (compositionFrameRateSpin) compositionFrameRateSpin->setEnabled(hasAnyComposition);
        if (compositionApplyFrameRateButton) compositionApplyFrameRateButton->setEnabled(hasAnyComposition);
    }

    QModelIndex currentSelectionIndex0() const {
        if (!projectView_ || !projectView_->selectionModel()) {
            return {};
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        if (rows.isEmpty()) {
            return {};
        }
        QModelIndex index = rows.first();
        if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(index.model())) {
            index = proxy->mapToSource(index).siblingAtColumn(0);
        }
        return index.siblingAtColumn(0);
    }

    static int relinkMissingFootage(const QString& rootDir, const QVector<FootageItem*>& targets) {
        auto findByFileName = [](const QString& searchRoot, const QString& fileName) -> QString {
            if (searchRoot.isEmpty() || fileName.isEmpty()) {
                return QString();
            }
            QDirIterator it(searchRoot, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (QFileInfo(candidate).fileName().compare(fileName, Qt::CaseInsensitive) == 0) {
                    return candidate;
                }
            }
            return QString();
        };

        int relinked = 0;
        for (auto* footage : targets) {
            if (!footage) {
                continue;
            }
            const QFileInfo currentInfo(footage->filePath);
            if (currentInfo.exists()) {
                continue;
            }
            const QString replacement = findByFileName(rootDir, currentInfo.fileName());
            if (!replacement.isEmpty()) {
                footage->filePath = QFileInfo(replacement).absoluteFilePath();
                ++relinked;
            }
        }
        return relinked;
    }

    QModelIndex currentSourceIndexFromSelection() const {
        return currentSelectionIndex0();
    }

    ProjectItem* currentSelectedItem() const {
        const QModelIndex sourceIdx = currentSourceIndexFromSelection();
        const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
        return ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
    }

    QStringList selectedFootageFilePaths() const {
        QStringList filePaths;
        if (!projectView_ || !projectView_->selectionModel()) {
            return filePaths;
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        QSet<QString> seen;
        for (const auto& row : rows) {
            QModelIndex sourceIdx = row;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
            }
            const QVariant typeVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
            if (!typeVar.isValid() || typeVar.toInt() != static_cast<int>(eProjectItemType::Footage)) {
                continue;
            }
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (!item || item->type() != eProjectItemType::Footage) {
                continue;
            }
            const QString path = QFileInfo(static_cast<FootageItem*>(item)->filePath).absoluteFilePath();
            if (path.isEmpty() || seen.contains(path)) {
                continue;
            }
            seen.insert(path);
            filePaths.push_back(path);
        }
        return filePaths;
    }

    QString proxySummaryForSelectedFootage() const {
        const QStringList footagePaths = selectedFootageFilePaths();
        if (footagePaths.isEmpty()) {
            return {};
        }
        int readyCount = 0;
        int staleCount = 0;
        QString firstReadyFileName;
        for (const QString& path : footagePaths) {
            const QString proxyPath = proxyFilePathForFootage(path);
            if (!proxyPath.isEmpty() && QFileInfo(proxyPath).exists()) {
                ++readyCount;
                if (firstReadyFileName.isEmpty()) {
                    firstReadyFileName = QFileInfo(proxyPath).fileName();
                }
                const auto it = proxyMetadata().constFind(path);
                if (it != proxyMetadata().constEnd()) {
                    const QFileInfo src(path);
                    if (src.exists() && it->sourceLastModified.isValid() &&
                        src.lastModified() > it->sourceLastModified) {
                        ++staleCount;
                    }
                }
            }
        }
        const QString qualityTag = [&]() -> QString {
            if (footagePaths.size() == 1) {
                const auto it = proxyMetadata().constFind(footagePaths.first());
                if (it != proxyMetadata().constEnd() && !it->qualityLabel.isEmpty())
                    return it->qualityLabel;
            }
            return {};
        }();
        const QString staleTag = staleCount > 0
            ? QStringLiteral(" [%1 stale]").arg(staleCount) : QString();
        const QString qTag = qualityTag.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(qualityTag);
        if (footagePaths.size() == 1) {
            return readyCount > 0
                ? QStringLiteral("Proxy: Ready%1%2").arg(qTag, staleTag)
                : QStringLiteral("Proxy: Missing");
        }
        return readyCount > 0
            ? QStringLiteral("Proxy: Ready %1/%2%3%4").arg(readyCount).arg(footagePaths.size()).arg(qTag, staleTag)
            : QStringLiteral("Proxy: Missing %1").arg(footagePaths.size());
    }

    static QString normalizeFilePathForProjectSelection(const QString& path) {
        if (path.isEmpty()) {
            return {};
        }
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            return QDir::cleanPath(canonical);
        }
        return QDir::cleanPath(info.absoluteFilePath());
    }

    bool selectItemsByFilePaths(const QStringList& filePaths) {
        if (!projectView_ || !proxyModel_ || !projectModel_ || !projectView_->selectionModel() || filePaths.isEmpty()) {
            return false;
        }

        QSet<QString> targetPaths;
        for (const QString& rawPath : filePaths) {
            const QString path = normalizeFilePathForProjectSelection(rawPath.trimmed());
            if (path.isEmpty()) {
                continue;
            }
            if (QFileInfo(path).isDir()) {
                continue;
            }
            targetPaths.insert(path);
        }
        if (targetPaths.isEmpty()) {
            return false;
        }

        QList<QModelIndex> sourceMatches;
        std::function<void(const QModelIndex&)> visit = [&](const QModelIndex& parent) {
            const int rowCount = projectModel_->rowCount(parent);
            for (int row = 0; row < rowCount; ++row) {
                const QModelIndex idx = projectModel_->index(row, 0, parent);
                if (!idx.isValid()) {
                    continue;
                }
                const QVariant ptrVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
                auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
                if (item && item->type() == eProjectItemType::Footage) {
                    const auto* footage = static_cast<FootageItem*>(item);
                    const QString itemPath = normalizeFilePathForProjectSelection(footage->filePath);
                    if (!itemPath.isEmpty() && targetPaths.contains(itemPath)) {
                        sourceMatches.push_back(idx);
                    }
                }
                visit(idx);
            }
        };
        visit({});

        if (sourceMatches.isEmpty()) {
            return false;
        }

        QItemSelection selection;
        QModelIndex firstProxyIndex;
        QModelIndex firstSourceIndex;
        for (const QModelIndex& sourceIdx : sourceMatches) {
            const QModelIndex proxyIdx = proxyModel_->mapFromSource(sourceIdx).siblingAtColumn(0);
            if (!proxyIdx.isValid()) {
                continue;
            }
            selection.select(proxyIdx, proxyIdx);
            if (!firstProxyIndex.isValid()) {
                firstProxyIndex = proxyIdx;
                firstSourceIndex = sourceIdx;
            }
            for (QModelIndex parent = proxyIdx.parent(); parent.isValid(); parent = parent.parent()) {
                projectView_->expand(parent);
            }
        }

        if (!firstProxyIndex.isValid()) {
            return false;
        }

        auto* sel = projectView_->selectionModel();
        QSignalBlocker blocker(sel);
        sel->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        sel->setCurrentIndex(firstProxyIndex, QItemSelectionModel::Current | QItemSelectionModel::Rows);
        projectView_->ensureIndexVisible(firstProxyIndex);
        projectView_->refreshVisibleContent();
        if (infoPanel_) {
            infoPanel_->updateInfo(firstSourceIndex);
        }
        refreshSelectionChrome();
        return true;
    }

    FolderItem* currentFolderTarget() const {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return nullptr;
        }
        if (item->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item);
        }
        if (item->parent && item->parent->type() == eProjectItemType::Folder) {
            return static_cast<FolderItem*>(item->parent);
        }
        return nullptr;
    }

    void createFolderAtSelection() const {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            return;
        }

        bool ok;
        QString name = QInputDialog::getText(projectView_, QStringLiteral("New Folder"),
            QStringLiteral("Folder Name:"), QLineEdit::Normal, QStringLiteral("New Folder"), &ok);
        
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }

        project->createFolder(UniString::fromQString(name), currentFolderTarget());
        // createFolder notifies projectChanged() internally
    }

    bool renameSelectedItem(QWidget* parent) {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return false;
        }
        bool ok = false;
        const QString currentName = item->name.toQString();
        const QString newName = QInputDialog::getText(parent, "Rename", "New Name:", QLineEdit::Normal, currentName, &ok);
        if (!ok || newName.trimmed().isEmpty()) {
            return false;
        }
        return renameProjectItem(item, newName);
    }

    bool deleteSelectedItem(QWidget* parent) {
        ProjectItem* item = currentSelectedItem();
        auto* svc = ArtifactProjectService::instance();
        if (!item || !svc) {
            return false;
        }
        const QString message = svc->projectItemRemovalConfirmationMessage(item);
        const auto answer = QMessageBox::question(
            parent,
            QStringLiteral("項目削除"),
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return svc->removeProjectItem(item);
    }

    QString selectedItemPath() const {
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return QString();
        }
        if (item->type() == eProjectItemType::Footage) {
            return static_cast<FootageItem*>(item)->filePath;
        }
        if (item->type() == eProjectItemType::Folder) {
            return folderDisplayPath(static_cast<FolderItem*>(item));
        }
        if (item->type() == eProjectItemType::Composition) {
            return static_cast<CompositionItem*>(item)->compositionId.toString();
        }
        return item->name.toQString();
    }

    QStringList selectedItemIds() const {
        QStringList ids;
        if (!projectView_ || !projectView_->selectionModel()) {
            return ids;
        }
        const auto rows = projectView_->selectionModel()->selectedRows(0);
        ids.reserve(rows.size());
        for (const auto& row : rows) {
            QModelIndex sourceIdx = row;
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(sourceIdx.model())) {
                sourceIdx = proxy->mapToSource(sourceIdx).siblingAtColumn(0);
            }
            const QVariant ptrVar = sourceIdx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemPtr));
            auto* item = ptrVar.isValid() ? reinterpret_cast<ProjectItem*>(ptrVar.value<quintptr>()) : nullptr;
            if (item) {
                ids.push_back(item->id.toString());
            }
        }
        return ids;
    }

    SelectionChangedEvent makeSelectionChangedEvent() const {
        SelectionChangedEvent event;
        event.selectedItemIds = selectedItemIds();
        event.currentItemId = currentSelectedItem() ? currentSelectedItem()->id.toString() : QString();
        event.selectedCount = event.selectedItemIds.size();
        return event;
    }

    QString selectionSummaryText() const {
        const int selectedCount = projectView_ && projectView_->selectionModel()
            ? projectView_->selectionModel()->selectedRows(0).size()
            : 0;
        const int selectedCompositionCountValue = selectedCompositionCount();
        const QString searchText = searchBar ? searchBar->text().trimmed() : QString();
        const QString typeText = typeFilterBox ? typeFilterBox->currentText() : QStringLiteral("All");
        const QString unusedText = unusedOnlyCheck && unusedOnlyCheck->isChecked()
            ? QStringLiteral("Unused only")
            : QStringLiteral("All items");
        const QString proxyText = proxySummaryForSelectedFootage();
        const QString proxyPart = proxyText.isEmpty() ? QString() : QStringLiteral(" | %1").arg(proxyText);
        const QString viewModeText = projectView_ && projectView_->presentationMode() == ArtifactProjectView::PresentationMode::Tile
            ? QStringLiteral("Tile")
            : QStringLiteral("List");
        const QString compositionPart = selectedCompositionCountValue > 0
            ? QStringLiteral(" | Comps: %1").arg(selectedCompositionCountValue)
            : QString();
        return QStringLiteral("Recent: - | Selected: %1 | View: %2 / %3 / %4 | Search: %5%6%7")
            .arg(selectedCount)
            .arg(viewModeText)
            .arg(typeText)
            .arg(unusedText)
            .arg(searchText.isEmpty() ? QStringLiteral("-") : searchText)
            .arg(proxyPart)
            .arg(compositionPart);
    }

    QString syncStateText() const {
        return QStringLiteral("Status: Asset Browser linked");
    }

    QString projectHealthText() const {
        auto* svc = ArtifactProjectService::instance();
        if (!svc || !svc->hasProject()) {
            return QStringLiteral("Status: Open a project to inspect details");
        }
        const QString health = svc->currentProjectHealthSummaryText();
        const QString unused = QStringLiteral("Unused: %1").arg(unusedAssetPaths_.size());
        return health.isEmpty() ? unused : QStringLiteral("%1 | %2").arg(health, unused);
    }

    void refreshSelectionChrome() {
        if (viewModeBox && projectView_) {
            const QString desiredMode = projectView_->presentationMode() == ArtifactProjectView::PresentationMode::Tile
                                            ? QStringLiteral("Tile")
                                            : QStringLiteral("List");
            if (viewModeBox->currentText() != desiredMode) {
                const QSignalBlocker blocker(viewModeBox);
                viewModeBox->setCurrentText(desiredMode);
            }
        }
        if (syncStateLabel) {
            syncStateLabel->setText(syncStateText());
        }
        if (projectHealthLabel) {
            projectHealthLabel->setText(projectHealthText());
        }
        if (selectionSummaryLabel) {
            const int resultCount = proxyModel_ ? proxyModel_->visibleRowCount() : 0;
            selectionSummaryLabel->setText(QStringLiteral("%1 | Results: %2")
                                               .arg(selectionSummaryText())
                                               .arg(resultCount));
            selectionSummaryLabel->setToolTip(
                QStringLiteral("Current selection count, active filter, search text, and filtered result count."));
        }
        refreshCompositionEditor();
        ProjectItem* item = currentSelectedItem();
        const bool hasItem = item != nullptr;
        const bool isFootage = item && item->type() == eProjectItemType::Footage;
        const bool isFolder = item && item->type() == eProjectItemType::Folder;
        const bool isComposition = item && item->type() == eProjectItemType::Composition;
        const int selectedCompositionCountValue = selectedCompositionCount();
        const QString pathText = selectedItemPath();
        const QString proxyPath = isFootage ? proxyFilePathForFootage(static_cast<FootageItem*>(item)->filePath) : QString();
        const bool hasProxy = !proxyPath.isEmpty() && QFileInfo(proxyPath).exists();
        const QString proxySummaryText = isFootage ? proxySummaryForSelectedFootage() : QString();
        const QString statusText = !item ? QStringLiteral("Select an item to inspect details")
            : isFootage ? (QFileInfo(static_cast<FootageItem*>(item)->filePath).exists() ? QStringLiteral("Available") : QStringLiteral("Missing"))
            : isFolder ? QStringLiteral("Folder")
            : isComposition ? QStringLiteral("Composition")
            : QStringLiteral("Item");
        if (selectionDetailLabel) {
            if (!hasItem) {
                selectionDetailLabel->setText(QStringLiteral("Open a project or search to inspect details | Click an item to open it."));
                selectionDetailLabel->setToolTip(QStringLiteral("Open a project or search, then click an item to inspect it."));
            } else {
                const QString pathPart = pathText.isEmpty() ? QStringLiteral("-") : pathText;
                if (isFootage) {
                    const QString proxyPart = proxySummaryText.isEmpty()
                        ? (hasProxy ? QStringLiteral("Proxy: Ready (%1)").arg(QFileInfo(proxyPath).fileName())
                                    : QStringLiteral("Proxy: Missing"))
                        : proxySummaryText;
                    selectionDetailLabel->setText(QStringLiteral("Status: %1 | %2 | %3 | Preview")
                                                      .arg(statusText, pathPart, proxyPart));
                    selectionDetailLabel->setToolTip(QStringLiteral("Open the selected footage, reveal it, generate a proxy, or copy its path."));
                } else {
                    const QString batchPart = isComposition && selectedCompositionCountValue > 1
                        ? QStringLiteral(" | Batch FPS: %1 comps").arg(selectedCompositionCountValue)
                        : QString();
                    const QString inlineEditPart = isComposition
                        ? QStringLiteral(" | Inline edit ready")
                        : QString();
                    selectionDetailLabel->setText(QStringLiteral("Status: %1 | %2%3%4")
                                                      .arg(statusText, pathPart, batchPart, inlineEditPart));
                    selectionDetailLabel->setToolTip(isComposition
                        ? QStringLiteral("Click to focus the inline composition editor, or use the action buttons below.")
                        : QStringLiteral("Open the selected item or use the action buttons below."));
                }
            }
        }
        if (openSelectionButton) openSelectionButton->setEnabled(hasItem);
        if (revealSelectionButton) revealSelectionButton->setEnabled(isFootage);
        if (generateProxyButton) generateProxyButton->setEnabled(isFootage);
        if (revealProxyButton) {
            revealProxyButton->setEnabled(isFootage && hasProxy);
        }
        if (clearProxyButton) {
            clearProxyButton->setEnabled(isFootage && hasProxy);
        }
        const int selectedFootageCount = selectedFootageFilePaths().size();
        if (generateSelectedProxiesButton) {
            generateSelectedProxiesButton->setEnabled(selectedFootageCount > 1);
        }
        if (clearSelectedProxiesButton) {
            clearSelectedProxiesButton->setEnabled(selectedFootageCount > 1);
        }
        if (regenerateStaleProxiesButton) {
            bool hasStaleOrMissing = false;
            for (const QString& path : selectedFootageFilePaths()) {
                const QString pp = proxyFilePathForFootage(path);
                if (pp.isEmpty()) continue;
                const bool exists = QFileInfo(pp).exists();
                if (!exists) { hasStaleOrMissing = true; break; }
                const auto it = proxyMetadata().constFind(path);
                if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                    const QFileInfo src(path);
                    if (src.exists() && src.lastModified() > it->sourceLastModified) {
                        hasStaleOrMissing = true; break;
                    }
                }
            }
            regenerateStaleProxiesButton->setEnabled(hasStaleOrMissing);
        }
        if (renameSelectionButton) renameSelectionButton->setEnabled(hasItem);
        if (deleteSelectionButton) deleteSelectionButton->setEnabled(hasItem);
        if (copyPathButton) copyPathButton->setEnabled(isFootage);
        if (relinkSelectionButton) {
            relinkSelectionButton->setEnabled(isFootage && !QFileInfo(static_cast<FootageItem*>(item)->filePath).exists());
        }
    }

    void openSelectedItem(QWidget* parent) {
        Q_UNUSED(parent);
        const QModelIndex idx = currentSelectionIndex0();
        if (!idx.isValid()) {
            return;
        }
        if (projectView_) {
            ProjectItem* item = currentSelectedItem();
            if (item && item->type() == eProjectItemType::Folder) {
                projectView_->handleItemDoubleClicked(idx);
            } else {
                projectView_->itemDoubleClicked(idx);
                projectView_->handleItemDoubleClicked(idx);
            }
        }
    }

    void revealSelectedItem(QWidget* parent) {
        Q_UNUSED(parent);
        ProjectItem* item = currentSelectedItem();
        if (!item) {
            return;
        }
        if (item->type() == eProjectItemType::Footage) {
            const QString path = static_cast<FootageItem*>(item)->filePath;
            if (!path.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        }
    }

    void generateProxyForSelectedItem() {
        auto* svc = ArtifactProjectService::instance();
        ProjectItem* item = currentSelectedItem();
        if (!svc || !item || item->type() != eProjectItemType::Footage) {
            return;
        }
        const QString path = static_cast<FootageItem*>(item)->filePath;
        if (!proxyMetadata().contains(path) || proxyMetadata()[path].qualityLabel.isEmpty()) {
            auto* parentW = projectView_ ? projectView_->parentWidget() : nullptr;
            QDialog dlg(parentW);
            dlg.setWindowTitle(QStringLiteral("Proxy Quality"));
            dlg.setMinimumWidth(320);
            auto* dl = new QVBoxLayout(&dlg);
            dl->addWidget(new QLabel(QStringLiteral("Select proxy resolution:"), &dlg));
            auto* combo = new QComboBox(&dlg);
            combo->addItem(QStringLiteral("1/4 (Quarter)"), static_cast<int>(ProjectProxyQuality::Quarter));
            combo->addItem(QStringLiteral("1/2 (Half)"), static_cast<int>(ProjectProxyQuality::Half));
            combo->addItem(QStringLiteral("Full (1:1)"), static_cast<int>(ProjectProxyQuality::Full));
            combo->setCurrentIndex(1);
            dl->addWidget(combo);
            auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            dl->addWidget(btns);
            if (dlg.exec() != QDialog::Accepted) return;
            const auto q = static_cast<ProjectProxyQuality>(combo->itemData(combo->currentIndex()).toInt());
            proxyMetadata()[path].quality = q;
            proxyMetadata()[path].qualityLabel = combo->currentText();
        }
        QVector<FootageItem*> footage;
        auto* footageItem = static_cast<FootageItem*>(item);
        footage.append(footageItem);
        queueProxyGeneration(footage);
        const QString proxyPath = proxyFilePathForFootage(footageItem->filePath);
        const bool enabled = proxyMetadata().value(footageItem->filePath).enabled;
        syncProxyPathToProject(footageItem->filePath, proxyPath, enabled, proxyGlobalEnabled_);
    }

    void generateProxyForFilePath(const QString& sourceFilePath) {
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        const QString targetPath = QFileInfo(sourceFilePath).absoluteFilePath();
        if (targetPath.isEmpty()) {
            return;
        }
        auto project = svc->getCurrentProjectSharedPtr();
        if (!project) {
            return;
        }

        // Prompt quality selection on first generation for this source
        if (!proxyMetadata().contains(targetPath) || proxyMetadata()[targetPath].qualityLabel.isEmpty()) {
            auto* parentW = projectView_ ? projectView_->parentWidget() : nullptr;
            QDialog dlg(parentW);
            dlg.setWindowTitle(QStringLiteral("Proxy Quality"));
            dlg.setMinimumWidth(320);
            auto* dl = new QVBoxLayout(&dlg);
            dl->addWidget(new QLabel(QStringLiteral("Select proxy resolution:"), &dlg));
            auto* combo = new QComboBox(&dlg);
            combo->addItem(QStringLiteral("1/4 (Quarter)"), static_cast<int>(ProjectProxyQuality::Quarter));
            combo->addItem(QStringLiteral("1/2 (Half)"), static_cast<int>(ProjectProxyQuality::Half));
            combo->addItem(QStringLiteral("Full (1:1)"), static_cast<int>(ProjectProxyQuality::Full));
            combo->setCurrentIndex(1);
            dl->addWidget(combo);
            auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            dl->addWidget(btns);
            if (dlg.exec() != QDialog::Accepted) return;
            const int selIdx = combo->currentIndex();
            const auto q = static_cast<ProjectProxyQuality>(combo->itemData(selIdx).toInt());
            proxyMetadata()[targetPath].quality = q;
            proxyMetadata()[targetPath].qualityLabel = combo->currentText();
        }

        QVector<FootageItem*> footage;
        const auto roots = project->projectItems();
        for (auto* root : roots) {
            if (!root) {
                continue;
            }
            std::function<void(ProjectItem*)> collect = [&](ProjectItem* item) {
                if (!item) {
                    return;
                }
                if (item->type() == eProjectItemType::Footage) {
                    auto* footageItem = static_cast<FootageItem*>(item);
                    if (QFileInfo(footageItem->filePath).absoluteFilePath() == targetPath) {
                        footage.append(footageItem);
                    }
                }
                for (auto* child : item->children) {
                    collect(child);
                }
            };
            collect(root);
        }
        if (footage.isEmpty()) {
            return;
        }
        queueProxyGeneration(footage);
        const QString proxyPath = proxyFilePathForFootage(targetPath);
        const bool enabled = proxyMetadata().value(targetPath).enabled;
        syncProxyPathToProject(targetPath, proxyPath, enabled, proxyGlobalEnabled_);
        if (auto* widget = projectView_ ? qobject_cast<ArtifactProjectManagerWidget*>(projectView_->parentWidget()) : nullptr) {
            widget->updateRequested();
        }
    }

    void revealProxyForSelectedItem(QWidget* parent) {
        Q_UNUSED(parent);
        ProjectItem* item = currentSelectedItem();
        if (!item || item->type() != eProjectItemType::Footage) {
            return;
        }
        const QString proxyPath = proxyFilePathForFootage(static_cast<FootageItem*>(item)->filePath);
        if (proxyPath.isEmpty() || !QFileInfo(proxyPath).exists()) {
            QMessageBox::information(parent, QStringLiteral("Proxy"), QStringLiteral("Proxy file is not available yet."));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(proxyPath).absolutePath()));
    }

    void revealProxyForFilePath(const QString& sourceFilePath, QWidget* parent) {
        Q_UNUSED(parent);
        const QString proxyPath = proxyFilePathForFootage(sourceFilePath);
        if (proxyPath.isEmpty() || !QFileInfo(proxyPath).exists()) {
            QMessageBox::information(parent, QStringLiteral("Proxy"), QStringLiteral("Proxy file is not available yet."));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(proxyPath).absolutePath()));
    }

    bool clearProxyForFilePath(const QString& sourceFilePath, QWidget* parent) {
        const QString targetPath = QFileInfo(sourceFilePath).absoluteFilePath();
        const QString proxyPath = proxyFilePathForFootage(targetPath);
        if (proxyPath.isEmpty()) {
            return false;
        }
        if (QFileInfo::exists(proxyPath) && !QFile::remove(proxyPath)) {
            QMessageBox::warning(parent, QStringLiteral("Proxy"), QStringLiteral("Proxy file could not be removed."));
            return false;
        }
        proxyMetadata().remove(targetPath);
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return true;
        }
        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return true;
        }
        const auto layers = currentComp->allLayer();
        for (const auto& layer : layers) {
            auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
            if (!videoLayer) {
                continue;
            }
            const QString layerSourcePath = videoLayer->sourcePath().trimmed();
            if (layerSourcePath.isEmpty() || QFileInfo(layerSourcePath).absoluteFilePath() != targetPath) {
                continue;
            }
            if (!videoLayer->proxyPath().isEmpty()) {
                videoLayer->clearProxy();
                videoLayer->changed();
                ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
                    LayerChangedEvent{currentComp->id().toString(), videoLayer->id().toString(),
                                      LayerChangedEvent::ChangeType::Modified});
            }
        }
        return true;
    }

    void clearProxyForSelectedItem(QWidget* parent) {
        ProjectItem* item = currentSelectedItem();
        if (!item || item->type() != eProjectItemType::Footage) {
            return;
        }
        const auto* footage = static_cast<FootageItem*>(item);
        if (!clearProxyForFilePath(footage->filePath, parent)) {
            return;
        }
        refreshSelectionChrome();
        if (auto* widget = projectView_ ? qobject_cast<ArtifactProjectManagerWidget*>(projectView_->parentWidget()) : nullptr) {
            widget->updateRequested();
        }
    }

    void copySelectedPathToClipboard() {
        const QString path = selectedItemPath();
        if (!path.isEmpty()) {
            QApplication::clipboard()->setText(path);
        }
    }

    void relinkSelectedItem(QWidget* parent) {
        auto* svc = ArtifactProjectService::instance();
        ProjectItem* item = currentSelectedItem();
        if (!svc || !item || item->type() != eProjectItemType::Footage) {
            return;
        }
        const QString root = QFileDialog::getExistingDirectory(parent, QStringLiteral("Relink Selected Footage - Search Root"));
        if (root.isEmpty()) {
            return;
        }
        QVector<FootageItem*> targets;
        targets.append(static_cast<FootageItem*>(item));
        const int relinked = Impl::relinkMissingFootage(root, targets);
        if (relinked > 0) {
            svc->projectChanged();
        }
        QMessageBox::information(parent, QStringLiteral("Relink Result"),
                                 QStringLiteral("Relinked %1 file(s).").arg(relinked));
    }

    void syncSelectionToCurrentComposition() {
        if (syncingSelectionToComposition_) {
            return;
        }
        if (!projectView_ || !proxyModel_) {
            return;
        }
        auto* svc = ArtifactProjectService::instance();
        if (!svc) {
            return;
        }
        auto currentComp = svc->currentComposition().lock();
        if (!currentComp) {
            return;
        }

        std::function<QModelIndex(const QModelIndex&)> findCompositionIndex = [&](const QModelIndex& parent) -> QModelIndex {
            if (!projectModel_) {
                return {};
            }
            const int rowCount = projectModel_->rowCount(parent);
            for (int row = 0; row < rowCount; ++row) {
                const QModelIndex idx = projectModel_->index(row, 0, parent);
                if (!idx.isValid()) {
                    continue;
                }
                const QVariant typeVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::ProjectItemType));
                if (typeVar.isValid() && typeVar.toInt() == static_cast<int>(eProjectItemType::Composition)) {
                    const QVariant idVar = idx.data(Qt::UserRole + static_cast<int>(Artifact::ProjectItemDataRole::CompositionId));
                    if (idVar.isValid() && idVar.toString() == currentComp->id().toString()) {
                        return idx;
                    }
                }
                if (const QModelIndex child = findCompositionIndex(idx); child.isValid()) {
                    return child;
                }
            }
            return {};
        };

        const QModelIndex sourceIndex = findCompositionIndex({});
        if (!sourceIndex.isValid()) {
            return;
        }
        const QModelIndex proxyIndex = proxyModel_->mapFromSource(sourceIndex);
        if (!proxyIndex.isValid()) {
            return;
        }
        if (projectView_->selectionModel() && projectView_->selectionModel()->currentIndex() == proxyIndex) {
            for (QModelIndex parent = proxyIndex.parent(); parent.isValid(); parent = parent.parent()) {
                projectView_->expand(parent);
            }
            projectView_->ensureIndexVisible(proxyIndex);
            if (infoPanel_) {
                infoPanel_->updateInfo(sourceIndex);
            }
            return;
        }

        struct SyncGuard {
            bool& flag;
            explicit SyncGuard(bool& f) : flag(f) { flag = true; }
            ~SyncGuard() { flag = false; }
        } syncGuard(syncingSelectionToComposition_);

        for (QModelIndex parent = proxyIndex.parent(); parent.isValid(); parent = parent.parent()) {
            projectView_->expand(parent);
        }
        if (auto* selectionModel = projectView_->selectionModel()) {
            const QSignalBlocker blocker(selectionModel);
            selectionModel->setCurrentIndex(
                proxyIndex,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current);
        }
        projectView_->ensureIndexVisible(proxyIndex);
        if (infoPanel_) {
            infoPanel_->updateInfo(sourceIndex);
        }
    }

    static void collectFootageRecursive(ProjectItem* item, QVector<FootageItem*>& out) {
        if (!item) return;
        if (item->type() == eProjectItemType::Footage) {
            out.append(static_cast<FootageItem*>(item));
        }
        for (auto* child : item->children) {
            collectFootageRecursive(child, out);
        }
    }

    void queueProxyGeneration(const QVector<FootageItem*>& footageItems) {
        const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir proxyDir(appDataRoot);
        proxyDir.mkpath(QStringLiteral("ProxyCache"));

        for (auto* f : footageItems) {
            if (!f || f->filePath.isEmpty()) continue;
            const QFileInfo src(f->filePath);
            if (!src.exists()) continue;
            const QString out = proxyFilePathForFootage(src.absoluteFilePath());
            if (out.isEmpty()) {
                continue;
            }

            auto& meta = proxyMetadata()[src.absoluteFilePath()];
            meta.sourceLastModified = src.lastModified();

            const double scale = meta.quality == ProjectProxyQuality::Quarter ? 0.25
                               : meta.quality == ProjectProxyQuality::Full  ? 1.0
                               : 0.5;
            proxyJobs_.push_back({src.absoluteFilePath(), out, scale});
        }
        if (!proxyQueueTimer_) {
            proxyQueueTimer_ = new QTimer();
            proxyQueueTimer_->setInterval(5);
            QObject::connect(proxyQueueTimer_, &QTimer::timeout, [this]() {
                processNextProxyJob();
            });
        }
        if (!proxyJobs_.empty()) {
            if (proxyQueueProgress) {
                proxyQueueProgress->setMaximum(static_cast<int>(proxyJobs_.size()));
                proxyQueueProgress->setValue(0);
                proxyQueueProgress->setVisible(true);
            }
            proxyQueueTimer_->start();
        }
    }

    void processNextProxyJob() {
        if (proxyJobs_.empty()) {
            if (proxyQueueTimer_) proxyQueueTimer_->stop();
            if (proxyQueueProgress) proxyQueueProgress->setVisible(false);
            return;
        }

        const ProxyJob job = proxyJobs_.front();
        proxyJobs_.pop_front();
        QImage img(job.inputPath);
        if (!img.isNull()) {
            const int targetW = qMax(64, static_cast<int>(img.width() * job.scaleFactor));
            const int targetH = qMax(64, static_cast<int>(img.height() * job.scaleFactor));
            const QImage scaled = img.scaled(targetW, targetH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const int jpegQuality = job.scaleFactor >= 0.9 ? 92 : 80;
            scaled.save(job.outputPath, "JPG", jpegQuality);
        }

        if (proxyQueueProgress) {
            const int done = proxyQueueProgress->maximum() - static_cast<int>(proxyJobs_.size());
            proxyQueueProgress->setValue(done);
        }
    }

    void refreshUnusedAssetCache() {
        unusedAssetPaths_.clear();
        auto shared = ArtifactProjectService::instance()->getCurrentProjectSharedPtr();
        if (!shared) return;
        const QStringList list = ArtifactProjectCleanupTool::findUnusedAssetPaths(shared.get());
        for (const QString& s : list) {
            unusedAssetPaths_.insert(s);
        }
        if (proxyModel_) {
            proxyModel_->setUnusedAssetPaths(unusedAssetPaths_);
        }
    }

    void update() {
        auto* svc = ArtifactProjectService::instance();
        std::shared_ptr<ArtifactProject> shared = svc ? svc->getCurrentProjectSharedPtr()
                                                       : nullptr;
        if (!projectModel_) projectModel_ = new ArtifactProjectModel();
        projectModel_->setProject(shared);

        if (!proxyModel_) {
            proxyModel_ = new ProjectFilterProxyModel();
            proxyModel_->setSourceModel(projectModel_);
            proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
            proxyModel_->setRecursiveFilteringEnabled(true);
            proxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);
        }
        if (projectView_) {
            projectView_->setModel(proxyModel_);
            projectView_->setSortingEnabled(true);
            projectView_->sortByColumn(0, Qt::AscendingOrder);
            if (!headerLayoutInitialized_) {
                projectView_->setColumnWidth(0, 260);
                projectView_->setColumnWidth(1, 120);
                projectView_->setColumnWidth(2, 120);
                projectView_->setColumnWidth(3, 100);
                projectView_->setColumnWidth(4, 140);
                projectView_->setColumnWidth(5, 180);
                headerLayoutInitialized_ = true;
            }
            projectView_->expandToDepth(1);
            if (projectView_->selectionModel()) {
                QObject::disconnect(currentRowChangedConnection_);
                currentRowChangedConnection_ =
                    QObject::connect(projectView_->selectionModel(), &QItemSelectionModel::currentRowChanged, projectView_,
                    [this](const QModelIndex& current, const QModelIndex&) {
                        if (!current.isValid() || !proxyModel_ || !infoPanel_) {
                            refreshSelectionChrome();
                            return;
                        }
                        infoPanel_->updateInfo(proxyModel_->mapToSource(current));
                        refreshSelectionChrome();
                    });
            }
        }
        refreshUnusedAssetCache();
        if (proxyModel_) {
            proxyModel_->setAdvancedFilter(
                searchBar ? searchBar->text() : QString(),
                typeFilterBox ? typeFilterBox->currentText() : QString(),
                unusedOnlyCheck ? unusedOnlyCheck->isChecked() : false);
        }
        refreshSelectionChrome();
        syncSelectionToCurrentComposition();
    }

    void handleSearch(const QString& text) {
        if (!proxyModel_) return;
        const QString trimmed = text.trimmed();
        ArtifactLayerSearchQuery query;
        query.setSearchText(trimmed.toUtf8().constData());
        proxyModel_->setSearchQuery(query);
        proxyModel_->setAdvancedFilter(
            trimmed,
            typeFilterBox ? typeFilterBox->currentText() : QString(),
            unusedOnlyCheck ? unusedOnlyCheck->isChecked() : false);
        if (!trimmed.isEmpty() && projectView_) projectView_->expandAll();
        refreshSelectionChrome();
    }

    bool proxyGlobalEnabled_ = true;
    QCheckBox* proxyGlobalToggle_ = nullptr;
};

ArtifactProjectManagerWidget::ArtifactProjectManagerWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setObjectName(QStringLiteral("artifactProjectManagerWidget"));
    setAutoFillBackground(true);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* chromePanel = new QWidget(this);
    chromePanel->setObjectName(QStringLiteral("projectManagerChrome"));
    chromePanel->setAutoFillBackground(true);
    auto* chromeLayout = new QVBoxLayout(chromePanel);
    chromeLayout->setContentsMargins(0, 0, 0, 0);
    chromeLayout->setSpacing(0);

    impl_->infoPanel_ = new ProjectInfoPanel(chromePanel);
    chromeLayout->addWidget(impl_->infoPanel_);

    impl_->projectNameLabel = new QLabel(QStringLiteral("Current: Project View"));
    impl_->projectNameLabel->setObjectName(QStringLiteral("projectManagerSectionLabel"));
    impl_->projectNameLabel->setMaximumHeight(24);
    chromeLayout->addWidget(impl_->projectNameLabel);

    impl_->syncStateLabel = new QLabel(QStringLiteral("Status: Asset Browser linked"), chromePanel);
    impl_->syncStateLabel->setObjectName(QStringLiteral("projectManagerSyncChip"));
    {
        QFont f = impl_->syncStateLabel->font();
        f.setPointSize(9);
        f.setBold(true);
        impl_->syncStateLabel->setFont(f);
        impl_->syncStateLabel->setAlignment(Qt::AlignCenter);
        impl_->syncStateLabel->setContentsMargins(8, 3, 8, 3);
        impl_->syncStateLabel->setMaximumHeight(28);
        QPalette pal = impl_->syncStateLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().accentColor));
        impl_->syncStateLabel->setPalette(pal);
    }
    chromeLayout->addWidget(impl_->syncStateLabel);

    impl_->projectHealthLabel = new QLabel(QStringLiteral("Status: Open a project to inspect details"), chromePanel);
    impl_->projectHealthLabel->setObjectName(QStringLiteral("projectManagerSyncChip"));
    {
        QFont f = impl_->projectHealthLabel->font();
        f.setPointSize(9);
        f.setBold(true);
        impl_->projectHealthLabel->setFont(f);
        impl_->projectHealthLabel->setAlignment(Qt::AlignCenter);
        impl_->projectHealthLabel->setContentsMargins(8, 3, 8, 3);
        impl_->projectHealthLabel->setMaximumHeight(28);
        QPalette pal = impl_->projectHealthLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
        impl_->projectHealthLabel->setPalette(pal);
    }
    chromeLayout->addWidget(impl_->projectHealthLabel);

    auto* selectionChrome = new QWidget(chromePanel);
    auto* selectionChromeLayout = new QVBoxLayout(selectionChrome);
    selectionChromeLayout->setContentsMargins(8, 0, 8, 5);
    selectionChromeLayout->setSpacing(2);
    impl_->selectionSummaryLabel = new QLabel(QStringLiteral("Recent: - | Selection: 0 items | Status: All / All items | Search: -"), selectionChrome);
    impl_->selectionSummaryLabel->setWordWrap(true);
    impl_->selectionSummaryLabel->setMaximumHeight(42);
    impl_->selectionSummaryLabel->setToolTip(QStringLiteral("Current filters and selection count."));
    {
        QFont f = impl_->selectionSummaryLabel->font();
        f.setPointSize(10);
        impl_->selectionSummaryLabel->setFont(f);
        QPalette pal = impl_->selectionSummaryLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
        impl_->selectionSummaryLabel->setPalette(pal);
    }
    selectionChromeLayout->addWidget(impl_->selectionSummaryLabel);
    auto* selectionDetailLabel = new ProjectActionLabel(QStringLiteral("Open a project or search to inspect details | Select an item to inspect details"), selectionChrome);
    impl_->selectionDetailLabel = selectionDetailLabel;
    impl_->selectionDetailLabel->setWordWrap(true);
    impl_->selectionDetailLabel->setMaximumHeight(42);
    impl_->selectionDetailLabel->setCursor(Qt::PointingHandCursor);
    impl_->selectionDetailLabel->setToolTip(QStringLiteral("Click to open the selected item."));
    {
        QFont f = impl_->selectionDetailLabel->font();
        f.setPointSize(11);
        impl_->selectionDetailLabel->setFont(f);
        QPalette pal = impl_->selectionDetailLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(120));
        impl_->selectionDetailLabel->setPalette(pal);
    }
    selectionChromeLayout->addWidget(impl_->selectionDetailLabel);
    static_cast<ProjectActionLabel*>(impl_->selectionDetailLabel)->activated = [this]() {
        if (impl_) {
            ProjectItem* item = impl_->currentSelectedItem();
            if (item && item->type() == eProjectItemType::Composition && impl_->compositionNameEdit) {
                impl_->compositionNameEdit->setFocus(Qt::MouseFocusReason);
                impl_->compositionNameEdit->selectAll();
                return;
            }
            impl_->openSelectedItem(this);
        }
    };

    auto* selectionButtons = new QHBoxLayout();
    selectionButtons->setContentsMargins(0, 0, 0, 0);
    selectionButtons->setSpacing(3);
    impl_->openSelectionButton = new QPushButton(QStringLiteral("Open"), selectionChrome);
    impl_->revealSelectionButton = new QPushButton(QStringLiteral("Reveal"), selectionChrome);
    impl_->generateProxyButton = new QPushButton(QStringLiteral("Proxy"), selectionChrome);
    impl_->revealProxyButton = new QPushButton(QStringLiteral("Reveal Proxy"), selectionChrome);
    impl_->clearProxyButton = new QPushButton(QStringLiteral("Clear Proxy"), selectionChrome);
    impl_->generateSelectedProxiesButton = new QPushButton(QStringLiteral("Gen Selected"), selectionChrome);
    impl_->clearSelectedProxiesButton = new QPushButton(QStringLiteral("Clear Selected"), selectionChrome);
    impl_->regenerateStaleProxiesButton = new QPushButton(QStringLiteral("Re-gen Stale"), selectionChrome);
    impl_->renameSelectionButton = new QPushButton(QStringLiteral("Rename"), selectionChrome);
    impl_->deleteSelectionButton = new QPushButton(QStringLiteral("Delete"), selectionChrome);
    impl_->relinkSelectionButton = new QPushButton(QStringLiteral("Relink"), selectionChrome);
    impl_->copyPathButton = new QPushButton(QStringLiteral("Copy Path"), selectionChrome);
    impl_->proxyGlobalToggle_ = new QCheckBox(QStringLiteral("Global Proxy"), selectionChrome);
    impl_->proxyGlobalToggle_->setChecked(impl_->proxyGlobalEnabled_);
    impl_->proxyGlobalToggle_->setToolTip(QStringLiteral("Enable or disable proxy playback for all footage. When disabled, layers fall back to original source."));
    QObject::connect(impl_->proxyGlobalToggle_, &QCheckBox::toggled, [this](bool checked) {
        if (!impl_) return;
        impl_->proxyGlobalEnabled_ = checked;
        if (auto* svc = ArtifactProjectService::instance()) {
            auto project = svc->getCurrentProjectSharedPtr();
            if (!project) return;
            const auto roots = project->projectItems();
            std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
                if (!item) return;
                if (item->type() == eProjectItemType::Composition) {
                    auto* compItem = static_cast<CompositionItem*>(item);
                    auto found = svc->findComposition(compItem->compositionId);
                    auto comp = found.ptr.lock();
                    if (!found.success || !comp) return;
                    for (const auto& layer : comp->allLayer()) {
                        auto vl = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
                        if (!vl) continue;
                        if (checked) {
                            const QString proxyPath = proxyFilePathForFootage(vl->sourcePath().trimmed());
                            if (!proxyPath.isEmpty() && QFileInfo(proxyPath).exists())
                                vl->setProxyPath(proxyPath);
                        } else {
                            vl->clearProxy();
                        }
                        vl->changed();
                    }
                }
                for (auto* child : item->children) visit(child);
            };
            for (auto* root : roots) visit(root);
        }
        if (auto* w = qobject_cast<ArtifactProjectManagerWidget*>(parentWidget())) {
            w->updateRequested();
        }
    });
    impl_->openSelectionButton->setToolTip(QStringLiteral("Open the selected item."));
    impl_->revealSelectionButton->setToolTip(QStringLiteral("Reveal the selected footage file in the system file manager."));
    impl_->generateProxyButton->setToolTip(QStringLiteral("Generate a proxy for the selected footage item."));
    impl_->revealProxyButton->setToolTip(QStringLiteral("Reveal the generated proxy file in the system file manager."));
    impl_->clearProxyButton->setToolTip(QStringLiteral("Clear the generated proxy for the selected footage item."));
    impl_->generateSelectedProxiesButton->setToolTip(QStringLiteral("Generate proxies for the selected footage items."));
    impl_->clearSelectedProxiesButton->setToolTip(QStringLiteral("Clear proxies for the selected footage items."));
    impl_->regenerateStaleProxiesButton->setToolTip(QStringLiteral("Regenerate stale proxies (source file has changed since generation)."));
    impl_->renameSelectionButton->setToolTip(QStringLiteral("Rename the selected item."));
    impl_->deleteSelectionButton->setToolTip(QStringLiteral("Delete the selected item."));
    impl_->relinkSelectionButton->setToolTip(QStringLiteral("Relink the selected footage item."));
    impl_->copyPathButton->setToolTip(QStringLiteral("Copy the selected item path."));
    {
        const auto tightenProjectButton = [](QPushButton* button, int minWidth = 52) {
            if (!button) {
                return;
            }
            button->setMinimumHeight(22);
            button->setMaximumHeight(22);
            button->setMinimumWidth(minWidth);
            button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            QFont font = button->font();
            font.setPointSizeF(std::max<qreal>(8.5, font.pointSizeF() > 0 ? font.pointSizeF() - 1.0 : 9.0));
            font.setBold(false);
            button->setFont(font);
            QPalette pal = button->palette();
            const auto& theme = ArtifactCore::currentDCCTheme();
            pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor).lighter(106));
            pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
            pal.setColor(QPalette::Highlight, QColor(theme.accentColor));
            button->setPalette(pal);
        };
        tightenProjectButton(impl_->openSelectionButton, 44);
        tightenProjectButton(impl_->revealSelectionButton, 50);
        tightenProjectButton(impl_->generateProxyButton, 48);
        tightenProjectButton(impl_->revealProxyButton, 76);
        tightenProjectButton(impl_->clearProxyButton, 72);
        tightenProjectButton(impl_->generateSelectedProxiesButton, 88);
        tightenProjectButton(impl_->clearSelectedProxiesButton, 86);
        tightenProjectButton(impl_->regenerateStaleProxiesButton, 78);
        tightenProjectButton(impl_->relinkSelectionButton, 52);
        tightenProjectButton(impl_->renameSelectionButton, 58);
        tightenProjectButton(impl_->deleteSelectionButton, 54);
        tightenProjectButton(impl_->copyPathButton, 66);
        QFont proxyFont = impl_->proxyGlobalToggle_->font();
        proxyFont.setPointSizeF(std::max<qreal>(8.5, proxyFont.pointSizeF() > 0 ? proxyFont.pointSizeF() - 1.0 : 9.0));
        impl_->proxyGlobalToggle_->setFont(proxyFont);
        impl_->proxyGlobalToggle_->setMaximumHeight(22);
    }
    selectionButtons->addWidget(impl_->openSelectionButton);
    selectionButtons->addWidget(impl_->revealSelectionButton);
    selectionButtons->addWidget(impl_->generateProxyButton);
    selectionButtons->addWidget(impl_->revealProxyButton);
    selectionButtons->addWidget(impl_->clearProxyButton);
    selectionButtons->addWidget(impl_->generateSelectedProxiesButton);
    selectionButtons->addWidget(impl_->clearSelectedProxiesButton);
    selectionButtons->addWidget(impl_->regenerateStaleProxiesButton);
    selectionButtons->addWidget(impl_->relinkSelectionButton);
    selectionButtons->addWidget(impl_->renameSelectionButton);
    selectionButtons->addWidget(impl_->deleteSelectionButton);
    selectionButtons->addWidget(impl_->copyPathButton);
    selectionButtons->addWidget(impl_->proxyGlobalToggle_);
    selectionButtons->addStretch();
    selectionChromeLayout->addLayout(selectionButtons);
    chromeLayout->addWidget(selectionChrome);

    impl_->compositionEditorPanel = new QWidget(chromePanel);
    impl_->compositionEditorPanel->setObjectName(QStringLiteral("projectCompositionEditorPanel"));
    auto* compositionEditorLayout = new QVBoxLayout(impl_->compositionEditorPanel);
    compositionEditorLayout->setContentsMargins(10, 0, 10, 8);
    compositionEditorLayout->setSpacing(6);

    impl_->compositionEditorTitleLabel = new QLabel(QStringLiteral("Composition Edit"), impl_->compositionEditorPanel);
    {
        QFont f = impl_->compositionEditorTitleLabel->font();
        f.setPointSize(10);
        f.setBold(true);
        impl_->compositionEditorTitleLabel->setFont(f);
        impl_->compositionEditorTitleLabel->setPalette(impl_->selectionSummaryLabel->palette());
    }
    compositionEditorLayout->addWidget(impl_->compositionEditorTitleLabel);

    impl_->compositionEditorModeLabel = new QLabel(
        QStringLiteral("Select a composition to edit its settings here."), impl_->compositionEditorPanel);
    impl_->compositionEditorModeLabel->setWordWrap(true);
    impl_->compositionEditorModeLabel->setMaximumHeight(36);
    {
        QPalette pal = impl_->compositionEditorModeLabel->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(125));
        impl_->compositionEditorModeLabel->setPalette(pal);
    }
    compositionEditorLayout->addWidget(impl_->compositionEditorModeLabel);

    auto* compositionForm = new QFormLayout();
    compositionForm->setContentsMargins(0, 0, 0, 0);
    compositionForm->setHorizontalSpacing(10);
    compositionForm->setVerticalSpacing(6);

    impl_->compositionNameEdit = new QLineEdit(impl_->compositionEditorPanel);
    impl_->compositionWidthSpin = new QSpinBox(impl_->compositionEditorPanel);
    impl_->compositionWidthSpin->setRange(1, 32768);
    impl_->compositionHeightSpin = new QSpinBox(impl_->compositionEditorPanel);
    impl_->compositionHeightSpin->setRange(1, 32768);
    impl_->compositionFrameRateSpin = new QDoubleSpinBox(impl_->compositionEditorPanel);
    impl_->compositionFrameRateSpin->setRange(1.0, 240.0);
    impl_->compositionFrameRateSpin->setDecimals(3);
    impl_->compositionFrameRateSpin->setSingleStep(0.5);
    impl_->compositionStartFrameSpin = new QSpinBox(impl_->compositionEditorPanel);
    impl_->compositionStartFrameSpin->setRange(-1000000, 1000000);
    impl_->compositionEndFrameSpin = new QSpinBox(impl_->compositionEditorPanel);
    impl_->compositionEndFrameSpin->setRange(-1000000, 1000000);
    impl_->compositionBackgroundButton = new CompositionBackgroundColorButton(QColor(0, 0, 0), impl_->compositionEditorPanel);

    auto* sizeRow = new QWidget(impl_->compositionEditorPanel);
    auto* sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(6);
    sizeLayout->addWidget(new QLabel(QStringLiteral("W"), sizeRow));
    sizeLayout->addWidget(impl_->compositionWidthSpin);
    sizeLayout->addWidget(new QLabel(QStringLiteral("H"), sizeRow));
    sizeLayout->addWidget(impl_->compositionHeightSpin);

    auto* rangeRow = new QWidget(impl_->compositionEditorPanel);
    auto* rangeLayout = new QHBoxLayout(rangeRow);
    rangeLayout->setContentsMargins(0, 0, 0, 0);
    rangeLayout->setSpacing(6);
    rangeLayout->addWidget(new QLabel(QStringLiteral("Start"), rangeRow));
    rangeLayout->addWidget(impl_->compositionStartFrameSpin);
    rangeLayout->addWidget(new QLabel(QStringLiteral("End"), rangeRow));
    rangeLayout->addWidget(impl_->compositionEndFrameSpin);

    compositionForm->addRow(QStringLiteral("Name"), impl_->compositionNameEdit);
    compositionForm->addRow(QStringLiteral("Size"), sizeRow);
    compositionForm->addRow(QStringLiteral("Frame Rate"), impl_->compositionFrameRateSpin);
    compositionForm->addRow(QStringLiteral("Range"), rangeRow);
    compositionForm->addRow(QStringLiteral("Background"), impl_->compositionBackgroundButton);
    compositionEditorLayout->addLayout(compositionForm);

    auto* compositionActionRow = new QHBoxLayout();
    compositionActionRow->setSpacing(6);
    impl_->compositionApplyButton = new QPushButton(QStringLiteral("Apply Settings"), impl_->compositionEditorPanel);
    impl_->compositionApplyFrameRateButton = new QPushButton(
        QStringLiteral("Apply FPS to Selection"), impl_->compositionEditorPanel);
    impl_->compositionApplyButton->setToolTip(
        QStringLiteral("Apply the inline composition settings to the selected composition."));
    impl_->compositionApplyFrameRateButton->setToolTip(
        QStringLiteral("Apply the frame rate to every selected composition."));
    compositionActionRow->addWidget(impl_->compositionApplyButton);
    compositionActionRow->addWidget(impl_->compositionApplyFrameRateButton);
    compositionActionRow->addStretch();
    compositionEditorLayout->addLayout(compositionActionRow);

    {
        QPalette pal = impl_->compositionNameEdit->palette();
        pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
        pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
        impl_->compositionNameEdit->setPalette(pal);
        impl_->compositionWidthSpin->setPalette(pal);
        impl_->compositionHeightSpin->setPalette(pal);
        impl_->compositionFrameRateSpin->setPalette(pal);
        impl_->compositionStartFrameSpin->setPalette(pal);
        impl_->compositionEndFrameSpin->setPalette(pal);
    }

    impl_->compositionEditorPanel->setVisible(false);
    {
        QSizePolicy policy = impl_->compositionEditorPanel->sizePolicy();
        policy.setRetainSizeWhenHidden(false);
        impl_->compositionEditorPanel->setSizePolicy(policy);
        impl_->compositionEditorPanel->setMinimumHeight(176);
    }
    chromeLayout->addWidget(impl_->compositionEditorPanel);

    impl_->searchBar = new QLineEdit(chromePanel);
    impl_->searchBar->setPlaceholderText(QStringLiteral("Search assets, tags, type:footage, unused:true"));
    impl_->searchBar->setClearButtonEnabled(true);
    {
        QFont f = impl_->searchBar->font();
        f.setPointSize(11);
        impl_->searchBar->setFont(f);
        QPalette pal = impl_->searchBar->palette();
        pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
        pal.setColor(QPalette::PlaceholderText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(145));
        pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().accentColor));
        impl_->searchBar->setPalette(pal);
    }
    chromeLayout->addWidget(impl_->searchBar);

    auto* searchGuide = new QLabel(impl_->searchBar);
    searchGuide->setTextFormat(Qt::RichText);
    searchGuide->setWordWrap(false);
    searchGuide->setTextInteractionFlags(Qt::NoTextInteraction);
    searchGuide->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    const QString accentHex = QColor(ArtifactCore::currentDCCTheme().accentColor).name();
    searchGuide->setText(QStringLiteral(
        "<span style=\"color:#9AA3AE;\">Search (</span> "
        "<span style=\"color:%1; font-weight:600;\">type:footage</span> "
        "<span style=\"color:#9AA3AE;\"> </span> "
        "<span style=\"color:%1; font-weight:600;\">tag:png</span> "
        "<span style=\"color:#9AA3AE;\"> </span> "
        "<span style=\"color:%1; font-weight:600;\">regex:shot_.*</span> "
        "<span style=\"color:#9AA3AE;\"> </span> "
        "<span style=\"color:%1; font-weight:600;\">unused:true</span>"
        "<span style=\"color:#9AA3AE;\">)...</span>")
        .arg(accentHex));
    {
        QFont f = searchGuide->font();
        f.setPointSizeF(std::max(8.5, f.pointSizeF() - 0.5));
        searchGuide->setFont(f);
        QPalette pal = searchGuide->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
        searchGuide->setPalette(pal);
    }
    QPointer<QLabel> searchGuidePtr(searchGuide);
    const auto updateSearchGuide = [searchGuidePtr](const QString& text) {
        if (!searchGuidePtr) {
            return;
        }
        const bool showGuide = text.trimmed().isEmpty();
        searchGuidePtr->setVisible(showGuide);
        if (showGuide) {
            searchGuidePtr->adjustSize();
            const QRect barRect = searchGuidePtr->parentWidget() ? searchGuidePtr->parentWidget()->rect() : QRect();
            const int x = 10;
            const int y = std::max(0, (barRect.height() - searchGuidePtr->height()) / 2);
            searchGuidePtr->move(x, y);
            searchGuidePtr->raise();
        }
    };
    updateSearchGuide(impl_->searchBar->text());
    QObject::connect(impl_->searchBar, &QLineEdit::textChanged, chromePanel, updateSearchGuide);

    auto* filterBarHost = new QWidget(chromePanel);
    filterBarHost->setObjectName(QStringLiteral("projectManagerFilterBar"));
    filterBarHost->setAutoFillBackground(true);
    auto* filterBar = new QHBoxLayout(filterBarHost);
    filterBar->setContentsMargins(10, 0, 10, 6);
    filterBar->setSpacing(8);
    impl_->typeFilterBox = new QComboBox(filterBarHost);
    impl_->typeFilterBox->addItems(QStringList() << "All" << "Composition" << "Footage" << "Folder" << "Solid");
    impl_->viewModeBox = new QComboBox(filterBarHost);
    impl_->viewModeBox->addItems(QStringList() << "List" << "Tile");
    impl_->unusedOnlyCheck = new QCheckBox("Unused only", filterBarHost);
    impl_->proxyQueueProgress = new QProgressBar(filterBarHost);
    impl_->proxyQueueProgress->setVisible(false);
    impl_->proxyQueueProgress->setTextVisible(true);
    impl_->proxyQueueProgress->setFormat("Proxy queue %v/%m");
    filterBar->addWidget(impl_->typeFilterBox);
    filterBar->addWidget(impl_->viewModeBox);
    filterBar->addWidget(impl_->unusedOnlyCheck);
    filterBar->addStretch();
    filterBar->addWidget(impl_->proxyQueueProgress, 1);
    chromeLayout->addWidget(filterBarHost);
    mainLayout->addWidget(chromePanel);

    impl_->projectView_ = new ArtifactProjectView(this);
    impl_->projectView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(impl_->projectView_);

    impl_->toolBox = new ArtifactProjectManagerToolBox(this);
    mainLayout->addWidget(impl_->toolBox);

    connect(impl_->searchBar, &QLineEdit::textChanged, [this](const QString& t) { impl_->handleSearch(t); });
    connect(impl_->typeFilterBox, &QComboBox::currentTextChanged, [this](const QString&) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->viewModeBox, &QComboBox::currentTextChanged, [this](const QString& modeText) {
        if (!impl_ || !impl_->projectView_) {
            return;
        }
        const bool tileMode = modeText.compare(QStringLiteral("Tile"), Qt::CaseInsensitive) == 0;
        impl_->projectView_->setPresentationMode(tileMode
                                                     ? ArtifactProjectView::PresentationMode::Tile
                                                     : ArtifactProjectView::PresentationMode::List);
        impl_->refreshSelectionChrome();
        scheduleProjectViewRefresh(impl_->projectView_);
    });
    connect(impl_->unusedOnlyCheck, &QCheckBox::toggled, [this](bool) {
        impl_->handleSearch(impl_->searchBar ? impl_->searchBar->text() : QString());
    });
    connect(impl_->openSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->openSelectedItem(this);
    });
    connect(impl_->revealSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->revealSelectedItem(this);
    });
    connect(impl_->generateProxyButton, &QPushButton::clicked, this, [this]() {
        impl_->generateProxyForSelectedItem();
    });
    connect(impl_->revealProxyButton, &QPushButton::clicked, this, [this]() {
        impl_->revealProxyForSelectedItem(this);
    });
    connect(impl_->clearProxyButton, &QPushButton::clicked, this, [this]() {
        impl_->clearProxyForSelectedItem(this);
    });
    connect(impl_->generateSelectedProxiesButton, &QPushButton::clicked, this, [this]() {
        for (const QString& path : impl_->selectedFootageFilePaths()) {
            impl_->generateProxyForFilePath(path);
        }
    });
    connect(impl_->clearSelectedProxiesButton, &QPushButton::clicked, this, [this]() {
        for (const QString& path : impl_->selectedFootageFilePaths()) {
            impl_->clearProxyForFilePath(path, this);
        }
    });
    connect(impl_->regenerateStaleProxiesButton, &QPushButton::clicked, this, [this]() {
        const QStringList paths = impl_->selectedFootageFilePaths();
        for (const QString& path : paths) {
            const QString proxyPath = proxyFilePathForFootage(path);
            if (proxyPath.isEmpty()) continue;
            if (!QFileInfo(proxyPath).exists()) {
                impl_->generateProxyForFilePath(path);
                continue;
            }
            const auto it = proxyMetadata().constFind(path);
            if (it != proxyMetadata().constEnd() && it->sourceLastModified.isValid()) {
                const QFileInfo src(path);
                if (src.exists() && src.lastModified() > it->sourceLastModified) {
                    impl_->generateProxyForFilePath(path);
                }
            }
        }
    });
    auto* expandAllShortcut = new QShortcut(
        ArtifactCore::ShortcutBindings::instance().shortcut(
            ArtifactCore::ShortcutId::ProjectExpandAll),
        this);
    connect(expandAllShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->projectView_) {
            impl_->projectView_->expandAll();
        }
    });
    auto* collapseAllShortcut = new QShortcut(
        ArtifactCore::ShortcutBindings::instance().shortcut(
            ArtifactCore::ShortcutId::ProjectCollapseAll),
        this);
    connect(collapseAllShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->projectView_) {
            impl_->projectView_->collapseAll();
        }
    });
    connect(impl_->renameSelectionButton, &QPushButton::clicked, this, [this]() {
        if (!impl_->renameSelectedItem(this)) {
            QMessageBox::information(this, QStringLiteral("Rename"), QStringLiteral("Select an item to rename."));
        }
    });
    connect(impl_->deleteSelectionButton, &QPushButton::clicked, this, [this]() {
        if (!impl_->deleteSelectedItem(this)) {
            QMessageBox::information(this, QStringLiteral("Delete"), QStringLiteral("Select an item to delete."));
        }
    });
    connect(impl_->relinkSelectionButton, &QPushButton::clicked, this, [this]() {
        impl_->relinkSelectedItem(this);
    });
    connect(impl_->copyPathButton, &QPushButton::clicked, this, [this]() {
        impl_->copySelectedPathToClipboard();
    });
    connect(impl_->compositionApplyButton, &QPushButton::clicked, this, [this]() {
        if (!impl_ || !impl_->applySelectedCompositionSettings()) {
            QMessageBox::information(this, QStringLiteral("Composition Edit"),
                                     QStringLiteral("Select one composition to edit its full settings."));
        }
    });
    connect(impl_->compositionApplyFrameRateButton, &QPushButton::clicked, this, [this]() {
        if (!impl_ || !impl_->applySelectedCompositionFrameRate(
                         impl_->compositionFrameRateSpin ? impl_->compositionFrameRateSpin->value() : 30.0)) {
            QMessageBox::information(this, QStringLiteral("Composition Edit"),
                                     QStringLiteral("Select one or more compositions to update their frame rate."));
        }
    });
    connect(impl_->projectView_, &ArtifactProjectView::itemSelected, [this](const QModelIndex& idx) {
        if (!impl_) {
            return;
        }
        if (impl_->syncingSelectionToComposition_) {
            return;
        }
        if (impl_->proxyModel_ && impl_->infoPanel_) {
            impl_->infoPanel_->updateInfo(impl_->proxyModel_->mapToSource(idx));
        }
        impl_->eventBus_.post<SelectionChangedEvent>(impl_->makeSelectionChangedEvent());
        (void)impl_->eventBus_.drain();
    });
    connect(impl_->projectView_, &ArtifactProjectView::itemDoubleClicked, [this](const QModelIndex& idx) {
        itemDoubleClicked(idx);
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newCompositionRequested, [this]() {
         auto dialog = new CreateCompositionDialog(this);
         if (dialog->exec()) {
             const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
             if (auto* svc = ArtifactProjectService::instance()) {
                 svc->createComposition(params);
             }
         }
         dialog->deleteLater();
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::newFolderRequested, [this]() {
         impl_->createFolderAtSelection();
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::generateProxyRequested, [this]() {
         QVector<FootageItem*> footage;
         auto* svc = ArtifactProjectService::instance();
         if (!svc) return;
         auto project = svc->getCurrentProjectSharedPtr();
         if (project) {
             const auto roots = project->projectItems();
             for (auto* root : roots) {
                 Impl::collectFootageRecursive(root, footage);
             }
         }
         impl_->queueProxyGeneration(footage);
    });
    connect(impl_->toolBox, &ArtifactProjectManagerToolBox::deleteRequested, [this]() {
         if (!impl_->deleteSelectedItem(this)) {
             QMessageBox::information(this, QStringLiteral("Delete"), QStringLiteral("Select an item to delete."));
         }
    });

    QPointer<ArtifactProjectManagerWidget> widgetPtr(this);
    const auto queueProjectRefresh = [widgetPtr]() {
        if (!widgetPtr || !widgetPtr->impl_ || widgetPtr->impl_->projectRefreshQueued_) {
            return;
        }
        widgetPtr->impl_->projectRefreshQueued_ = true;
        QMetaObject::invokeMethod(widgetPtr, [widgetPtr]() {
            if (!widgetPtr) {
                return;
            }
            widgetPtr->updateRequested();
        }, Qt::QueuedConnection);
    };

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CompositionCreatedEvent>([queueProjectRefresh](const CompositionCreatedEvent&) {
        queueProjectRefresh();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<LayerChangedEvent>([queueProjectRefresh](const LayerChangedEvent&) {
        queueProjectRefresh();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
        updateRequested();
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
        if (impl_) {
            impl_->syncSelectionToCurrentComposition();
        }
    }));
    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<SelectionChangedEvent>([this](const SelectionChangedEvent&) {
        if (impl_) {
            impl_->refreshSelectionChrome();
        }
    }));

    impl_->refreshSelectionChrome();

    auto* focusSearchShortcut = new QShortcut(
        ArtifactCore::ShortcutBindings::instance().shortcut(
            ArtifactCore::ShortcutId::ProjectFocusSearch),
        this);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->searchBar) {
            impl_->searchBar->setFocus();
            impl_->searchBar->selectAll();
        }
    });

    auto* clearSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), impl_->searchBar);
    connect(clearSearchShortcut, &QShortcut::activated, this, [this]() {
        if (impl_->searchBar && !impl_->searchBar->text().isEmpty()) {
            impl_->searchBar->clear();
        }
    });

    auto* renameShortcut = new QShortcut(
        ArtifactCore::ShortcutBindings::instance().shortcut(
            ArtifactCore::ShortcutId::ProjectRenameSelected),
        this);
    connect(renameShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->renameSelectedItem(this)) {
            return;
        }
    });

    auto* deleteShortcut = new QShortcut(
        ArtifactCore::ShortcutBindings::instance().shortcut(
            ArtifactCore::ShortcutId::ProjectDeleteSelected),
        this);
    connect(deleteShortcut, &QShortcut::activated, this, [this]() {
        if (!impl_->deleteSelectedItem(this)) {
            return;
        }
    });

    impl_->update();

}

ArtifactProjectManagerWidget::~ArtifactProjectManagerWidget() { delete impl_; }

ArtifactProjectView* ArtifactProjectManagerWidget::projectView() const
{
    return impl_ ? impl_->projectView_ : nullptr;
}

bool ArtifactProjectManagerWidget::selectItemsByFilePaths(const QStringList& filePaths)
{
    return impl_ ? impl_->selectItemsByFilePaths(filePaths) : false;
}

void ArtifactProjectManagerWidget::generateProxyForSelection()
{
    if (impl_) {
        impl_->generateProxyForSelectedItem();
        updateRequested();
    }
}

void ArtifactProjectManagerWidget::revealProxyForSelection()
{
    if (impl_) {
        impl_->revealProxyForSelectedItem(this);
    }
}

void ArtifactProjectManagerWidget::generateProxyForFilePath(const QString& sourceFilePath)
{
    if (impl_) {
        impl_->generateProxyForFilePath(sourceFilePath);
        updateRequested();
    }
}

void ArtifactProjectManagerWidget::revealProxyForFilePath(const QString& sourceFilePath)
{
    if (impl_) {
        impl_->revealProxyForFilePath(sourceFilePath, this);
    }
}

bool ArtifactProjectManagerWidget::clearProxyForFilePath(const QString& sourceFilePath)
{
    const bool ok = impl_ ? impl_->clearProxyForFilePath(sourceFilePath, this) : false;
    if (ok) {
        updateRequested();
    }
    return ok;
}

void ArtifactProjectManagerWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(0x20, 0x25, 0x2C));
    QWidget::paintEvent(event);
}

void ArtifactProjectManagerWidget::updateRequested() {
    if (!impl_) {
        return;
    }
    impl_->projectRefreshQueued_ = false;
    impl_->update();
    if (impl_) {
        impl_->refreshSelectionChrome();
    }
    setEnabled(true);
}

void ArtifactProjectManagerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (impl_ && impl_->projectView_) {
        scheduleProjectViewRefresh(impl_->projectView_);
        update();
    }
}

void ArtifactProjectManagerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!impl_) {
        return;
    }
    impl_->update();
    if (impl_->projectView_) {
        scheduleProjectViewRefresh(impl_->projectView_);
    }
}

bool ArtifactProjectManagerWidget::event(QEvent* event)
{
    if (impl_) {
        (void)impl_->eventBus_.drain();
    }
    const bool handled = QWidget::event(event);
    if (event && (event->type() == QEvent::WindowActivate ||
                  event->type() == QEvent::ActivationChange ||
                  event->type() == QEvent::PolishRequest)) {
        if (!impl_) {
            return handled;
        }
        if (impl_->projectView_ && isVisible()) {
            scheduleProjectViewRefresh(impl_->projectView_);
        }
        update();
    }
    return handled;
}

void ArtifactProjectManagerWidget::triggerUpdate() { impl_->update(); }
void ArtifactProjectManagerWidget::setFilter() {
    if (!impl_ || !impl_->searchBar) return;
    impl_->searchBar->setFocus();
    impl_->searchBar->selectAll();
}
void ArtifactProjectManagerWidget::setThumbnailEnabled(bool b) {
    if (!impl_ || !impl_->infoPanel_) return;
    impl_->thumbnailEnabled = b;
    impl_->infoPanel_->thumbnail->setVisible(b);
}
void ArtifactProjectManagerWidget::dropEvent(QDropEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::dragEnterEvent(QDragEnterEvent* event) { /* Handled by child view but kept for API */ }
void ArtifactProjectManagerWidget::contextMenuEvent(QContextMenuEvent* event) { /* Handled by child view but kept for API */ }
QSize ArtifactProjectManagerWidget::sizeHint() const { return QSize(250, 600); }

// --- ToolBox Implementation ---
ArtifactProjectManagerToolBox::ArtifactProjectManagerToolBox(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("projectManagerToolBox"));
    setAutoFillBackground(true);
    setFixedHeight(28);
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(5);

    auto createBtn = [](const QString& tip, const QString& iconPath, QStyle::StandardPixmap fallbackIcon, const QString& fallbackText) {
        auto b = new QPushButton();
        b->setFixedSize(22, 22);
        b->setToolTip(tip);
        QIcon icon = loadProjectViewIcon(iconPath);
        if ((icon.isNull() || icon.pixmap(16, 16).isNull()) && QApplication::style()) {
            icon = QApplication::style()->standardIcon(fallbackIcon);
        }
        if (!icon.isNull() && !icon.pixmap(16, 16).isNull()) {
            b->setIcon(icon);
        } else {
            b->setText(fallbackText);
        }
        b->setIconSize(QSize(15, 15));
        b->setFlat(true);
        {
            QPalette pal = b->palette();
            pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
            b->setPalette(pal);
        }
        return b;
    };

    auto btnNew = createBtn("New Composition", QStringLiteral("Studio/composition.svg"), QStyle::SP_FileDialogNewFolder, QStringLiteral("N"));
    auto btnFolder = createBtn("New Folder", QStringLiteral("Studio/folder.svg"), QStyle::SP_DirIcon, QStringLiteral("F"));
    auto btnProxy = createBtn("Generate Proxies", QStringLiteral("Studio/replay.svg"), QStyle::SP_BrowserReload, QStringLiteral("P"));
    auto btnDel = createBtn("Delete", QStringLiteral("Studio/delete.svg"), QStyle::SP_TrashIcon, QStringLiteral("D"));

    layout->addWidget(btnNew);
    layout->addWidget(btnFolder);
    layout->addWidget(btnProxy);
    layout->addStretch();
    layout->addWidget(btnDel);

    connect(btnNew, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::newCompositionRequested);
    connect(btnFolder, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::newFolderRequested);
    connect(btnProxy, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::generateProxyRequested);
    connect(btnDel, &QPushButton::clicked, this, &ArtifactProjectManagerToolBox::deleteRequested);
}

ArtifactProjectManagerToolBox::~ArtifactProjectManagerToolBox() {}
void ArtifactProjectManagerToolBox::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
    QWidget::paintEvent(event);
}
void ArtifactProjectManagerToolBox::resizeEvent(QResizeEvent*) {}

}
