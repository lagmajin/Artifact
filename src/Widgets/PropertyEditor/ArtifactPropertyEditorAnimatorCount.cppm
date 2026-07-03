module;
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVariant>
#include <QWidget>
#include <memory>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.RelativeSpinBox;

namespace Artifact {
using namespace detail;

    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyAnimatorCountEditor"));

  const auto meta = property.metadata();
  minCount_ = meta.hardMin.isValid() ? meta.hardMin.toInt() : 0;
  maxCount_ = meta.hardMax.isValid() ? meta.hardMax.toInt() : 16;
  if (maxCount_ < minCount_) {
    std::swap(maxCount_, minCount_);
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  removeButton_ = new PropertyCallbackButton(QStringLiteral("-"), this);
  removeButton_->setToolTip(QStringLiteral("Remove last animator"));
  removeButton_->setFixedHeight(24);
  removeButton_->setMinimumWidth(28);
  applyPropertyButtonPalette(removeButton_, false);
  static_cast<PropertyCallbackButton *>(removeButton_)->setCallback(
      [this]() { stepCount(-1); });

  countLabel_ = new QLabel(this);
  countLabel_->setAlignment(Qt::AlignCenter);
  countLabel_->setMinimumHeight(24);
  countLabel_->setMinimumWidth(92);
  countLabel_->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  applyPropertyFieldPalette(countLabel_, true);
  applyPropertyLabelPalette(countLabel_, true);

  addButton_ = new PropertyCallbackButton(QStringLiteral("+"), this);
  addButton_->setToolTip(QStringLiteral("Add animator (Click to select type)"));
  addButton_->setFixedHeight(24);
  addButton_->setMinimumWidth(28);
  applyPropertyButtonPalette(addButton_, true);
  static_cast<PropertyCallbackButton *>(addButton_)->setCallback(
      [this]() {
        QMenu menu(this);
        QAction *defaultAct = menu.addAction(QStringLiteral("Default Animator"));
        menu.addSeparator();
        QAction *typewriterAct = menu.addAction(QStringLiteral("Typewriter Preset"));
        QAction *slideUpAct = menu.addAction(QStringLiteral("Slide Up Preset"));
        QAction *scaleInAct = menu.addAction(QStringLiteral("Scale In Preset"));
        QAction *rotationInAct = menu.addAction(QStringLiteral("Rotation In Preset"));
        QAction *trackingFadeAct = menu.addAction(QStringLiteral("Tracking Fade Preset"));
        QAction *wigglyPositionAct = menu.addAction(QStringLiteral("Wiggly Position Preset"));
        QAction *blurRevealAct = menu.addAction(QStringLiteral("Blur Reveal Preset"));

        defaultAct->setToolTip(QStringLiteral("Standard animator with blank settings"));
        typewriterAct->setToolTip(QStringLiteral("Typewriter animation: scale, opacity, tracking, blur"));
        slideUpAct->setToolTip(QStringLiteral("Slide up animation: position, opacity"));
        scaleInAct->setToolTip(QStringLiteral("Scale in animation: scale, opacity"));
        rotationInAct->setToolTip(QStringLiteral("Rotation in animation: scale, rotation, opacity"));
        trackingFadeAct->setToolTip(QStringLiteral("Tracking fade animation: tracking, scale, opacity"));
        wigglyPositionAct->setToolTip(QStringLiteral("Wiggly selector: smooth position and rotation wiggles"));
        blurRevealAct->setToolTip(QStringLiteral("Blur reveal animation: scale, opacity, blur"));

        QAction *chosen = menu.exec(addButton_->mapToGlobal(QPoint(0, addButton_->height())));
        if (!chosen) {
          return;
        }

        const int nextCount = std::clamp(currentCount_ + 1, minCount_, maxCount_);
        if (nextCount == currentCount_) {
          return;
        }
        currentCount_ = nextCount;
        syncUi();

        int presetId = 0;
        if (chosen == typewriterAct) presetId = 1;
        else if (chosen == slideUpAct) presetId = 2;
        else if (chosen == scaleInAct) presetId = 3;
        else if (chosen == rotationInAct) presetId = 4;
        else if (chosen == trackingFadeAct) presetId = 5;
        else if (chosen == wigglyPositionAct) presetId = 6;
        else if (chosen == blurRevealAct) presetId = 7;

        if (presetId > 0) {
          commitValue((presetId * 100) + currentCount_);
        } else {
          commitValue(currentCount_);
        }
      });

  layout->addWidget(removeButton_, 0);
  layout->addWidget(countLabel_, 1);
  layout->addWidget(addButton_, 0);

  setValueFromVariant(property.getValue());
}

QVariant ArtifactAnimatorCountPropertyEditor::value() const {
  return currentCount_;
}

void ArtifactAnimatorCountPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  currentCount_ = std::clamp(value.toInt(), minCount_, maxCount_);
  syncUi();
}

void ArtifactAnimatorCountPropertyEditor::stepCount(const int delta) {
  const int nextCount =
      std::clamp(currentCount_ + delta, minCount_, maxCount_);
  if (nextCount == currentCount_) {
    return;
  }
  currentCount_ = nextCount;
  syncUi();
  commitValue(currentCount_);
}

void ArtifactAnimatorCountPropertyEditor::syncUi() {
  if (countLabel_) {
    countLabel_->setText(
        QStringLiteral("%1 animator%2")
            .arg(currentCount_)
            .arg(currentCount_ == 1 ? QStringLiteral("") : QStringLiteral("s")));
  }
  if (removeButton_) {
    removeButton_->setEnabled(currentCount_ > minCount_);
  }
  if (addButton_) {
    addButton_->setEnabled(currentCount_ < maxCount_);
  }
}

ArtifactDashPatternPropertyEditor::ArtifactDashPatternPropertyEditor(
} // namespace Artifact
