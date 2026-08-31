module;

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QPoint>
#include <QRectF>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QTextEdit>
#include <QObject>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>

export module Artifact.Widgets.CompositionTextEditor;

import Memory.SharedPtr;
import Artifact.Widgets.CompositionRenderController;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Artifact.Event.Types;
import Event.Bus;
import Time.Rational;
import Undo.UndoManager;
import Utils.String.UniString;
import Widgets.Utils.CSS;

export namespace Artifact {
bool editTextLayerInline(QWidget *parent, const ArtifactAbstractLayerPtr &layer,
                         CompositionRenderController *controller);
}

namespace Artifact {
namespace {
int compositionFrameRateScale(const ArtifactAbstractComposition *composition) {
  if (!composition) {
    return 30;
  }
  const double fps = composition->frameRate().framerate();
  if (!std::isfinite(fps) || fps <= 0.0) {
    return 30;
  }
  return std::max(1, static_cast<int>(std::llround(
      std::clamp(fps, 1.0, 10000.0))));
}

qint64 textEditFrame(const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer) {
  if (!layer) {
    return 0;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return composition->framePosition().framePosition();
  }
  return 0;
}

QString textEditorValue(const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer) {
  if (!layer) {
    return {};
  }
  return layer->hasSourceTextKeyframes()
             ? layer->sourceTextAtFrame(textEditFrame(layer))
             : layer->text().toQString();
}

bool commitTextEditorValue(const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer,
                           const QString &nextText) {
  if (!layer) {
    return false;
  }

  const QString beforeText = textEditorValue(layer);
  if (beforeText == nextText) {
    return false;
  }

  if (layer->hasSourceTextKeyframes()) {
    const auto property = layer->getProperty(QStringLiteral("text.value"));
    if (!property) {
      return false;
    }
    const auto beforeKeyframes = property->getKeyFrames();
    auto afterKeyframes = beforeKeyframes;
    const qint64 frame = textEditFrame(layer);
    const auto *composition = static_cast<ArtifactAbstractComposition *>(
        layer->composition());
    const int fps = compositionFrameRateScale(composition);
    ArtifactCore::KeyFrame editedKeyframe;
    editedKeyframe.time = RationalTime(frame, fps);
    editedKeyframe.value = nextText;
    editedKeyframe.interpolation = ArtifactCore::InterpolationType::Constant;
    const auto existing = std::find_if(
        afterKeyframes.begin(), afterKeyframes.end(),
        [&editedKeyframe](const ArtifactCore::KeyFrame &keyframe) {
          return keyframe.time == editedKeyframe.time;
        });
    if (existing != afterKeyframes.end()) {
      *existing = editedKeyframe;
    } else {
      afterKeyframes.push_back(editedKeyframe);
    }
    auto* manager = UndoManager::instance();
    if (manager) {
      if (!manager->push(std::make_unique<SetLayerPropertyKeyframesCommand>(
              layer, QStringLiteral("text.value"), beforeKeyframes,
              afterKeyframes, QStringLiteral("Edit Source Text")))) {
        return false;
      }
    } else {
      layer->setSourceTextAtFrame(frame, nextText);
      if (layer->sourceTextAtFrame(frame) != nextText) {
        return false;
      }
    }
  } else {
    auto* manager = UndoManager::instance();
    if (manager) {
      if (!manager->push(std::make_unique<SetTextLayerTextCommand>(
              layer, beforeText, nextText, QStringLiteral("Edit Text")))) {
        return false;
      }
    } else {
      if (!layer->setLayerPropertyValue(QStringLiteral("text.value"),
                                         nextText)) {
        return false;
      }
    }
  }
  return true;
}

class TextOverlayFilter : public QObject {
public:
  TextOverlayFilter(QPlainTextEdit *editor,
                    ArtifactCore::SharedPtr<ArtifactTextLayer> layer,
                    CompositionRenderController *ctrl)
      : QObject(editor), editor_(editor), layer_(layer), ctrl_(ctrl) {}

  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::InputMethod) {
      // Keep the native Qt input-method path alive while Microsoft IME is
      // composing.  In particular, Ctrl+Enter/Escape must not close the
      // overlay before the preedit has been committed or cancelled by Qt.
      const auto *imeEvent = static_cast<const QInputMethodEvent *>(event);
      imePreeditActive_ = !imeEvent->preeditString().isEmpty();
    } else if (event->type() == QEvent::KeyPress) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (imePreeditActive_ &&
          (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter ||
           ke->key() == Qt::Key_Escape)) {
        return QObject::eventFilter(obj, event);
      }
      if (isCancelKey(ke)) {
        cancel();
        return true;
      }
      if (isCommitKey(ke)) {
        commit();
        return true;
      }
    } else if (event->type() == QEvent::FocusOut) {
      commit();
      return false;
    }
    return QObject::eventFilter(obj, event);
  }

private:
  void commit() {
    if (!editor_)
      return;
    if (commitTextEditorValue(layer_, editor_->toPlainText())) {
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(
              layer_->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer_->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
      if (ctrl_)
        ctrl_->markRenderDirty();
    }
    editor_->hide();
    editor_->deleteLater();
    editor_ = nullptr;
  }

  void cancel() {
    if (!editor_) {
      return;
    }
    editor_->hide();
    editor_->deleteLater();
    editor_ = nullptr;
  }

  bool isCommitKey(QKeyEvent *ke) const {
    return ke && ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
                  (ke->modifiers() & Qt::ControlModifier));
  }

  bool isCancelKey(QKeyEvent *ke) const {
    return ke && ke->key() == Qt::Key_Escape;
  }

  void finishInlineEdit(bool commitChanges) {
    if (commitChanges) {
      commit();
    } else {
      cancel();
    }
  }

  QPlainTextEdit *editor_;
  ArtifactCore::SharedPtr<ArtifactTextLayer> layer_;
  CompositionRenderController *ctrl_;
  bool imePreeditActive_ = false;
};

class ArtifactTextEditorDialog final : public QDialog {
public:
  ArtifactTextEditorDialog(const ArtifactAbstractLayerPtr &layer,
                           CompositionRenderController *controller,
                           QWidget *parent = nullptr)
      : QDialog(parent), layer_(layer), controller_(controller) {
    setWindowTitle(QStringLiteral("Text Editor"));
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    const auto theme = ArtifactCore::currentDCCTheme();
    captureInitialState(textLayer);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto *header = new QLabel(this);
    header->setText(textLayer ? QStringLiteral("Text Layer Editor") : QStringLiteral("Text Editor"));
    header->setFont(font());
    root->addWidget(header);

    auto *summary = new QLabel(this);
    summary->setWordWrap(true);
    summary->setText(editorSummaryText(textLayer));
    root->addWidget(summary);

    auto *preview = new QFrame(this);
    preview->setObjectName(QStringLiteral("compositionPreviewFrame"));
    preview->setFrameShape(QFrame::StyledPanel);
    preview->setFrameShadow(QFrame::Plain);
    preview->setMinimumHeight(180);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    preview->setAutoFillBackground(true);
    QPalette previewPalette = preview->palette();
    previewPalette.setColor(QPalette::Window,
                            QColor(theme.secondaryBackgroundColor));
    previewPalette.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
    previewPalette.setColor(QPalette::Text, QColor(theme.textColor));
    preview->setPalette(previewPalette);
    preview->installEventFilter(this);
    preview_ = preview;
    root->addWidget(preview, 1);

    editor_ = new QTextEdit(this);
    const QString initialText = textEditorValue(textLayer);
    richText_ = Qt::mightBeRichText(initialText);
    if (richText_) {
      editor_->setHtml(initialText);
    } else {
      editor_->setPlainText(initialText);
    }
    editor_->setPlaceholderText(QStringLiteral("Enter text..."));
    editor_->selectAll();
    editor_->setMinimumHeight(160);
    editor_->setTabChangesFocus(false);

    QFont editorFont = editor_->font();
    if (textLayer) {
      editorFont.setFamily(textLayer->fontFamily().toQString());
      editorFont.setPointSizeF(std::max(11.0f, textLayer->fontSize()));
    } else {
      editorFont.setPointSizeF(std::max(11.0, editorFont.pointSizeF()));
    }
    editor_->setFont(editorFont);

    QPalette editorPalette = editor_->palette();
    editorPalette.setColor(QPalette::Base, QColor(theme.backgroundColor));
    editorPalette.setColor(QPalette::Text, QColor(theme.textColor));
    editorPalette.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor));
    editor_->setPalette(editorPalette);

    editor_->installEventFilter(this);
    root->addWidget(editor_);

    auto *typeRow = new QHBoxLayout();
    auto addMetric = [this, typeRow](const QString &label, double value,
                                     double minimum, double maximum,
                                     double step, QDoubleSpinBox **out) {
      typeRow->addWidget(new QLabel(label, this));
      auto *spin = new QDoubleSpinBox(this);
      spin->setRange(minimum, maximum);
      spin->setSingleStep(step);
      spin->setValue(value);
      spin->setDecimals(2);
      typeRow->addWidget(spin);
      *out = spin;
    };
    if (textLayer) {
      addMetric(QStringLiteral("Size"), textLayer->fontSize(), 1.0, 1000.0,
                1.0, &fontSizeSpin_);
      addMetric(QStringLiteral("Tracking"), textLayer->tracking(), -500.0,
                500.0, 1.0, &trackingSpin_);
      addMetric(QStringLiteral("Leading"), textLayer->leading(), -1000.0,
                1000.0, 1.0, &leadingSpin_);
      addMetric(QStringLiteral("Stretch"), textLayer->fontStretch(), 50.0,
                200.0, 1.0, &stretchSpin_);
      fontSizeSpin_->installEventFilter(this);
      trackingSpin_->installEventFilter(this);
      stretchSpin_->installEventFilter(this);
    }
    typeRow->addStretch(1);
    root->addLayout(typeRow);

    if (textLayer) {
      auto *characterRow = new QHBoxLayout();
      characterRow->addWidget(new QLabel(QStringLiteral("Font"), this));
      fontFamilyCombo_ = new QComboBox(this);
      const QString currentFamily = textLayer->fontFamily().toQString();
      const QStringList families = {currentFamily, QStringLiteral("Arial"),
                                    QStringLiteral("Segoe UI"),
                                    QStringLiteral("Noto Sans"),
                                    QStringLiteral("sans-serif")};
      for (const auto &family : families) {
        if (fontFamilyCombo_->findText(family) < 0) {
          fontFamilyCombo_->addItem(family);
        }
      }
      fontFamilyCombo_->setCurrentText(currentFamily);
      fontFamilyCombo_->installEventFilter(this);
      characterRow->addWidget(fontFamilyCombo_, 1);
      characterRow->addWidget(new QLabel(QStringLiteral("Align"), this));
      alignmentCombo_ = new QComboBox(this);
      alignmentCombo_->addItem(QStringLiteral("Left"), 0);
      alignmentCombo_->addItem(QStringLiteral("Center"), 1);
      alignmentCombo_->addItem(QStringLiteral("Right"), 2);
      alignmentCombo_->addItem(QStringLiteral("Justify"), 3);
      alignmentCombo_->setCurrentIndex(
          static_cast<int>(textLayer->horizontalAlignment()));
      alignmentCombo_->installEventFilter(this);
      characterRow->addWidget(alignmentCombo_);
      root->addLayout(characterRow);

      auto *flagsRow = new QHBoxLayout();
      boldCheck_ = new QCheckBox(QStringLiteral("Bold"), this);
      italicCheck_ = new QCheckBox(QStringLiteral("Italic"), this);
      allCapsCheck_ = new QCheckBox(QStringLiteral("All Caps"), this);
      underlineCheck_ = new QCheckBox(QStringLiteral("Underline"), this);
      strikethroughCheck_ = new QCheckBox(QStringLiteral("Strike"), this);
      boldCheck_->setChecked(textLayer->isBold());
      italicCheck_->setChecked(textLayer->isItalic());
      boldCheck_->installEventFilter(this);
      italicCheck_->installEventFilter(this);
      allCapsCheck_->installEventFilter(this);
      underlineCheck_->installEventFilter(this);
      strikethroughCheck_->installEventFilter(this);
      allCapsCheck_->setChecked(textLayer->isAllCaps());
      underlineCheck_->setChecked(textLayer->isUnderline());
      strikethroughCheck_->setChecked(textLayer->isStrikethrough());
      flagsRow->addWidget(boldCheck_);
      flagsRow->addWidget(italicCheck_);
      flagsRow->addWidget(allCapsCheck_);
      flagsRow->addWidget(underlineCheck_);
      flagsRow->addWidget(strikethroughCheck_);
      flagsRow->addStretch(1);
      root->addLayout(flagsRow);

      auto *effectsRow = new QHBoxLayout();
      strokeCheck_ = new QCheckBox(QStringLiteral("Stroke"), this);
      strokeWidthSpin_ = new QDoubleSpinBox(this);
      strokeWidthSpin_->setRange(0.0, 500.0);
      strokeWidthSpin_->setSingleStep(0.5);
      strokeWidthSpin_->setDecimals(1);
      strokeCheck_->setChecked(textLayer->isStrokeEnabled());
      strokeWidthSpin_->setValue(textLayer->strokeWidth());
      strokeCheck_->installEventFilter(this);
      strokeWidthSpin_->installEventFilter(this);
      effectsRow->addWidget(strokeCheck_);
      effectsRow->addWidget(new QLabel(QStringLiteral("Width"), this));
      effectsRow->addWidget(strokeWidthSpin_);
      shadowCheck_ = new QCheckBox(QStringLiteral("Shadow"), this);
      shadowBlurSpin_ = new QDoubleSpinBox(this);
      shadowBlurSpin_->setRange(0.0, 500.0);
      shadowBlurSpin_->setSingleStep(1.0);
      shadowBlurSpin_->setDecimals(1);
      shadowCheck_->setChecked(textLayer->isShadowEnabled());
      shadowBlurSpin_->setValue(textLayer->shadowBlur());
      shadowCheck_->installEventFilter(this);
      shadowBlurSpin_->installEventFilter(this);
      effectsRow->addWidget(shadowCheck_);
      effectsRow->addWidget(new QLabel(QStringLiteral("Blur"), this));
      effectsRow->addWidget(shadowBlurSpin_);
      effectsRow->addStretch(1);
      root->addLayout(effectsRow);

      auto *layoutRow = new QHBoxLayout();
      layoutRow->addWidget(new QLabel(QStringLiteral("Layout"), this));
      layoutModeCombo_ = new QComboBox(this);
      layoutModeCombo_->addItem(QStringLiteral("Point"), 0);
      layoutModeCombo_->addItem(QStringLiteral("Box"), 1);
      layoutModeCombo_->addItem(QStringLiteral("Path"), 2);
      layoutModeCombo_->setCurrentIndex(static_cast<int>(textLayer->layoutMode()));
      layoutModeCombo_->installEventFilter(this);
      layoutRow->addWidget(layoutModeCombo_);
      layoutRow->addWidget(new QLabel(QStringLiteral("Wrap"), this));
      wrapModeCombo_ = new QComboBox(this);
      wrapModeCombo_->addItem(QStringLiteral("None"), 0);
      wrapModeCombo_->addItem(QStringLiteral("Words"), 1);
      wrapModeCombo_->addItem(QStringLiteral("Anywhere"), 2);
      wrapModeCombo_->addItem(QStringLiteral("Manual"), 3);
      wrapModeCombo_->setCurrentIndex(static_cast<int>(textLayer->wrapMode()));
      wrapModeCombo_->installEventFilter(this);
      layoutRow->addWidget(wrapModeCombo_);
      layoutRow->addWidget(new QLabel(QStringLiteral("V Align"), this));
      verticalAlignmentCombo_ = new QComboBox(this);
      verticalAlignmentCombo_->addItem(QStringLiteral("Top"), 0);
      verticalAlignmentCombo_->addItem(QStringLiteral("Middle"), 1);
      verticalAlignmentCombo_->addItem(QStringLiteral("Bottom"), 2);
      verticalAlignmentCombo_->setCurrentIndex(
          static_cast<int>(textLayer->verticalAlignment()));
      verticalAlignmentCombo_->installEventFilter(this);
      layoutRow->addWidget(verticalAlignmentCombo_);
      layoutRow->addWidget(new QLabel(QStringLiteral("Direction"), this));
      writingModeCombo_ = new QComboBox(this);
      writingModeCombo_->addItem(QStringLiteral("Horizontal"), 0);
      writingModeCombo_->addItem(QStringLiteral("Vertical"), 1);
      writingModeCombo_->setCurrentIndex(static_cast<int>(textLayer->writingMode()));
      writingModeCombo_->installEventFilter(this);
      layoutRow->addWidget(writingModeCombo_);
      layoutRow->addStretch(1);
      root->addLayout(layoutRow);

      auto *paragraphRow = new QHBoxLayout();
      auto addParagraphMetric = [this, paragraphRow](const QString& label,
                                                       double value,
                                                       double minimum,
                                                       double maximum,
                                                       QDoubleSpinBox** out) {
        paragraphRow->addWidget(new QLabel(label, this));
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(1.0);
        spin->setDecimals(1);
        spin->setValue(value);
        spin->installEventFilter(this);
        paragraphRow->addWidget(spin);
        *out = spin;
      };
      addParagraphMetric(QStringLiteral("Width"), textLayer->maxWidth(), 0.0,
                         100000.0, &boxWidthSpin_);
      addParagraphMetric(QStringLiteral("Height"), textLayer->boxHeight(), 0.0,
                         100000.0, &boxHeightSpin_);
      addParagraphMetric(QStringLiteral("Paragraph"), textLayer->paragraphSpacing(),
                         -1000.0, 1000.0, &paragraphSpacingSpin_);
      addParagraphMetric(QStringLiteral("Shadow X"), textLayer->shadowOffsetX(),
                         -10000.0, 10000.0, &shadowOffsetXSpin_);
      addParagraphMetric(QStringLiteral("Y"), textLayer->shadowOffsetY(),
                         -10000.0, 10000.0, &shadowOffsetYSpin_);
      paragraphRow->addStretch(1);
      root->addLayout(paragraphRow);

      auto *animatorRow = new QHBoxLayout();
      animatorRow->addWidget(new QLabel(QStringLiteral("Text Animators"), this));
      animatorCountSpin_ = new QSpinBox(this);
      animatorCountSpin_->setRange(0, 16);
      animatorCountSpin_->setValue(textLayer->animatorCount());
      animatorCountSpin_->setAccessibleName(QStringLiteral("Text animator count"));
      animatorCountSpin_->setAccessibleDescription(
          QStringLiteral("Set the number of text animator stacks on this layer"));
      animatorCountSpin_->setToolTip(
          QStringLiteral("Number of character animator stacks attached to this text layer"));
      animatorCountSpin_->installEventFilter(this);
      animatorRow->addWidget(animatorCountSpin_);
      animatorRow->addWidget(new QLabel(QStringLiteral("Preset"), this));
      animatorPresetCombo_ = new QComboBox(this);
      animatorPresetCombo_->addItem(QStringLiteral("Keep current"), -1);
      animatorPresetCombo_->addItem(QStringLiteral("None"), 0);
      animatorPresetCombo_->addItem(QStringLiteral("Typewriter"), 1);
      animatorPresetCombo_->addItem(QStringLiteral("Slide Up"), 2);
      animatorPresetCombo_->addItem(QStringLiteral("Scale In"), 3);
      animatorPresetCombo_->addItem(QStringLiteral("Rotation In"), 4);
      animatorPresetCombo_->addItem(QStringLiteral("Tracking Fade"), 5);
      animatorPresetCombo_->addItem(QStringLiteral("Wiggly Position"), 6);
      animatorPresetCombo_->addItem(QStringLiteral("Blur Reveal"), 7);
      animatorPresetCombo_->setAccessibleName(QStringLiteral("Text animator preset"));
      animatorPresetCombo_->setAccessibleDescription(
          QStringLiteral("Choose a text animator preset to apply when accepted"));
      animatorPresetCombo_->setToolTip(
          QStringLiteral("Apply a text animator preset when the dialog is accepted"));
      animatorPresetCombo_->installEventFilter(this);
      animatorRow->addWidget(animatorPresetCombo_, 1);
      animatorRow->addStretch(1);
      root->addLayout(animatorRow);
    }

    setMinimumSize(680, 520);
    resize(900, 680);
  }

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if ((obj == fontFamilyCombo_ || obj == fontSizeSpin_ ||
         obj == trackingSpin_ || obj == stretchSpin_ ||
         obj == boldCheck_ || obj == italicCheck_ || obj == allCapsCheck_ ||
         obj == underlineCheck_ || obj == strikethroughCheck_ ||
         obj == alignmentCombo_ || obj == layoutModeCombo_ ||
         obj == wrapModeCombo_ || obj == verticalAlignmentCombo_ ||
         obj == writingModeCombo_ || obj == boxWidthSpin_ ||
         obj == boxHeightSpin_ || obj == paragraphSpacingSpin_ ||
         obj == shadowOffsetXSpin_ || obj == shadowOffsetYSpin_ ||
         obj == strokeCheck_ || obj == strokeWidthSpin_ ||
         obj == shadowCheck_ || obj == shadowBlurSpin_ ||
         obj == animatorCountSpin_ || obj == animatorPresetCombo_) &&
        (event->type() == QEvent::KeyRelease ||
         event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::Wheel ||
         event->type() == QEvent::FocusIn)) {
      queueLivePreview();
      return QDialog::eventFilter(obj, event);
    }
    if (obj == editor_) {
      if (event->type() == QEvent::InputMethod) {
        const auto *imeEvent = static_cast<const QInputMethodEvent *>(event);
        imePreeditActive_ = !imeEvent->preeditString().isEmpty();
      } else if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (imePreeditActive_ &&
            (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter ||
             ke->key() == Qt::Key_Escape)) {
          return QDialog::eventFilter(obj, event);
        }
        if (ke->key() == Qt::Key_Escape) {
          reject();
          return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            (ke->modifiers() & Qt::ControlModifier)) {
          accept();
          return true;
        }
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) &&
            !(ke->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
          const auto textLayer =
              ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
          if (textLayer && textLayer->layoutMode() == TextLayoutMode::Point) {
            accept();
            return true;
          }
        }
      } else if (event->type() == QEvent::KeyRelease) {
        applyLivePreview();
      } else if (event->type() == QEvent::FocusOut) {
        accept();
        return false;
      }
    } else if (obj == preview_ && event->type() == QEvent::Paint) {
      paintPreview(static_cast<QWidget *>(obj));
      return true;
    }
    return QDialog::eventFilter(obj, event);
  }

  void accept() override {
    restoreInitialState();
    commit();
    QDialog::accept();
  }

  void reject() override {
    restoreInitialState();
    QDialog::reject();
  }

private:
  struct TextEditorState {
    QString text;
    std::vector<ArtifactCore::KeyFrame> sourceKeyframes;
    float fontSize = 12.0f;
    float tracking = 0.0f;
    float leading = 0.0f;
    float stretch = 100.0f;
    QString family;
    ArtifactCore::TextHorizontalAlignment alignment =
        ArtifactCore::TextHorizontalAlignment::Left;
    bool bold = false;
    bool italic = false;
    bool allCaps = false;
    bool underline = false;
    bool strikethrough = false;
    bool stroke = false;
    float strokeWidth = 0.0f;
    bool shadow = false;
    float shadowBlur = 0.0f;
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 0.0f;
    float boxWidth = 1.0f;
    float boxHeight = 1.0f;
    float paragraphSpacing = 0.0f;
    TextLayoutMode layoutMode = TextLayoutMode::Point;
    ArtifactCore::TextWrapMode wrapMode = ArtifactCore::TextWrapMode::WordWrap;
    ArtifactCore::TextVerticalAlignment verticalAlignment =
        ArtifactCore::TextVerticalAlignment::Top;
    ArtifactCore::TextWritingMode writingMode =
        ArtifactCore::TextWritingMode::Horizontal;
    int animatorCount = 0;
  };

  void captureInitialState(
      const ArtifactCore::SharedPtr<ArtifactTextLayer>& textLayer) {
    if (!textLayer) {
      return;
    }
    initialState_.text = textLayer->text().toQString();
    if (const auto property = textLayer->getProperty(QStringLiteral("text.value"))) {
      initialState_.sourceKeyframes = property->getKeyFrames();
    }
    initialState_.fontSize = textLayer->fontSize();
    initialState_.tracking = textLayer->tracking();
    initialState_.leading = textLayer->leading();
    initialState_.stretch = textLayer->fontStretch();
    initialState_.family = textLayer->fontFamily().toQString();
    initialState_.alignment = textLayer->horizontalAlignment();
    initialState_.bold = textLayer->isBold();
    initialState_.italic = textLayer->isItalic();
    initialState_.allCaps = textLayer->isAllCaps();
    initialState_.underline = textLayer->isUnderline();
    initialState_.strikethrough = textLayer->isStrikethrough();
    initialState_.stroke = textLayer->isStrokeEnabled();
    initialState_.strokeWidth = textLayer->strokeWidth();
    initialState_.shadow = textLayer->isShadowEnabled();
    initialState_.shadowBlur = textLayer->shadowBlur();
    initialState_.shadowOffsetX = textLayer->shadowOffsetX();
    initialState_.shadowOffsetY = textLayer->shadowOffsetY();
    initialState_.boxWidth = textLayer->maxWidth();
    initialState_.boxHeight = textLayer->boxHeight();
    initialState_.paragraphSpacing = textLayer->paragraphSpacing();
    initialState_.layoutMode = textLayer->layoutMode();
    initialState_.wrapMode = textLayer->wrapMode();
    initialState_.verticalAlignment = textLayer->verticalAlignment();
    initialState_.writingMode = textLayer->writingMode();
    initialState_.animatorCount = textLayer->animatorCount();
  }

  void restoreInitialState() {
    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    if (!textLayer) {
      return;
    }
    textLayer->setText(UniString(initialState_.text));
    if (const auto property = textLayer->getProperty(QStringLiteral("text.value"))) {
      property->clearKeyFrames();
      for (const auto& keyframe : initialState_.sourceKeyframes) {
        property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation);
      }
    }
    textLayer->setFontSize(initialState_.fontSize);
    textLayer->setTracking(initialState_.tracking);
    textLayer->setLeading(initialState_.leading);
    textLayer->setFontStretch(initialState_.stretch);
    textLayer->setFontFamily(UniString(initialState_.family));
    textLayer->setHorizontalAlignment(initialState_.alignment);
    textLayer->setBold(initialState_.bold);
    textLayer->setItalic(initialState_.italic);
    textLayer->setAllCaps(initialState_.allCaps);
    textLayer->setUnderline(initialState_.underline);
    textLayer->setStrikethrough(initialState_.strikethrough);
    textLayer->setStrokeEnabled(initialState_.stroke);
    textLayer->setStrokeWidth(initialState_.strokeWidth);
    textLayer->setShadowEnabled(initialState_.shadow);
    textLayer->setShadowBlur(initialState_.shadowBlur);
    textLayer->setShadowOffset(initialState_.shadowOffsetX,
                               initialState_.shadowOffsetY);
    textLayer->setMaxWidth(initialState_.boxWidth);
    textLayer->setBoxHeight(initialState_.boxHeight);
    textLayer->setParagraphSpacing(initialState_.paragraphSpacing);
    textLayer->setLayoutMode(initialState_.layoutMode);
    textLayer->setWrapMode(initialState_.wrapMode);
    textLayer->setVerticalAlignment(initialState_.verticalAlignment);
    textLayer->setWritingMode(initialState_.writingMode);
    textLayer->setAnimatorCount(initialState_.animatorCount);
    textLayer->setDirty();
    textLayer->changed();
    if (controller_) {
      controller_->markRenderDirty();
    }
  }

  void applyLivePreview() {
    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    if (!textLayer || !editor_) {
      return;
    }
    if (initialState_.sourceKeyframes.empty()) {
      textLayer->setText(UniString(richText_ ? editor_->toHtml()
                                              : editor_->toPlainText()));
    } else if (const auto property = textLayer->getProperty(QStringLiteral("text.value"))) {
      property->clearKeyFrames();
      for (const auto& keyframe : initialState_.sourceKeyframes) {
        property->addKeyFrame(keyframe.time, keyframe.value, keyframe.interpolation);
      }
      textLayer->setSourceTextAtFrame(
          textEditFrame(textLayer), richText_ ? editor_->toHtml()
                                               : editor_->toPlainText());
    }
    if (fontSizeSpin_) textLayer->setFontSize(static_cast<float>(fontSizeSpin_->value()));
    if (trackingSpin_) textLayer->setTracking(static_cast<float>(trackingSpin_->value()));
    if (leadingSpin_) textLayer->setLeading(static_cast<float>(leadingSpin_->value()));
    if (stretchSpin_) textLayer->setFontStretch(static_cast<float>(stretchSpin_->value()));
    if (fontFamilyCombo_) textLayer->setFontFamily(UniString(fontFamilyCombo_->currentText()));
    if (alignmentCombo_) textLayer->setHorizontalAlignment(
        static_cast<ArtifactCore::TextHorizontalAlignment>(alignmentCombo_->currentData().toInt()));
    if (boldCheck_) textLayer->setBold(boldCheck_->isChecked());
    if (italicCheck_) textLayer->setItalic(italicCheck_->isChecked());
    if (allCapsCheck_) textLayer->setAllCaps(allCapsCheck_->isChecked());
    if (underlineCheck_) textLayer->setUnderline(underlineCheck_->isChecked());
    if (strikethroughCheck_) textLayer->setStrikethrough(strikethroughCheck_->isChecked());
    if (strokeCheck_) textLayer->setStrokeEnabled(strokeCheck_->isChecked());
    if (strokeWidthSpin_) textLayer->setStrokeWidth(static_cast<float>(strokeWidthSpin_->value()));
    if (shadowCheck_) textLayer->setShadowEnabled(shadowCheck_->isChecked());
    if (shadowBlurSpin_) textLayer->setShadowBlur(static_cast<float>(shadowBlurSpin_->value()));
    if (shadowOffsetXSpin_ || shadowOffsetYSpin_) {
      textLayer->setShadowOffset(
          shadowOffsetXSpin_ ? static_cast<float>(shadowOffsetXSpin_->value())
                             : textLayer->shadowOffsetX(),
          shadowOffsetYSpin_ ? static_cast<float>(shadowOffsetYSpin_->value())
                             : textLayer->shadowOffsetY());
    }
    if (boxWidthSpin_) textLayer->setMaxWidth(static_cast<float>(boxWidthSpin_->value()));
    if (boxHeightSpin_) textLayer->setBoxHeight(static_cast<float>(boxHeightSpin_->value()));
    if (paragraphSpacingSpin_) textLayer->setParagraphSpacing(
        static_cast<float>(paragraphSpacingSpin_->value()));
    if (layoutModeCombo_) textLayer->setLayoutMode(
        static_cast<TextLayoutMode>(layoutModeCombo_->currentData().toInt()));
    if (wrapModeCombo_) textLayer->setWrapMode(
        static_cast<ArtifactCore::TextWrapMode>(wrapModeCombo_->currentData().toInt()));
    if (verticalAlignmentCombo_) textLayer->setVerticalAlignment(
        static_cast<ArtifactCore::TextVerticalAlignment>(
            verticalAlignmentCombo_->currentData().toInt()));
    if (writingModeCombo_) textLayer->setWritingMode(
        static_cast<ArtifactCore::TextWritingMode>(writingModeCombo_->currentData().toInt()));
    textLayer->setDirty();
    textLayer->changed();
    if (preview_) {
      preview_->update();
    }
    if (controller_) {
      controller_->markRenderDirty();
    }
  }

  void queueLivePreview() {
    QTimer::singleShot(0, this, [this]() { applyLivePreview(); });
  }

  static QString editorSummaryText(const ArtifactCore::SharedPtr<ArtifactTextLayer> &textLayer) {
    if (!textLayer) {
      return QStringLiteral("No text layer selected.");
    }
    const QRectF bbox = textLayer->transformedBoundingBox();
    return QStringLiteral("%1 | layout=%2 | box=%3x%4 | mode=%5")
        .arg(textLayer->fontFamily().toQString())
        .arg(textLayer->layoutMode() == TextLayoutMode::Point
                 ? QStringLiteral("Point")
                 : textLayer->layoutMode() == TextLayoutMode::Box
                       ? QStringLiteral("Box")
                       : QStringLiteral("Path"))
        .arg(bbox.width(), 0, 'f', 1)
        .arg(bbox.height(), 0, 'f', 1)
        .arg(textLayer->writingMode() == TextWritingMode::Vertical
                 ? QStringLiteral("Vertical")
                 : QStringLiteral("Horizontal"));
  }

  void paintPreview(QWidget *widget) {
    if (!widget) {
      return;
    }
    QPainter painter(widget);
    painter.fillRect(widget->rect(), widget->palette().window());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    const QRectF inner = widget->rect().adjusted(20, 20, -20, -20);
    painter.setPen(QPen(QColor(120, 160, 220, 220), 1.5));
    painter.drawRoundedRect(inner, 8, 8);

    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0, Qt::DashLine));
    painter.drawLine(inner.left(), inner.center().y(), inner.right(), inner.center().y());

    painter.setPen(QColor(240, 240, 245));
    QFont previewFont = font();
    if (fontFamilyCombo_ && !fontFamilyCombo_->currentText().trimmed().isEmpty()) {
      previewFont.setFamily(fontFamilyCombo_->currentText().trimmed());
    }
    if (fontSizeSpin_) {
      previewFont.setPointSizeF(std::clamp(fontSizeSpin_->value() * 0.22, 8.0, 42.0));
    }
    if (boldCheck_) {
      previewFont.setBold(boldCheck_->isChecked());
    }
    if (italicCheck_) {
      previewFont.setItalic(italicCheck_->isChecked());
    }
    if (trackingSpin_) {
      previewFont.setLetterSpacing(QFont::AbsoluteSpacing,
                                   trackingSpin_->value() * 0.12);
    }
    if (stretchSpin_) {
      previewFont.setStretch(static_cast<int>(std::clamp(
          stretchSpin_->value(), 50.0, 200.0)));
    }
    if (underlineCheck_) {
      previewFont.setUnderline(underlineCheck_->isChecked());
    }
    painter.setFont(previewFont);
    QString title = textLayer ? textEditorValue(textLayer).trimmed()
                              : QStringLiteral("Text editor shell");
    if (title.isEmpty()) {
      title = QStringLiteral("Diligent text surface shell");
    }
    if (title.size() > 180) {
      title = title.left(177) + QStringLiteral("…");
    }
    if (allCapsCheck_ && allCapsCheck_->isChecked()) {
      title = title.toUpper();
    }
    Qt::Alignment titleAlignment = Qt::AlignLeft | Qt::AlignTop;
    if (alignmentCombo_) {
      switch (alignmentCombo_->currentData().toInt()) {
      case 1: titleAlignment = Qt::AlignHCenter | Qt::AlignTop; break;
      case 2: titleAlignment = Qt::AlignRight | Qt::AlignTop; break;
      case 3: titleAlignment = Qt::AlignJustify | Qt::AlignTop; break;
      default: break;
      }
    }
    const QRectF titleRect = inner.adjusted(14, 10, -14, -10);
    const auto toQColor = [](const ArtifactCore::FloatColor &color) {
      return QColor::fromRgbF(std::clamp(color.r(), 0.0f, 1.0f),
                              std::clamp(color.g(), 0.0f, 1.0f),
                              std::clamp(color.b(), 0.0f, 1.0f),
                              std::clamp(color.a(), 0.0f, 1.0f));
    };
    const QColor fillColor = textLayer ? toQColor(textLayer->textColor())
                                       : QColor(240, 240, 245);
    if (shadowCheck_ && shadowCheck_->isChecked()) {
      const qreal blur = shadowBlurSpin_ ? shadowBlurSpin_->value() : 0.0;
      const int alpha = std::clamp(150 - static_cast<int>(blur * 3.0), 35, 150);
      QColor shadowColor = textLayer ? toQColor(textLayer->shadowColor())
                                     : QColor(0, 0, 0);
      shadowColor.setAlpha(std::min(shadowColor.alpha(), alpha));
      painter.setPen(shadowColor);
      painter.drawText(titleRect.translated(3.0 + blur * 0.04,
                                            3.0 + blur * 0.04),
                       titleAlignment, title);
    }
    if (strokeCheck_ && strokeCheck_->isChecked()) {
      const qreal width = strokeWidthSpin_ ? strokeWidthSpin_->value() * 0.08 : 1.0;
      QPen outline(textLayer ? toQColor(textLayer->strokeColor())
                            : QColor(40, 80, 140, 230));
      outline.setWidthF(std::clamp(width, 0.5, 6.0));
      painter.setPen(outline);
      painter.drawText(titleRect, titleAlignment, title);
    }
    painter.setPen(fillColor);
    painter.drawText(titleRect, titleAlignment, title);

    if (textLayer) {
      const QRectF bbox = textLayer->transformedBoundingBox();
      const QString detail = QStringLiteral("bbox %1 x %2  |  text length %3")
                                 .arg(bbox.width(), 0, 'f', 1)
                                 .arg(bbox.height(), 0, 'f', 1)
                                 .arg(textEditorValue(textLayer).size());
      painter.setPen(QColor(180, 200, 220));
      painter.drawText(inner.adjusted(14, 44, -14, -10),
                       Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, detail);
    }
  }

  void commit() {
    if (!editor_) {
      return;
    }

    const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer_);
    if (!textLayer) {
      return;
    }

    const float beforeFontSize = textLayer->fontSize();
    const float beforeTracking = textLayer->tracking();
    const float beforeLeading = textLayer->leading();
    const float beforeStretch = textLayer->fontStretch();
    const QString beforeFamily = textLayer->fontFamily().toQString();
    const auto beforeAlignment = textLayer->horizontalAlignment();
    const bool beforeBold = textLayer->isBold();
    const bool beforeItalic = textLayer->isItalic();
    const bool beforeAllCaps = textLayer->isAllCaps();
    const bool beforeUnderline = textLayer->isUnderline();
    const bool beforeStrikethrough = textLayer->isStrikethrough();
    const bool beforeStroke = textLayer->isStrokeEnabled();
    const float beforeStrokeWidth = textLayer->strokeWidth();
    const bool beforeShadow = textLayer->isShadowEnabled();
    const float beforeShadowBlur = textLayer->shadowBlur();
    const float beforeShadowOffsetX = textLayer->shadowOffsetX();
    const float beforeShadowOffsetY = textLayer->shadowOffsetY();
    const float beforeBoxWidth = textLayer->maxWidth();
    const float beforeBoxHeight = textLayer->boxHeight();
    const float beforeParagraphSpacing = textLayer->paragraphSpacing();
    const auto beforeLayoutMode = textLayer->layoutMode();
    const auto beforeWrapMode = textLayer->wrapMode();
    const auto beforeVerticalAlignment = textLayer->verticalAlignment();
    const auto beforeWritingMode = textLayer->writingMode();
    const QJsonArray beforeAnimatorStack = textLayer->textAnimatorStackSnapshot();
    bool animatorStackApplied = false;
    if (fontSizeSpin_) textLayer->setFontSize(static_cast<float>(fontSizeSpin_->value()));
    if (trackingSpin_) textLayer->setTracking(static_cast<float>(trackingSpin_->value()));
    if (leadingSpin_) textLayer->setLeading(static_cast<float>(leadingSpin_->value()));
    if (stretchSpin_) textLayer->setFontStretch(static_cast<float>(stretchSpin_->value()));
    if (fontFamilyCombo_) {
      textLayer->setFontFamily(UniString(fontFamilyCombo_->currentText()));
    }
    if (alignmentCombo_) {
      textLayer->setHorizontalAlignment(
          static_cast<ArtifactCore::TextHorizontalAlignment>(
              alignmentCombo_->currentData().toInt()));
    }
    if (boldCheck_) textLayer->setBold(boldCheck_->isChecked());
    if (italicCheck_) textLayer->setItalic(italicCheck_->isChecked());
    if (allCapsCheck_) textLayer->setAllCaps(allCapsCheck_->isChecked());
    if (underlineCheck_) textLayer->setUnderline(underlineCheck_->isChecked());
    if (strikethroughCheck_) textLayer->setStrikethrough(strikethroughCheck_->isChecked());
    if (strokeCheck_) textLayer->setStrokeEnabled(strokeCheck_->isChecked());
    if (strokeWidthSpin_) {
      textLayer->setStrokeWidth(static_cast<float>(strokeWidthSpin_->value()));
    }
    if (shadowCheck_) textLayer->setShadowEnabled(shadowCheck_->isChecked());
    if (shadowBlurSpin_) {
      textLayer->setShadowBlur(static_cast<float>(shadowBlurSpin_->value()));
    }
    if (shadowOffsetXSpin_ || shadowOffsetYSpin_) {
      textLayer->setShadowOffset(
          shadowOffsetXSpin_ ? static_cast<float>(shadowOffsetXSpin_->value())
                             : beforeShadowOffsetX,
          shadowOffsetYSpin_ ? static_cast<float>(shadowOffsetYSpin_->value())
                             : beforeShadowOffsetY);
    }
    if (boxWidthSpin_) textLayer->setMaxWidth(static_cast<float>(boxWidthSpin_->value()));
    if (boxHeightSpin_) textLayer->setBoxHeight(static_cast<float>(boxHeightSpin_->value()));
    if (paragraphSpacingSpin_) textLayer->setParagraphSpacing(
        static_cast<float>(paragraphSpacingSpin_->value()));
    if (layoutModeCombo_) textLayer->setLayoutMode(
        static_cast<TextLayoutMode>(layoutModeCombo_->currentData().toInt()));
    if (wrapModeCombo_) textLayer->setWrapMode(
        static_cast<ArtifactCore::TextWrapMode>(wrapModeCombo_->currentData().toInt()));
    if (verticalAlignmentCombo_) textLayer->setVerticalAlignment(
        static_cast<ArtifactCore::TextVerticalAlignment>(
            verticalAlignmentCombo_->currentData().toInt()));
    if (writingModeCombo_) textLayer->setWritingMode(
        static_cast<ArtifactCore::TextWritingMode>(writingModeCombo_->currentData().toInt()));
    if (animatorCountSpin_) {
      textLayer->setAnimatorCount(animatorCountSpin_->value());
    }
    if (animatorPresetCombo_) {
      const int presetId = animatorPresetCombo_->currentData().toInt();
      if (presetId >= 0) {
        textLayer->setLayerPropertyValue(QStringLiteral("text.animatorPreset"),
                                         presetId);
      }
    }
    const QJsonArray afterAnimatorStack = textLayer->textAnimatorStackSnapshot();
    if (beforeAnimatorStack != afterAnimatorStack) {
      if (auto *mgr = UndoManager::instance()) {
        animatorStackApplied = mgr->push(std::make_unique<SetTextAnimatorStackCommand>(
            textLayer, beforeAnimatorStack, afterAnimatorStack,
            QStringLiteral("Edit Text Animators")));
        if (!animatorStackApplied) {
          textLayer->restoreTextAnimatorStack(beforeAnimatorStack);
        }
      } else {
        animatorStackApplied = true;
      }
    }
    const bool styleChanged = beforeFontSize != textLayer->fontSize() ||
                              beforeTracking != textLayer->tracking() ||
                              beforeLeading != textLayer->leading() ||
                              beforeStretch != textLayer->fontStretch() ||
                              beforeFamily != textLayer->fontFamily().toQString() ||
                              beforeAlignment != textLayer->horizontalAlignment() ||
                              beforeBold != textLayer->isBold() ||
                              beforeItalic != textLayer->isItalic() ||
                              beforeAllCaps != textLayer->isAllCaps() ||
                              beforeUnderline != textLayer->isUnderline() ||
                              beforeStrikethrough != textLayer->isStrikethrough() ||
                              beforeStroke != textLayer->isStrokeEnabled() ||
                              beforeStrokeWidth != textLayer->strokeWidth() ||
                              beforeShadow != textLayer->isShadowEnabled() ||
                              beforeShadowBlur != textLayer->shadowBlur() ||
                              beforeShadowOffsetX != textLayer->shadowOffsetX() ||
                              beforeShadowOffsetY != textLayer->shadowOffsetY() ||
                              beforeBoxWidth != textLayer->maxWidth() ||
                              beforeBoxHeight != textLayer->boxHeight() ||
                              beforeParagraphSpacing != textLayer->paragraphSpacing() ||
                              beforeLayoutMode != textLayer->layoutMode() ||
                              beforeWrapMode != textLayer->wrapMode() ||
                              beforeVerticalAlignment != textLayer->verticalAlignment() ||
                              beforeWritingMode != textLayer->writingMode();

    bool styleApplied = styleChanged;
    if (styleChanged) {
      const QVector<QPair<QString, QVariant>> beforeStyleValues = {
          {QStringLiteral("text.fontSize"), QVariant(beforeFontSize)},
          {QStringLiteral("text.tracking"), QVariant(beforeTracking)},
          {QStringLiteral("text.leading"), QVariant(beforeLeading)},
          {QStringLiteral("text.fontStretch"), QVariant(beforeStretch)},
          {QStringLiteral("text.fontFamily"), QVariant(beforeFamily)},
          {QStringLiteral("text.alignment"),
           QVariant(static_cast<int>(beforeAlignment))},
          {QStringLiteral("text.bold"), QVariant(beforeBold)},
          {QStringLiteral("text.italic"), QVariant(beforeItalic)},
          {QStringLiteral("text.allCaps"), QVariant(beforeAllCaps)},
          {QStringLiteral("text.underline"), QVariant(beforeUnderline)},
          {QStringLiteral("text.strikethrough"), QVariant(beforeStrikethrough)},
          {QStringLiteral("text.strokeEnabled"), QVariant(beforeStroke)},
          {QStringLiteral("text.strokeWidth"), QVariant(beforeStrokeWidth)},
          {QStringLiteral("text.shadowEnabled"), QVariant(beforeShadow)},
          {QStringLiteral("text.shadowBlur"), QVariant(beforeShadowBlur)},
          {QStringLiteral("text.shadowOffsetX"), QVariant(beforeShadowOffsetX)},
          {QStringLiteral("text.shadowOffsetY"), QVariant(beforeShadowOffsetY)},
          {QStringLiteral("text.maxWidth"), QVariant(beforeBoxWidth)},
          {QStringLiteral("text.boxHeight"), QVariant(beforeBoxHeight)},
          {QStringLiteral("text.paragraphSpacing"),
           QVariant(beforeParagraphSpacing)},
          {QStringLiteral("text.layoutMode"),
           QVariant(static_cast<int>(beforeLayoutMode))},
          {QStringLiteral("text.wrapMode"),
           QVariant(static_cast<int>(beforeWrapMode))},
          {QStringLiteral("text.verticalAlignment"),
           QVariant(static_cast<int>(beforeVerticalAlignment))},
          {QStringLiteral("text.writingMode"),
           QVariant(static_cast<int>(beforeWritingMode))}};
      const QVector<QPair<QString, QVariant>> afterStyleValues = {
          {QStringLiteral("text.fontSize"), QVariant(textLayer->fontSize())},
          {QStringLiteral("text.tracking"), QVariant(textLayer->tracking())},
          {QStringLiteral("text.leading"), QVariant(textLayer->leading())},
          {QStringLiteral("text.fontStretch"), QVariant(textLayer->fontStretch())},
          {QStringLiteral("text.fontFamily"),
           QVariant(textLayer->fontFamily().toQString())},
          {QStringLiteral("text.alignment"),
           QVariant(static_cast<int>(textLayer->horizontalAlignment()))},
          {QStringLiteral("text.bold"), QVariant(textLayer->isBold())},
          {QStringLiteral("text.italic"), QVariant(textLayer->isItalic())},
          {QStringLiteral("text.allCaps"), QVariant(textLayer->isAllCaps())},
          {QStringLiteral("text.underline"), QVariant(textLayer->isUnderline())},
          {QStringLiteral("text.strikethrough"),
           QVariant(textLayer->isStrikethrough())},
          {QStringLiteral("text.strokeEnabled"),
           QVariant(textLayer->isStrokeEnabled())},
          {QStringLiteral("text.strokeWidth"), QVariant(textLayer->strokeWidth())},
          {QStringLiteral("text.shadowEnabled"),
           QVariant(textLayer->isShadowEnabled())},
          {QStringLiteral("text.shadowBlur"), QVariant(textLayer->shadowBlur())},
          {QStringLiteral("text.shadowOffsetX"),
           QVariant(textLayer->shadowOffsetX())},
          {QStringLiteral("text.shadowOffsetY"),
           QVariant(textLayer->shadowOffsetY())},
          {QStringLiteral("text.maxWidth"), QVariant(textLayer->maxWidth())},
          {QStringLiteral("text.boxHeight"), QVariant(textLayer->boxHeight())},
          {QStringLiteral("text.paragraphSpacing"),
           QVariant(textLayer->paragraphSpacing())},
          {QStringLiteral("text.layoutMode"),
           QVariant(static_cast<int>(textLayer->layoutMode()))},
          {QStringLiteral("text.wrapMode"),
           QVariant(static_cast<int>(textLayer->wrapMode()))},
          {QStringLiteral("text.verticalAlignment"),
           QVariant(static_cast<int>(textLayer->verticalAlignment()))},
          {QStringLiteral("text.writingMode"),
           QVariant(static_cast<int>(textLayer->writingMode()))}};
      auto styleMacro = std::make_unique<MacroUndoCommand>(
          QStringLiteral("Edit Text Style"));
      for (int index = 0; index < beforeStyleValues.size(); ++index) {
        styleMacro->addChild(std::make_unique<SetLayerPropertyValueCommand>(
            textLayer, beforeStyleValues[index].first,
            beforeStyleValues[index].second, afterStyleValues[index].second,
            QStringLiteral("Edit Text Style")));
      }
      if (auto* manager = UndoManager::instance()) {
        styleApplied = manager->push(std::move(styleMacro));
        if (!styleApplied) {
          for (const auto& [path, value] : beforeStyleValues) {
            textLayer->setLayerPropertyValue(path, value);
          }
        }
      }
    }

    const QString nextText = richText_ ? editor_->toHtml() : editor_->toPlainText();
    if (commitTextEditorValue(textLayer, nextText) || styleApplied ||
        animatorStackApplied) {
      if (auto *comp = static_cast<ArtifactAbstractComposition *>(textLayer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), textLayer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
      }
      if (controller_) {
        controller_->markRenderDirty();
      }
    }
  }

  ArtifactAbstractLayerPtr layer_;
  CompositionRenderController *controller_ = nullptr;
  QTextEdit *editor_ = nullptr;
  QDoubleSpinBox *fontSizeSpin_ = nullptr;
  QDoubleSpinBox *trackingSpin_ = nullptr;
  QDoubleSpinBox *leadingSpin_ = nullptr;
  QDoubleSpinBox *stretchSpin_ = nullptr;
  QComboBox *fontFamilyCombo_ = nullptr;
  QComboBox *alignmentCombo_ = nullptr;
  QCheckBox *boldCheck_ = nullptr;
  QCheckBox *italicCheck_ = nullptr;
  QCheckBox *allCapsCheck_ = nullptr;
  QCheckBox *underlineCheck_ = nullptr;
  QCheckBox *strikethroughCheck_ = nullptr;
  QCheckBox *strokeCheck_ = nullptr;
  QDoubleSpinBox *strokeWidthSpin_ = nullptr;
  QCheckBox *shadowCheck_ = nullptr;
  QDoubleSpinBox *shadowBlurSpin_ = nullptr;
  QDoubleSpinBox *shadowOffsetXSpin_ = nullptr;
  QDoubleSpinBox *shadowOffsetYSpin_ = nullptr;
  QDoubleSpinBox *boxWidthSpin_ = nullptr;
  QDoubleSpinBox *boxHeightSpin_ = nullptr;
  QDoubleSpinBox *paragraphSpacingSpin_ = nullptr;
  QComboBox *layoutModeCombo_ = nullptr;
  QComboBox *wrapModeCombo_ = nullptr;
  QComboBox *verticalAlignmentCombo_ = nullptr;
  QComboBox *writingModeCombo_ = nullptr;
  QSpinBox *animatorCountSpin_ = nullptr;
  QComboBox *animatorPresetCombo_ = nullptr;
  TextEditorState initialState_;
  bool richText_ = false;
  bool imePreeditActive_ = false;
  QWidget *preview_ = nullptr;
};

} // namespace

bool editTextLayerInline(QWidget *parent, const ArtifactAbstractLayerPtr &layer,
                         CompositionRenderController *controller) {
  const auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer);
  if (!textLayer || !parent) {
    return false;
  }

  QWidget *host = parent->window() ? parent->window() : parent;
  static QPointer<ArtifactTextEditorDialog> activeDialog;
  if (activeDialog) {
    activeDialog->raise();
    activeDialog->activateWindow();
    return true;
  }
  auto *dialog = new ArtifactTextEditorDialog(layer, controller, host);
  activeDialog = dialog;
  const QPoint center = host->rect().center();
  const QPoint globalCenter = host->mapToGlobal(center);
  dialog->move(globalCenter.x() - dialog->width() / 2,
               globalCenter.y() - dialog->height() / 2);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
  return true;
}
} // namespace Artifact
