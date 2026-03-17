module;
#include <QMessageBox>
#include <QWidget>
#include <QString>
#include <QPushButton>

module Artifact.Widgets.AppDialogs;

namespace Artifact {

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

} // namespace Artifact
