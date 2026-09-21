module;
#include <utility>
#include <functional>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QFont>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QPushButton>
#include <QVBoxLayout>
module Artifact.Widgets.AppDialogs;

import Widgets.Utils.CSS;

namespace Artifact {

namespace {

class RenameLineEdit final : public QLineEdit {
public:
    using Callback = std::function<void()>;

    explicit RenameLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {}
    void setChangeCallback(Callback callback) { callback_ = std::move(callback); }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        QLineEdit::keyPressEvent(event);
        if (callback_) callback_();
    }

    void inputMethodEvent(QInputMethodEvent* event) override
    {
        QLineEdit::inputMethodEvent(event);
        if (callback_) callback_();
    }

private:
    Callback callback_;
};

class RenameActionButton final : public QPushButton {
public:
    using Callback = std::function<void()>;

    explicit RenameActionButton(const QString& text, QWidget* parent = nullptr)
        : QPushButton(text, parent) {}
    void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
    void nextCheckState() override
    {
        QPushButton::nextCheckState();
        if (callback_) callback_();
    }

private:
    Callback callback_;
};

class RenameDialog final : public QDialog {
public:
    RenameDialog(QWidget* parent,
                 ArtifactRenameTarget target,
                 const QString& currentName,
                 const QString& contextText,
                 const QString& detailText,
                 const QStringList& unavailableNames)
        : QDialog(parent),
          initialName_(currentName.trimmed()),
          unavailableNames_(unavailableNames)
    {
        const QString targetName = target == ArtifactRenameTarget::Composition
            ? QStringLiteral("Composition")
            : target == ArtifactRenameTarget::Layer
                ? QStringLiteral("Layer")
                : QStringLiteral("Item");
        setWindowTitle(QStringLiteral("Rename %1").arg(targetName));
        setModal(true);
        setMinimumWidth(480);
        setSizeGripEnabled(false);

        const auto& theme = ArtifactCore::currentDCCTheme();
        const QColor background(theme.backgroundColor);
        const QColor secondary(theme.secondaryBackgroundColor);
        const QColor text(theme.textColor);
        QPalette dialogPalette = palette();
        dialogPalette.setColor(QPalette::Window, background);
        dialogPalette.setColor(QPalette::Base, secondary.darker(112));
        dialogPalette.setColor(QPalette::Text, text);
        dialogPalette.setColor(QPalette::WindowText, text);
        dialogPalette.setColor(QPalette::Button, secondary.lighter(106));
        dialogPalette.setColor(QPalette::ButtonText, text);
        dialogPalette.setColor(QPalette::Highlight, QColor(theme.accentColor));
        dialogPalette.setColor(QPalette::HighlightedText, Qt::white);
        setPalette(dialogPalette);
        setAutoFillBackground(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 20, 24, 20);
        root->setSpacing(10);

        auto* title = new QLabel(QStringLiteral("Rename %1").arg(targetName), this);
        QFont titleFont = title->font();
        titleFont.setPointSize(titleFont.pointSize() + 2);
        titleFont.setBold(true);
        title->setFont(titleFont);
        root->addWidget(title);

        if (!contextText.trimmed().isEmpty()) {
            auto* context = new QLabel(contextText.trimmed(), this);
            QPalette muted = context->palette();
            muted.setColor(QPalette::WindowText, text.darker(135));
            context->setPalette(muted);
            root->addWidget(context);
        }

        root->addSpacing(6);
        auto* nameLabel = new QLabel(QStringLiteral("Name"), this);
        root->addWidget(nameLabel);

        nameEdit_ = new RenameLineEdit(this);
        nameEdit_->setText(currentName);
        nameEdit_->setMinimumHeight(34);
        nameEdit_->setAccessibleName(QStringLiteral("New %1 name").arg(targetName.toLower()));
        nameLabel->setBuddy(nameEdit_);
        root->addWidget(nameEdit_);

        validationLabel_ = new QLabel(this);
        validationLabel_->setWordWrap(true);
        QPalette validationPalette = validationLabel_->palette();
        validationPalette.setColor(QPalette::WindowText, QColor(235, 88, 88));
        validationLabel_->setPalette(validationPalette);
        validationLabel_->setVisible(false);
        root->addWidget(validationLabel_);

        const QString helperText = target == ArtifactRenameTarget::Composition
            ? QStringLiteral("Used in Project and Timeline tabs.")
            : target == ArtifactRenameTarget::Layer
                ? QStringLiteral("The layer source and effects are unchanged.")
                : QStringLiteral("Only the displayed project item name changes.");
        auto* helper = new QLabel(helperText, this);
        QPalette helperPalette = helper->palette();
        helperPalette.setColor(QPalette::WindowText, text.darker(135));
        helper->setPalette(helperPalette);
        root->addWidget(helper);

        if (!detailText.trimmed().isEmpty()) {
            auto* detail = new QLabel(detailText.trimmed(), this);
            detail->setWordWrap(true);
            root->addWidget(detail);
        }

        auto* separator = new QFrame(this);
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Plain);
        root->addSpacing(8);
        root->addWidget(separator);

        auto* actions = new QHBoxLayout();
        actions->setContentsMargins(0, 4, 0, 0);
        actions->setSpacing(10);
        actions->addStretch(1);
        auto* cancel = new RenameActionButton(QStringLiteral("Cancel"), this);
        renameButton_ = new RenameActionButton(QStringLiteral("Rename"), this);
        cancel->setMinimumSize(112, 34);
        renameButton_->setMinimumSize(112, 34);
        renameButton_->setDefault(true);
        renameButton_->setAutoDefault(true);
        actions->addWidget(cancel);
        actions->addWidget(renameButton_);
        root->addLayout(actions);

        cancel->setCallback([this]() { reject(); });
        renameButton_->setCallback([this]() {
            refreshValidation();
            if (renameButton_->isEnabled()) accept();
        });
        nameEdit_->setChangeCallback([this]() { refreshValidation(); });
        refreshValidation();
        nameEdit_->setFocus(Qt::TabFocusReason);
        nameEdit_->selectAll();
    }

    QString selectedName() const { return nameEdit_ ? nameEdit_->text().trimmed() : QString(); }

private:
    void refreshValidation()
    {
        const QString candidate = selectedName();
        QString error;
        if (candidate.isEmpty()) {
            error = QStringLiteral("Enter a name to continue.");
        } else if (candidate.compare(initialName_, Qt::CaseSensitive) != 0) {
            for (const QString& unavailable : unavailableNames_) {
                if (candidate.compare(unavailable.trimmed(), Qt::CaseInsensitive) == 0) {
                    error = QStringLiteral("An item named “%1” already exists.").arg(candidate);
                    break;
                }
            }
        }
        validationLabel_->setText(error);
        validationLabel_->setVisible(!error.isEmpty());
        renameButton_->setEnabled(error.isEmpty() && candidate != initialName_);
    }

    QString initialName_;
    QStringList unavailableNames_;
    RenameLineEdit* nameEdit_ = nullptr;
    QLabel* validationLabel_ = nullptr;
    RenameActionButton* renameButton_ = nullptr;
};

} // namespace

bool ArtifactMessageBox::confirmDelete(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Warning);
    
    // Standardizing the buttons
    QPushButton* deleteButton = box.addButton("削除", QMessageBox::DestructiveRole);
    QPushButton* cancelButton = box.addButton("キャンセル", QMessageBox::RejectRole);
    box.setDefaultButton(cancelButton);

    box.exec();
    return box.clickedButton() == deleteButton;
}

bool ArtifactMessageBox::confirmOverwrite(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(QMessageBox::Warning);
    
    QPushButton* overwriteButton = box.addButton("上書き", QMessageBox::AcceptRole);
    QPushButton* cancelButton = box.addButton("キャンセル", QMessageBox::RejectRole);
    box.setDefaultButton(cancelButton);

    box.exec();
    return box.clickedButton() == overwriteButton;
}

bool ArtifactMessageBox::confirmAction(QWidget* parent, const QString& title, const QString& text)
{
    const auto answer = QMessageBox::question(
        parent,
        title,
        text,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    return answer == QMessageBox::Yes;
}

QString ArtifactRenameDialog::getName(QWidget* parent,
                                      ArtifactRenameTarget target,
                                      const QString& currentName,
                                      const QString& contextText,
                                      const QString& detailText,
                                      const QStringList& unavailableNames,
                                      bool* accepted)
{
    RenameDialog dialog(parent, target, currentName, contextText, detailText, unavailableNames);
    const bool didAccept = dialog.exec() == QDialog::Accepted;
    if (accepted) *accepted = didAccept;
    return didAccept ? dialog.selectedName() : currentName;
}

} // namespace Artifact
