module;

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>

export module Artifact.Widgets.ProjectResponsiveLayout;

import Artifact.Composition.Abstract;
import Artifact.Project.Manager;
import Undo.UndoManager;

export namespace Artifact {

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

bool applyResponsiveLayoutWithUndo(const ArtifactCompositionPtr& composition,
                                   const ResponsiveLayoutSet& before,
                                   const ResponsiveLayoutSet& after)
{
    if (!composition) return false;
    const QJsonObject beforeJson = before.toJson();
    const QJsonObject afterJson = after.toJson();
    if (beforeJson == afterJson) return true;
    if (auto* undo = UndoManager::instance()) {
        return undo->push(std::make_unique<SetCompositionResponsiveLayoutCommand>(
            composition, beforeJson, afterJson));
    }
    composition->setResponsiveLayout(after);
    if (composition->responsiveLayout().toJson() != afterJson) return false;
    if (auto project = ArtifactProjectManager::getInstance().getCurrentProjectSharedPtr()) {
        project->projectChanged();
    }
    return true;
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
    nameEdit->setObjectName(QStringLiteral("projectVariantNameEdit"));
    nameEdit->setObjectName(QStringLiteral("projectVariantNameEdit"));
    nameEdit->setText(variant->displayName.isEmpty() ? variant->variantId : variant->displayName);
    form->addRow(QStringLiteral("Name"), nameEdit);

    auto* idEdit = new QLineEdit(&dialog);
    idEdit->setObjectName(QStringLiteral("projectVariantIdEdit"));
    idEdit->setObjectName(QStringLiteral("projectVariantIdEdit"));
    idEdit->setText(variant->variantId);
    form->addRow(QStringLiteral("Variant ID"), idEdit);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setObjectName(QStringLiteral("projectVariantWidthSpin"));
    widthSpin->setObjectName(QStringLiteral("projectVariantWidthSpin"));
    widthSpin->setRange(1, 32768);
    widthSpin->setValue(std::max(1, variant->baseSize.width()));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setObjectName(QStringLiteral("projectVariantHeightSpin"));
    heightSpin->setObjectName(QStringLiteral("projectVariantHeightSpin"));
    heightSpin->setRange(1, 32768);
    heightSpin->setValue(std::max(1, variant->baseSize.height()));
    form->addRow(QStringLiteral("Width"), widthSpin);
    form->addRow(QStringLiteral("Height"), heightSpin);

    auto* enabledCheck = new QCheckBox(QStringLiteral("Enabled"), &dialog);
    enabledCheck->setObjectName(QStringLiteral("projectVariantEnabledCheck"));
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
    nameEdit->setObjectName(QStringLiteral("projectDraftNameEdit"));
    nameEdit->setObjectName(QStringLiteral("projectDraftNameEdit"));
    nameEdit->setText(draft.displayName);
    form->addRow(QStringLiteral("Name"), nameEdit);

    auto* idEdit = new QLineEdit(&dialog);
    idEdit->setObjectName(QStringLiteral("projectDraftIdEdit"));
    idEdit->setObjectName(QStringLiteral("projectDraftIdEdit"));
    idEdit->setText(draft.variantId);
    form->addRow(QStringLiteral("Variant ID"), idEdit);

    auto* widthSpin = new QSpinBox(&dialog);
    widthSpin->setObjectName(QStringLiteral("projectDraftWidthSpin"));
    widthSpin->setObjectName(QStringLiteral("projectDraftWidthSpin"));
    widthSpin->setRange(1, 32768);
    widthSpin->setValue(std::max(1, draft.baseSize.width()));
    auto* heightSpin = new QSpinBox(&dialog);
    heightSpin->setObjectName(QStringLiteral("projectDraftHeightSpin"));
    heightSpin->setObjectName(QStringLiteral("projectDraftHeightSpin"));
    heightSpin->setRange(1, 32768);
    heightSpin->setValue(std::max(1, draft.baseSize.height()));
    form->addRow(QStringLiteral("Width"), widthSpin);
    form->addRow(QStringLiteral("Height"), heightSpin);

    auto* enabledCheck = new QCheckBox(QStringLiteral("Enabled"), &dialog);
    enabledCheck->setObjectName(QStringLiteral("projectDraftEnabledCheck"));
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

} // namespace Artifact
