module;
#include <algorithm>
#include <utility>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <wobjectimpl.h>

module Widgets.ToolOptionsBar;

import Font.FreeFont;
import Artifact.Layer.Shape;
import Text.Style;

namespace Artifact {

W_OBJECT_IMPL(ArtifactToolOptionsBar)

// ツール種別定義
enum OptionRow : int {
  SelectTool,
  TransformTool,
  PenTool,
  ShapeTool,
  TextTool,
  BrushTool,
  CloneTool,
  EraserTool,
  OptionCount
};

class ArtifactToolOptionsBar::Impl {
public:
  Impl(ArtifactToolOptionsBar *parent) : toolOptionsBar(parent) {}
  ~Impl() = default;

  ArtifactToolOptionsBar *toolOptionsBar = nullptr;
  QWidget *optionFrames[OptionCount] = {};
  OptionRow currentRow = SelectTool;

  // SelectTool
  QComboBox *snapModeCombo = nullptr;
  QComboBox *selectionFilterCombo = nullptr;

  // TransformTool
  QComboBox *transformOriginCombo = nullptr;
  QCheckBox *numericInputCheck = nullptr;

  // PenTool
  QComboBox *curveTypeCombo = nullptr;
  QCheckBox *autoCloseCheck = nullptr;
  QCheckBox *showControlPointsCheck = nullptr;

  // ShapeTool
  QComboBox *shapeTypeCombo = nullptr;
  QSpinBox *shapeWidthSpin = nullptr;
  QSpinBox *shapeHeightSpin = nullptr;
  QCheckBox *shapeFillCheck = nullptr;
  QCheckBox *shapeStrokeCheck = nullptr;
  QSpinBox *shapeStrokeWidthSpin = nullptr;
  QLabel *shapePrimaryLabel = nullptr;
  QSpinBox *shapePrimarySpin = nullptr;
  QLabel *shapeSecondaryLabel = nullptr;
  QSpinBox *shapeSecondarySpin = nullptr;

  // TextTool
  QComboBox *fontCombo = nullptr;
  QSpinBox *fontSizeSpin = nullptr;
  QToolButton *boldButton = nullptr;
  QToolButton *italicButton = nullptr;
  QToolButton *underlineButton = nullptr;
  QComboBox *horizontalAlignCombo = nullptr;
  QComboBox *verticalAlignCombo = nullptr;
  QComboBox *wrapModeCombo = nullptr;
  QComboBox *layoutModeCombo = nullptr;

  // BrushTool
  QSpinBox *brushSizeSpin = nullptr;
  QSlider *brushSizeSlider = nullptr;
  QSpinBox *brushOpacitySpin = nullptr;
  QSpinBox *brushHardnessSpin = nullptr;

  // CloneTool
  QSpinBox *cloneRadiusSpin = nullptr;
  QCheckBox *alignedCheck = nullptr;

  // EraserTool
  QSpinBox *eraserSizeSpin = nullptr;
  QSpinBox *eraserOpacitySpin = nullptr;

  void createFrames(QHBoxLayout *parentLayout);
  void connectSignals();
};

// ヘルパー: ラベル
static QLabel *makeLabel(const QString &text, QWidget *parent) {
  auto *lbl = new QLabel(text, parent);
  lbl->setContentsMargins(6, 2, 6, 2);
  return lbl;
}

// ヘルパー: コンボボックス
static QComboBox *makeCombo(QWidget *parent) {
  auto *c = new QComboBox(parent);
  c->setFocusPolicy(Qt::StrongFocus);
  return c;
}

// ヘルパー: スピンボックス
static QSpinBox *makeSpin(QWidget *parent, int min, int max,
                          const QString &suffix = QString()) {
  auto *s = new QSpinBox(parent);
  s->setRange(min, max);
  if (!suffix.isEmpty())
    s->setSuffix(suffix);
  s->setAlignment(Qt::AlignCenter);
  return s;
}

// ヘルパー: トグルボタン
static QToolButton *makeToggle(const QString &text, QWidget *parent) {
  auto *b = new QToolButton(parent);
  b->setText(text);
  b->setCheckable(true);
  b->setToolButtonStyle(Qt::ToolButtonTextOnly);
  return b;
}

void ArtifactToolOptionsBar::Impl::createFrames(QHBoxLayout *parentLayout) {
  // ===== Select =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("選択", frame));

    snapModeCombo = makeCombo(frame);
    snapModeCombo->addItems({"スナップなし", "グリッドにスナップ",
                             "オブジェクトにスナップ", "ピクセルにスナップ"});
    ly->addWidget(snapModeCombo);

    selectionFilterCombo = makeCombo(frame);
    selectionFilterCombo->addItems(
        {"全てのレイヤー", "選択レイヤーのみ", "非ロックのみ"});
    ly->addWidget(selectionFilterCombo);

    ly->addStretch();
    optionFrames[SelectTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Transform =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("変形", frame));

    transformOriginCombo = makeCombo(frame);
    transformOriginCombo->addItems(
        {"中心", "左上", "右上", "左下", "右下", "カスタム"});
    ly->addWidget(transformOriginCombo);

    numericInputCheck = new QCheckBox("数値入力", frame);
    ly->addWidget(numericInputCheck);

    ly->addStretch();
    optionFrames[TransformTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Pen =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("ペン", frame));

    curveTypeCombo = makeCombo(frame);
    curveTypeCombo->addItems({"ベジェ", "直線", "スムーズ", "フリーハンド"});
    ly->addWidget(curveTypeCombo);

    autoCloseCheck = new QCheckBox("自動閉じ", frame);
    ly->addWidget(autoCloseCheck);

    showControlPointsCheck = new QCheckBox("制御点表示", frame);
    ly->addWidget(showControlPointsCheck);

    ly->addStretch();
    optionFrames[PenTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Text =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("テキスト", frame));

    fontCombo = makeCombo(frame);
    const QStringList families = ArtifactCore::FontManager::availableFamilies();
    if (!families.isEmpty()) {
      fontCombo->addItems(families);
    } else {
      fontCombo->addItems({"Sans Serif", "Serif", "Monospace"});
    }
    ly->addWidget(fontCombo);

    fontSizeSpin = makeSpin(frame, 1, 512, "pt");
    ly->addWidget(fontSizeSpin);

    boldButton = makeToggle("B", frame);
    ly->addWidget(boldButton);

    italicButton = makeToggle("I", frame);
    ly->addWidget(italicButton);

    underlineButton = makeToggle("U", frame);
    ly->addWidget(underlineButton);

    horizontalAlignCombo = makeCombo(frame);
    horizontalAlignCombo->addItem(QStringLiteral("左"),
                                  static_cast<int>(ArtifactCore::TextHorizontalAlignment::Left));
    horizontalAlignCombo->addItem(QStringLiteral("中"),
                                  static_cast<int>(ArtifactCore::TextHorizontalAlignment::Center));
    horizontalAlignCombo->addItem(QStringLiteral("右"),
                                  static_cast<int>(ArtifactCore::TextHorizontalAlignment::Right));
    horizontalAlignCombo->addItem(QStringLiteral("均等"),
                                  static_cast<int>(ArtifactCore::TextHorizontalAlignment::Justify));
    horizontalAlignCombo->setMinimumWidth(64);
    ly->addWidget(horizontalAlignCombo);

    verticalAlignCombo = makeCombo(frame);
    verticalAlignCombo->addItem(QStringLiteral("上"),
                                static_cast<int>(ArtifactCore::TextVerticalAlignment::Top));
    verticalAlignCombo->addItem(QStringLiteral("中段"),
                                static_cast<int>(ArtifactCore::TextVerticalAlignment::Middle));
    verticalAlignCombo->addItem(QStringLiteral("下"),
                                static_cast<int>(ArtifactCore::TextVerticalAlignment::Bottom));
    verticalAlignCombo->setMinimumWidth(64);
    ly->addWidget(verticalAlignCombo);

    wrapModeCombo = makeCombo(frame);
    wrapModeCombo->addItem(QStringLiteral("単語折返"),
                           static_cast<int>(ArtifactCore::TextWrapMode::WordWrap));
    wrapModeCombo->addItem(QStringLiteral("折返なし"),
                           static_cast<int>(ArtifactCore::TextWrapMode::NoWrap));
    wrapModeCombo->addItem(QStringLiteral("文字単位"),
                           static_cast<int>(ArtifactCore::TextWrapMode::WrapAnywhere));
    wrapModeCombo->addItem(QStringLiteral("手動改行"),
                           static_cast<int>(ArtifactCore::TextWrapMode::ManualWrap));
    wrapModeCombo->setMinimumWidth(96);
    ly->addWidget(wrapModeCombo);

    layoutModeCombo = makeCombo(frame);
    layoutModeCombo->addItem(QStringLiteral("点文字"), 0);
    layoutModeCombo->addItem(QStringLiteral("箱文字"), 1);
    layoutModeCombo->setMinimumWidth(88);
    ly->addWidget(layoutModeCombo);

    ly->addStretch();
    optionFrames[TextTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Shape =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("シェイプ", frame));

    shapeTypeCombo = makeCombo(frame);
    shapeTypeCombo->addItem(QStringLiteral("Rect"),
                            static_cast<int>(Artifact::ShapeType::Rect));
    shapeTypeCombo->addItem(QStringLiteral("Ellipse"),
                            static_cast<int>(Artifact::ShapeType::Ellipse));
    shapeTypeCombo->addItem(QStringLiteral("Star"),
                            static_cast<int>(Artifact::ShapeType::Star));
    shapeTypeCombo->addItem(QStringLiteral("Polygon"),
                            static_cast<int>(Artifact::ShapeType::Polygon));
    shapeTypeCombo->addItem(QStringLiteral("Line"),
                            static_cast<int>(Artifact::ShapeType::Line));
    shapeTypeCombo->addItem(QStringLiteral("Triangle"),
                            static_cast<int>(Artifact::ShapeType::Triangle));
    shapeTypeCombo->addItem(QStringLiteral("Square"),
                            static_cast<int>(Artifact::ShapeType::Square));
    ly->addWidget(shapeTypeCombo);

    shapeWidthSpin = makeSpin(frame, 1, 8192, "W");
    ly->addWidget(shapeWidthSpin);

    shapeHeightSpin = makeSpin(frame, 1, 8192, "H");
    ly->addWidget(shapeHeightSpin);

    shapePrimaryLabel = makeLabel(QStringLiteral("角丸"), frame);
    ly->addWidget(shapePrimaryLabel);
    shapePrimarySpin = makeSpin(frame, 0, 4096);
    ly->addWidget(shapePrimarySpin);

    shapeSecondaryLabel = makeLabel(QStringLiteral("副"), frame);
    ly->addWidget(shapeSecondaryLabel);
    shapeSecondarySpin = makeSpin(frame, 0, 100);
    shapeSecondarySpin->setSuffix("%");
    ly->addWidget(shapeSecondarySpin);

    shapeFillCheck = new QCheckBox(QStringLiteral("塗り"), frame);
    ly->addWidget(shapeFillCheck);

    shapeStrokeCheck = new QCheckBox(QStringLiteral("線"), frame);
    ly->addWidget(shapeStrokeCheck);

    shapeStrokeWidthSpin = makeSpin(frame, 0, 512, "px");
    ly->addWidget(shapeStrokeWidthSpin);

    ly->addStretch();
    optionFrames[ShapeTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Brush =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("ブラシ", frame));

    brushSizeSpin = makeSpin(frame, 1, 500, "px");
    ly->addWidget(brushSizeSpin);

    brushSizeSlider = new QSlider(Qt::Horizontal, frame);
    brushSizeSlider->setRange(1, 500);
    brushSizeSlider->setMinimumWidth(100);
    connect(brushSizeSlider, &QSlider::valueChanged, brushSizeSpin,
            &QSpinBox::setValue);
    connect(brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            brushSizeSlider, &QSlider::setValue);
    ly->addWidget(brushSizeSlider);

    brushHardnessSpin = makeSpin(frame, 0, 100, "%");
    ly->addWidget(brushHardnessSpin);

    brushOpacitySpin = makeSpin(frame, 1, 100, "%");
    ly->addWidget(brushOpacitySpin);

    ly->addStretch();
    optionFrames[BrushTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Clone =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("コピースタンプ", frame));

    cloneRadiusSpin = makeSpin(frame, 1, 300, "px");
    ly->addWidget(cloneRadiusSpin);

    alignedCheck = new QCheckBox("位置固定", frame);
    ly->addWidget(alignedCheck);

    ly->addStretch();
    optionFrames[CloneTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== Eraser =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("消しゴム", frame));

    eraserSizeSpin = makeSpin(frame, 1, 500, "px");
    ly->addWidget(eraserSizeSpin);

    eraserOpacitySpin = makeSpin(frame, 0, 100, "%");
    ly->addWidget(eraserOpacitySpin);

    ly->addStretch();
    optionFrames[EraserTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }
}

void ArtifactToolOptionsBar::Impl::connectSignals() {
  auto emitOpt = [this](const QString &tool, const QString &key,
                        const QVariant &v) {
    emit toolOptionsBar->optionChanged(tool, key, v);
  };

  if (snapModeCombo)
    connect(snapModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("選択", "snapMode", snapModeCombo->itemData(i));
            });

  if (transformOriginCombo)
    connect(transformOriginCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), toolOptionsBar,
            [this, emitOpt](int i) {
              emitOpt("変形", "transformOrigin",
                      transformOriginCombo->itemData(i));
            });

  if (curveTypeCombo)
    connect(curveTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("ペン", "curveType", curveTypeCombo->itemData(i));
            });

  if (autoCloseCheck)
    connect(autoCloseCheck, &QCheckBox::toggled, toolOptionsBar,
            [this, emitOpt](bool v) { emitOpt("ペン", "autoClose", v); });

  if (shapeTypeCombo)
    connect(shapeTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("シェイプ", "shapeType", shapeTypeCombo->itemData(i));
            });

  if (shapeWidthSpin)
    connect(shapeWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("シェイプ", "shapeWidth", v); });

  if (shapeHeightSpin)
    connect(shapeHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("シェイプ", "shapeHeight", v); });

  if (shapePrimarySpin)
    connect(shapePrimarySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("シェイプ", "shapePrimary", v); });

  if (shapeSecondarySpin)
    connect(shapeSecondarySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("シェイプ", "shapeSecondary", v); });

  if (shapeFillCheck)
    connect(shapeFillCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool v) { emitOpt("シェイプ", "fillEnabled", v); });

  if (shapeStrokeCheck)
    connect(shapeStrokeCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool v) { emitOpt("シェイプ", "strokeEnabled", v); });

  if (shapeStrokeWidthSpin)
    connect(shapeStrokeWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("シェイプ", "strokeWidth", v); });

  if (fontCombo)
    connect(fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              Q_UNUSED(i);
              emitOpt("テキスト", "font", fontCombo->currentText());
            });

  if (fontSizeSpin)
    connect(fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("テキスト", "fontSize", v); });

  if (boldButton)
    connect(boldButton, &QToolButton::toggled, toolOptionsBar,
            [emitOpt](bool v) { emitOpt("テキスト", "bold", v); });

  if (italicButton)
    connect(italicButton, &QToolButton::toggled, toolOptionsBar,
            [emitOpt](bool v) { emitOpt("テキスト", "italic", v); });

  if (underlineButton)
    connect(underlineButton, &QToolButton::toggled, toolOptionsBar,
            [emitOpt](bool v) { emitOpt("テキスト", "underline", v); });

  if (horizontalAlignCombo)
    connect(horizontalAlignCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), toolOptionsBar,
            [this, emitOpt](int i) {
              emitOpt("テキスト", "horizontalAlignment",
                      horizontalAlignCombo->itemData(i));
            });

  if (verticalAlignCombo)
    connect(verticalAlignCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), toolOptionsBar,
            [this, emitOpt](int i) {
              emitOpt("テキスト", "verticalAlignment",
                      verticalAlignCombo->itemData(i));
            });

  if (wrapModeCombo)
    connect(wrapModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("テキスト", "wrapMode", wrapModeCombo->itemData(i));
            });

  if (layoutModeCombo)
    connect(layoutModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("テキスト", "layoutMode", layoutModeCombo->itemData(i));
            });

  if (brushSizeSpin)
    connect(brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushSize", v); });

  if (brushOpacitySpin)
    connect(brushOpacitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushOpacity", v); });

  if (brushHardnessSpin)
    connect(brushHardnessSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushHardness", v); });

  if (cloneRadiusSpin)
    connect(cloneRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("コピースタンプ", "radius", v); });

  if (eraserSizeSpin)
    connect(eraserSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "size", v); });

  if (eraserOpacitySpin)
    connect(eraserOpacitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "opacity", v); });
}

ArtifactToolOptionsBar::ArtifactToolOptionsBar(QWidget *parent)
    : QWidget(parent), impl_(new Impl(this)) {
  setMinimumHeight(32);
  setMaximumHeight(40);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  impl_->createFrames(layout);
  impl_->connectSignals();

  // 初期表示: 選択ツール
  setCurrentTool("選択");
}

ArtifactToolOptionsBar::~ArtifactToolOptionsBar() { delete impl_; }

void ArtifactToolOptionsBar::setCurrentTool(const QString &toolName) {
  if (!impl_)
    return;

  for (auto *f : impl_->optionFrames)
    if (f)
      f->setVisible(false);

  impl_->currentRow = SelectTool;

  if (toolName == "選択") {
    impl_->optionFrames[SelectTool]->setVisible(true);
  } else if (toolName == "移動" || toolName == "回転" ||
             toolName == "スケール" || toolName == "アンカー") {
    impl_->optionFrames[TransformTool]->setVisible(true);
    impl_->currentRow = TransformTool;
  } else if (toolName == "ペン") {
    impl_->optionFrames[PenTool]->setVisible(true);
    impl_->currentRow = PenTool;
  } else if (toolName == "シェイプ") {
    impl_->optionFrames[ShapeTool]->setVisible(true);
    impl_->currentRow = ShapeTool;
  } else if (toolName == "テキスト") {
    impl_->optionFrames[TextTool]->setVisible(true);
    impl_->currentRow = TextTool;
  } else if (toolName == "ブラシ") {
    impl_->optionFrames[BrushTool]->setVisible(true);
    impl_->currentRow = BrushTool;
  } else if (toolName == "コピースタンプ") {
    impl_->optionFrames[CloneTool]->setVisible(true);
    impl_->currentRow = CloneTool;
  } else if (toolName == "消しゴム") {
    impl_->optionFrames[EraserTool]->setVisible(true);
    impl_->currentRow = EraserTool;
  }
}

void ArtifactToolOptionsBar::setTextOptions(const QString &fontFamily,
                                            int fontSize, bool bold,
                                            bool italic, bool underline,
                                            int horizontalAlignment,
                                            int verticalAlignment,
                                            int wrapMode,
                                            int layoutMode,
                                            bool enabled) {
  if (!impl_) {
    return;
  }

  if (impl_->fontCombo) {
    QSignalBlocker blocker(*impl_->fontCombo);
    if (!fontFamily.trimmed().isEmpty()) {
      int index = impl_->fontCombo->findText(fontFamily, Qt::MatchFixedString);
      if (index < 0) {
        impl_->fontCombo->addItem(fontFamily);
        index = impl_->fontCombo->findText(fontFamily, Qt::MatchFixedString);
      }
      if (index >= 0) {
        impl_->fontCombo->setCurrentIndex(index);
      }
    }
    impl_->fontCombo->setEnabled(enabled);
  }

  if (impl_->fontSizeSpin) {
    QSignalBlocker blocker(*impl_->fontSizeSpin);
    impl_->fontSizeSpin->setValue(std::clamp(fontSize, 1, 512));
    impl_->fontSizeSpin->setEnabled(enabled);
  }

  if (impl_->boldButton) {
    QSignalBlocker blocker(*impl_->boldButton);
    impl_->boldButton->setChecked(bold);
    impl_->boldButton->setEnabled(enabled);
  }

  if (impl_->italicButton) {
    QSignalBlocker blocker(*impl_->italicButton);
    impl_->italicButton->setChecked(italic);
    impl_->italicButton->setEnabled(enabled);
  }

  if (impl_->underlineButton) {
    QSignalBlocker blocker(*impl_->underlineButton);
    impl_->underlineButton->setChecked(underline);
    impl_->underlineButton->setEnabled(enabled);
  }

  if (impl_->horizontalAlignCombo) {
    QSignalBlocker blocker(*impl_->horizontalAlignCombo);
    const int index = impl_->horizontalAlignCombo->findData(horizontalAlignment);
    if (index >= 0) {
      impl_->horizontalAlignCombo->setCurrentIndex(index);
    }
    impl_->horizontalAlignCombo->setEnabled(enabled);
  }

  if (impl_->verticalAlignCombo) {
    QSignalBlocker blocker(*impl_->verticalAlignCombo);
    const int index = impl_->verticalAlignCombo->findData(verticalAlignment);
    if (index >= 0) {
      impl_->verticalAlignCombo->setCurrentIndex(index);
    }
    impl_->verticalAlignCombo->setEnabled(enabled);
  }

  if (impl_->wrapModeCombo) {
    QSignalBlocker blocker(*impl_->wrapModeCombo);
    const int index = impl_->wrapModeCombo->findData(wrapMode);
    if (index >= 0) {
      impl_->wrapModeCombo->setCurrentIndex(index);
    }
    impl_->wrapModeCombo->setEnabled(enabled);
  }

  if (impl_->layoutModeCombo) {
    QSignalBlocker blocker(*impl_->layoutModeCombo);
    const int index = impl_->layoutModeCombo->findData(layoutMode);
    if (index >= 0) {
      impl_->layoutModeCombo->setCurrentIndex(index);
    }
    impl_->layoutModeCombo->setEnabled(enabled);
  }
}

void ArtifactToolOptionsBar::clearTextOptions() {
  setTextOptions(QString(), 12, false, false, false,
                 static_cast<int>(ArtifactCore::TextHorizontalAlignment::Left),
                 static_cast<int>(ArtifactCore::TextVerticalAlignment::Top),
                 static_cast<int>(ArtifactCore::TextWrapMode::WordWrap), 0,
                 false);
}

void ArtifactToolOptionsBar::setShapeOptions(int shapeType, int width, int height,
                                             bool fillEnabled,
                                             bool strokeEnabled,
                                             int strokeWidth,
                                             int cornerRadius,
                                             int starPoints,
                                             int starInnerRadiusPercent,
                                             int polygonSides, bool enabled) {
  if (!impl_) {
    return;
  }

  if (impl_->shapeTypeCombo) {
    QSignalBlocker blocker(*impl_->shapeTypeCombo);
    const int index = impl_->shapeTypeCombo->findData(shapeType);
    if (index >= 0) {
      impl_->shapeTypeCombo->setCurrentIndex(index);
    }
    impl_->shapeTypeCombo->setEnabled(enabled);
  }

  if (impl_->shapeWidthSpin) {
    QSignalBlocker blocker(*impl_->shapeWidthSpin);
    impl_->shapeWidthSpin->setValue(std::clamp(width, 1, 8192));
    impl_->shapeWidthSpin->setEnabled(enabled);
  }

  if (impl_->shapeHeightSpin) {
    QSignalBlocker blocker(*impl_->shapeHeightSpin);
    impl_->shapeHeightSpin->setValue(std::clamp(height, 1, 8192));
    impl_->shapeHeightSpin->setEnabled(enabled);
  }

  if (impl_->shapeFillCheck) {
    QSignalBlocker blocker(*impl_->shapeFillCheck);
    impl_->shapeFillCheck->setChecked(fillEnabled);
    impl_->shapeFillCheck->setEnabled(enabled);
  }

  if (impl_->shapeStrokeCheck) {
    QSignalBlocker blocker(*impl_->shapeStrokeCheck);
    impl_->shapeStrokeCheck->setChecked(strokeEnabled);
    impl_->shapeStrokeCheck->setEnabled(enabled);
  }

  if (impl_->shapeStrokeWidthSpin) {
    QSignalBlocker blocker(*impl_->shapeStrokeWidthSpin);
    impl_->shapeStrokeWidthSpin->setValue(std::clamp(strokeWidth, 0, 512));
    impl_->shapeStrokeWidthSpin->setEnabled(enabled);
  }

  QString primaryLabel = QStringLiteral("値");
  int primaryValue = 0;
  bool primaryEnabled = false;
  QString secondaryLabel = QStringLiteral("副");
  int secondaryValue = 0;
  bool secondaryEnabled = false;
  bool secondaryPercent = false;

  switch (static_cast<Artifact::ShapeType>(shapeType)) {
  case Artifact::ShapeType::Rect:
  case Artifact::ShapeType::Square:
    primaryLabel = QStringLiteral("角丸");
    primaryValue = cornerRadius;
    primaryEnabled = true;
    break;
  case Artifact::ShapeType::Star:
    primaryLabel = QStringLiteral("点数");
    primaryValue = starPoints;
    primaryEnabled = true;
    secondaryLabel = QStringLiteral("内径");
    secondaryValue = starInnerRadiusPercent;
    secondaryEnabled = true;
    secondaryPercent = true;
    break;
  case Artifact::ShapeType::Polygon:
    primaryLabel = QStringLiteral("辺数");
    primaryValue = polygonSides;
    primaryEnabled = true;
    break;
  default:
    primaryLabel = QStringLiteral("値");
    break;
  }

  if (impl_->shapePrimaryLabel) {
    impl_->shapePrimaryLabel->setText(primaryLabel);
    impl_->shapePrimaryLabel->setEnabled(enabled && primaryEnabled);
  }
  if (impl_->shapePrimarySpin) {
    QSignalBlocker blocker(*impl_->shapePrimarySpin);
    impl_->shapePrimarySpin->setSuffix(QString());
    impl_->shapePrimarySpin->setRange(
        primaryLabel == QStringLiteral("点数") || primaryLabel == QStringLiteral("辺数")
            ? 3
            : 0,
        primaryLabel == QStringLiteral("点数") || primaryLabel == QStringLiteral("辺数")
            ? 64
            : 4096);
    impl_->shapePrimarySpin->setValue(std::max(0, primaryValue));
    impl_->shapePrimarySpin->setEnabled(enabled && primaryEnabled);
  }

  if (impl_->shapeSecondaryLabel) {
    impl_->shapeSecondaryLabel->setText(secondaryLabel);
    impl_->shapeSecondaryLabel->setEnabled(enabled && secondaryEnabled);
  }
  if (impl_->shapeSecondarySpin) {
    QSignalBlocker blocker(*impl_->shapeSecondarySpin);
    impl_->shapeSecondarySpin->setSuffix(secondaryPercent ? "%" : QString());
    impl_->shapeSecondarySpin->setRange(secondaryPercent ? 0 : 0,
                                        secondaryPercent ? 100 : 4096);
    impl_->shapeSecondarySpin->setValue(std::max(0, secondaryValue));
    impl_->shapeSecondarySpin->setEnabled(enabled && secondaryEnabled);
  }
}

void ArtifactToolOptionsBar::clearShapeOptions() {
  setShapeOptions(static_cast<int>(Artifact::ShapeType::Rect), 200, 200, true,
                  false, 0, 0, 5, 38, 6, false);
}

} // namespace Artifact
