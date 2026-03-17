module;

#include <wobjectimpl.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolTip>

module Artifact.Widgets.ObjectReference;

import std;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Project.Items;

namespace Artifact {

W_OBJECT_IMPL(ArtifactObjectReferenceWidget)

ArtifactObjectReferenceWidget::ArtifactObjectReferenceWidget(QWidget* parent)
    : QWidget(parent), currentId_(ArtifactCore::LayerID(ArtifactCore::Id::Nil()))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    
    // ○ ピッカーボタン
    pickButton_ = new QPushButton(QStringLiteral("○"), this);
    pickButton_->setFixedSize(24, 24);
    pickButton_->setToolTip(QStringLiteral("オブジェクトを選択"));
    layout->addWidget(pickButton_);
    
    // 参照 ID 表示
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setReadOnly(true);
    nameEdit_->setPlaceholderText(QStringLiteral("None"));
    layout->addWidget(nameEdit_, 1);
    
    // クリアボタン
    clearButton_ = new QPushButton(QStringLiteral("×"), this);
    clearButton_->setFixedSize(24, 24);
    clearButton_->setToolTip(QStringLiteral("参照をクリア"));
    layout->addWidget(clearButton_);
    
    // シグナル接続
    connect(pickButton_, &QPushButton::clicked, this, &ArtifactObjectReferenceWidget::onPickButtonClicked);
    connect(clearButton_, &QPushButton::clicked, this, &ArtifactObjectReferenceWidget::onClearButtonClicked);
    
    updateDisplay();
}

ArtifactObjectReferenceWidget::~ArtifactObjectReferenceWidget()
{
}

void ArtifactObjectReferenceWidget::setReferenceType(const QString& typeName)
{
    referenceType_ = typeName;
}

void ArtifactObjectReferenceWidget::setCurrentReferenceId(const ArtifactCore::LayerID& id)
{
    currentId_ = id;
    updateDisplay();
}

void ArtifactObjectReferenceWidget::setAllowNull(bool allow)
{
    allowNull_ = allow;
    clearButton_->setEnabled(allow && !currentId_.isNil());
}

ArtifactCore::LayerID ArtifactObjectReferenceWidget::currentReferenceId() const
{
    return currentId_;
}

QString ArtifactObjectReferenceWidget::referenceType() const
{
    return referenceType_;
}

bool ArtifactObjectReferenceWidget::allowNull() const
{
    return allowNull_;
}

void ArtifactObjectReferenceWidget::onPickButtonClicked()
{
    Q_EMIT referencePicked();
    // ObjectPickerDialog は呼び出し側で表示
}

void ArtifactObjectReferenceWidget::onClearButtonClicked()
{
    if (allowNull_) {
        currentId_ = ArtifactCore::LayerID(ArtifactCore::Id::Nil());
        updateDisplay();
        Q_EMIT referenceCleared();
        Q_EMIT referenceChanged(currentId_);
    }
}

void ArtifactObjectReferenceWidget::updateDisplay()
{
    if (currentId_.isNil()) {
        nameEdit_->setText(QStringLiteral("None"));
        nameEdit_->setStyleSheet(QStringLiteral("color: gray;"));
        clearButton_->setEnabled(false);
    } else {
        nameEdit_->setText(currentId_.toString());
        nameEdit_->setStyleSheet(QString());
        clearButton_->setEnabled(allowNull_);
        
        // 名前解決（オプション）
        auto* service = ArtifactProjectService::instance();
        if (service) {
            auto comp = service->currentComposition().lock();
            if (comp) {
                auto layer = comp->layerById(currentId_);
                if (layer) {
                    nameEdit_->setText(layer->layerName() + QStringLiteral(" (ID: %1)").arg(currentId_.toString()));
                    nameEdit_->setStyleSheet(QString());
                }
            }
        }
    }
}

void ArtifactObjectReferenceWidget::referenceChanged(const ArtifactCore::LayerID& newId) {}
void ArtifactObjectReferenceWidget::referenceCleared() {}
void ArtifactObjectReferenceWidget::referencePicked() {}

} // namespace Artifact
