module;
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSlider>
#include <QSpinBox>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <wobjectimpl.h>

module Widgets.ToolOptionsBar;

namespace Artifact {

W_OBJECT_IMPL(ArtifactToolOptionsBar)

// ツール種別定義
enum OptionRow : int {
  SelectTool,
  TransformTool,
  PenTool,
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

  // TextTool
  QComboBox *fontCombo = nullptr;
  QSpinBox *fontSizeSpin = nullptr;
  QToolButton *boldButton = nullptr;
  QToolButton *italicButton = nullptr;
  QToolButton *underlineButton = nullptr;

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
    fontCombo->addItems({"Sans Serif", "Serif", "Monospace"});
    ly->addWidget(fontCombo);

    fontSizeSpin = makeSpin(frame, 6, 512, "pt");
    ly->addWidget(fontSizeSpin);

    boldButton = makeToggle("B", frame);
    ly->addWidget(boldButton);

    italicButton = makeToggle("I", frame);
    ly->addWidget(italicButton);

    underlineButton = makeToggle("U", frame);
    ly->addWidget(underlineButton);

    ly->addStretch();
    optionFrames[TextTool] = frame;
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

  if (fontCombo)
    connect(fontCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("テキスト", "font", fontCombo->itemData(i));
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
  } else if (toolName == "ペン" || toolName == "シェイプ") {
    impl_->optionFrames[PenTool]->setVisible(true);
    impl_->currentRow = PenTool;
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

} // namespace Artifact