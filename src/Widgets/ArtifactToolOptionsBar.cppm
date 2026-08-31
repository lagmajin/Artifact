module;
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QPalette>
#include <QSlider>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QToolButton>
#include <QWidget>
#include <wobjectimpl.h>

module Widgets.ToolOptionsBar;

import Font.FreeFont;
import Artifact.Application.Manager;
import Artifact.Tool.MotionSketchTool;
import Artifact.Layer.Shape;
import Text.Style;
import Settings.Accessibility;
import FloatColorPickerDialog;
import Color.Float;
import Artifact.Event.Types;
import Event.Bus;

namespace Artifact {

W_OBJECT_IMPL(ArtifactToolOptionsBar)

void ArtifactToolOptionsBar::optionChanged(const QString &toolName,
                                           const QString &optionName,
                                           const QVariant &value) {
  const auto publish = [toolName, optionName, value]() {
    ArtifactCore::globalEventBus().publish<ToolOptionChangedEvent>(
        ToolOptionChangedEvent{toolName, optionName, value});
  };
  if (QThread::currentThread() == thread()) {
    publish();
    return;
  }
  QMetaObject::invokeMethod(this, publish, Qt::QueuedConnection);
}

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
  MotionSketchTool,
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
  QComboBox *strokeCapCombo = nullptr;
  QComboBox *strokeJoinCombo = nullptr;
  QComboBox *strokeAlignCombo = nullptr;
  QLineEdit *dashEdit = nullptr;
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
  QSpinBox *brushFlowSpin = nullptr;
  QSpinBox *brushHardnessSpin = nullptr;
  QSpinBox *brushSpacingSpin = nullptr;
  QSpinBox *brushAngleSpin = nullptr;
  QSpinBox *brushRoundnessSpin = nullptr;
  QSpinBox *brushSizeJitterSpin = nullptr;
  QSpinBox *brushOpacityJitterSpin = nullptr;
  QSpinBox *brushScatterSpin = nullptr;
  QSpinBox *brushAngleJitterSpin = nullptr;
  QSpinBox *brushRoundnessJitterSpin = nullptr;
  QSpinBox *brushFlowJitterSpin = nullptr;
  QCheckBox *brushPressureFlowCheck = nullptr;
  QCheckBox *brushPressureSizeCheck = nullptr;
  QCheckBox *brushPressureOpacityCheck = nullptr;
  QCheckBox *brushTiltAngleCheck = nullptr;
  QCheckBox *brushTiltRoundnessCheck = nullptr;
  QToolButton *brushColorButton = nullptr;
  ArtifactCore::FloatColor brushColor{0.0f, 0.0f, 0.0f, 1.0f};

  // CloneTool
  QSpinBox *cloneRadiusSpin = nullptr;
  QCheckBox *alignedCheck = nullptr;
  QSpinBox *cloneTimeOffsetSpin = nullptr;

  // EraserTool
  QSpinBox *eraserSizeSpin = nullptr;
  QSpinBox *eraserOpacitySpin = nullptr;
  QSpinBox *eraserHardnessSpin = nullptr;
  QSpinBox *eraserAngleSpin = nullptr;
  QSpinBox *eraserRoundnessSpin = nullptr;
  QSpinBox *eraserStrengthSpin = nullptr;
  QCheckBox *eraserLastStrokeCheck = nullptr;
  QComboBox *eraserModeCombo = nullptr;
  QSpinBox *motionSmoothingSpin = nullptr;
  QSpinBox *motionSampleRateSpin = nullptr;
  QCheckBox *motionWireframeCheck = nullptr;

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

    strokeCapCombo = makeCombo(frame);
    strokeCapCombo->addItem(QStringLiteral("Flat"),
                            static_cast<int>(Artifact::StrokeCap::Flat));
    strokeCapCombo->addItem(QStringLiteral("Round"),
                            static_cast<int>(Artifact::StrokeCap::Round));
    strokeCapCombo->addItem(QStringLiteral("Square"),
                            static_cast<int>(Artifact::StrokeCap::Square));
    strokeCapCombo->setToolTip(QStringLiteral("線端"));
    ly->addWidget(strokeCapCombo);

    strokeJoinCombo = makeCombo(frame);
    strokeJoinCombo->addItem(QStringLiteral("Miter"),
                             static_cast<int>(Artifact::StrokeJoin::Miter));
    strokeJoinCombo->addItem(QStringLiteral("Round"),
                             static_cast<int>(Artifact::StrokeJoin::Round));
    strokeJoinCombo->addItem(QStringLiteral("Bevel"),
                             static_cast<int>(Artifact::StrokeJoin::Bevel));
    strokeJoinCombo->setToolTip(QStringLiteral("結合"));
    ly->addWidget(strokeJoinCombo);

    strokeAlignCombo = makeCombo(frame);
    strokeAlignCombo->addItem(QStringLiteral("中央"),
                              static_cast<int>(Artifact::StrokeAlign::Center));
    strokeAlignCombo->addItem(QStringLiteral("内側"),
                              static_cast<int>(Artifact::StrokeAlign::Inside));
    strokeAlignCombo->addItem(QStringLiteral("外側"),
                              static_cast<int>(Artifact::StrokeAlign::Outside));
    strokeAlignCombo->setToolTip(QStringLiteral("線の位置"));
    ly->addWidget(strokeAlignCombo);

    dashEdit = new QLineEdit(frame);
    dashEdit->setPlaceholderText(QStringLiteral("破線"));
    dashEdit->setMaximumWidth(60);
    ly->addWidget(dashEdit);

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

    brushSizeSpin = makeSpin(frame, 1, 2500, "px");
    brushSizeSpin->setAccessibleName(QStringLiteral("Brush diameter"));
    brushSizeSpin->setToolTip(QStringLiteral("Brush diameter in pixels"));
    ly->addWidget(brushSizeSpin);

    brushSizeSlider = new QSlider(Qt::Horizontal, frame);
    brushSizeSlider->setRange(1, 2500);
    brushSizeSlider->setMinimumWidth(100);
    connect(brushSizeSlider, &QSlider::valueChanged, brushSizeSpin,
            &QSpinBox::setValue);
    connect(brushSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            brushSizeSlider, &QSlider::setValue);
    ly->addWidget(brushSizeSlider);

    brushHardnessSpin = makeSpin(frame, 0, 100, "%");
    brushHardnessSpin->setAccessibleName(QStringLiteral("Brush hardness"));
    brushHardnessSpin->setToolTip(QStringLiteral("Brush edge hardness"));
    ly->addWidget(brushHardnessSpin);

    brushOpacitySpin = makeSpin(frame, 1, 100, "%");
    brushOpacitySpin->setAccessibleName(QStringLiteral("Brush opacity"));
    brushOpacitySpin->setToolTip(QStringLiteral("Brush opacity"));
    ly->addWidget(brushOpacitySpin);

    brushFlowSpin = makeSpin(frame, 1, 100, "%");
    brushFlowSpin->setValue(100);
    brushFlowSpin->setAccessibleName(QStringLiteral("Brush flow"));
    brushFlowSpin->setToolTip(QStringLiteral("Paint flow per dab"));
    ly->addWidget(brushFlowSpin);

    brushSpacingSpin = makeSpin(frame, 1, 1000, "%");
    brushSpacingSpin->setValue(25);
    brushSpacingSpin->setAccessibleName(QStringLiteral("Brush spacing"));
    brushSpacingSpin->setToolTip(QStringLiteral("Distance between brush dabs"));
    ly->addWidget(brushSpacingSpin);

    brushAngleSpin = makeSpin(frame, 0, 360, "°");
    brushAngleSpin->setAccessibleName(QStringLiteral("Brush angle"));
    brushAngleSpin->setToolTip(QStringLiteral("Brush tip angle"));
    ly->addWidget(brushAngleSpin);

    brushRoundnessSpin = makeSpin(frame, 1, 100, "%");
    brushRoundnessSpin->setValue(100);
    brushRoundnessSpin->setAccessibleName(QStringLiteral("Brush roundness"));
    brushRoundnessSpin->setToolTip(QStringLiteral("Brush tip roundness"));
    ly->addWidget(brushRoundnessSpin);

    brushSizeJitterSpin = makeSpin(frame, 0, 100, "%");
    brushSizeJitterSpin->setAccessibleName(QStringLiteral("Size jitter"));
    brushSizeJitterSpin->setToolTip(QStringLiteral("Random size variation"));
    ly->addWidget(brushSizeJitterSpin);

    brushOpacityJitterSpin = makeSpin(frame, 0, 100, "%");
    brushOpacityJitterSpin->setAccessibleName(QStringLiteral("Opacity jitter"));
    brushOpacityJitterSpin->setToolTip(QStringLiteral("Random opacity variation"));
    ly->addWidget(brushOpacityJitterSpin);

    brushScatterSpin = makeSpin(frame, 0, 100, "%");
    brushScatterSpin->setAccessibleName(QStringLiteral("Scatter"));
    brushScatterSpin->setToolTip(QStringLiteral("Scatter brush dabs"));
    ly->addWidget(brushScatterSpin);

    brushAngleJitterSpin = makeSpin(frame, 0, 100, "%");
    brushAngleJitterSpin->setAccessibleName(QStringLiteral("Angle jitter"));
    brushAngleJitterSpin->setToolTip(QStringLiteral("Random angle variation"));
    ly->addWidget(brushAngleJitterSpin);

    brushRoundnessJitterSpin = makeSpin(frame, 0, 100, "%");
    brushRoundnessJitterSpin->setAccessibleName(QStringLiteral("Roundness jitter"));
    brushRoundnessJitterSpin->setToolTip(QStringLiteral("Random roundness variation"));
    ly->addWidget(brushRoundnessJitterSpin);

    brushFlowJitterSpin = makeSpin(frame, 0, 100, "%");
    brushFlowJitterSpin->setAccessibleName(QStringLiteral("Flow jitter"));
    brushFlowJitterSpin->setToolTip(QStringLiteral("Random flow variation"));
    ly->addWidget(brushFlowJitterSpin);

    brushPressureFlowCheck =
        new QCheckBox(QStringLiteral("Pressure Flow"), frame);
    brushPressureFlowCheck->setChecked(true);
    brushPressureFlowCheck->setAccessibleName(
        QStringLiteral("Pressure affects flow"));
    brushPressureFlowCheck->setToolTip(
        QStringLiteral("Use tablet pressure to modulate brush flow"));
    ly->addWidget(brushPressureFlowCheck);

    brushPressureSizeCheck =
        new QCheckBox(QStringLiteral("Pressure Size"), frame);
    brushPressureSizeCheck->setChecked(true);
    brushPressureSizeCheck->setAccessibleName(
        QStringLiteral("Pressure affects size"));
    brushPressureSizeCheck->setToolTip(
        QStringLiteral("Use tablet pressure to modulate brush diameter"));
    ly->addWidget(brushPressureSizeCheck);

    brushPressureOpacityCheck =
        new QCheckBox(QStringLiteral("Pressure Opacity"), frame);
    brushPressureOpacityCheck->setChecked(true);
    brushPressureOpacityCheck->setAccessibleName(
        QStringLiteral("Pressure affects opacity"));
    brushPressureOpacityCheck->setToolTip(
        QStringLiteral("Use tablet pressure to modulate brush opacity"));
    ly->addWidget(brushPressureOpacityCheck);

    brushTiltAngleCheck = new QCheckBox(QStringLiteral("Tilt Angle"), frame);
    brushTiltAngleCheck->setChecked(true);
    brushTiltAngleCheck->setAccessibleName(QStringLiteral("Tilt affects angle"));
    brushTiltAngleCheck->setToolTip(
        QStringLiteral("Use pen tilt to rotate the brush tip"));
    ly->addWidget(brushTiltAngleCheck);

    brushTiltRoundnessCheck =
        new QCheckBox(QStringLiteral("Tilt Roundness"), frame);
    brushTiltRoundnessCheck->setChecked(true);
    brushTiltRoundnessCheck->setAccessibleName(
        QStringLiteral("Tilt affects roundness"));
    brushTiltRoundnessCheck->setToolTip(
        QStringLiteral("Use pen tilt to change brush roundness"));
    ly->addWidget(brushTiltRoundnessCheck);

    brushColorButton = new QToolButton(frame);
    brushColorButton->setText(QStringLiteral("Color"));
    brushColorButton->setAccessibleName(QStringLiteral("Brush fill color"));
    brushColorButton->setAccessibleDescription(
        QStringLiteral("Choose the color used by the brush"));
    brushColorButton->setToolTip(QStringLiteral("Brush fill color"));
    {
      QPalette palette = brushColorButton->palette();
      palette.setColor(QPalette::Button, QColor(0, 0, 0));
      palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
      brushColorButton->setPalette(palette);
    }
    ly->addWidget(brushColorButton);

    ly->addStretch();
    optionFrames[BrushTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  // ===== MotionSketch =====
  {
    auto *frame = new QWidget(toolOptionsBar);
    auto *ly = new QHBoxLayout(frame);
    ly->setContentsMargins(4, 2, 4, 2);
    ly->setSpacing(8);
    ly->addWidget(makeLabel("モーションスケッチ", frame));
    motionSmoothingSpin = makeSpin(frame, 0, 100, "%");
    motionSmoothingSpin->setValue(50);
    ly->addWidget(motionSmoothingSpin);
    motionSampleRateSpin = makeSpin(frame, 1, 60, "fps");
    motionSampleRateSpin->setValue(60);
    ly->addWidget(motionSampleRateSpin);
    motionWireframeCheck = new QCheckBox(QStringLiteral("Wireframe"), frame);
    ly->addWidget(motionWireframeCheck);
    ly->addStretch();
    optionFrames[MotionSketchTool] = frame;
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
    cloneRadiusSpin->setAccessibleName(QStringLiteral("Clone radius"));
    cloneRadiusSpin->setAccessibleDescription(
        QStringLiteral("Sets the copy stamp radius in pixels"));
    cloneRadiusSpin->setMinimumHeight(Artifact::Accessibility::scaledSize(24));
    ly->addWidget(cloneRadiusSpin);

    alignedCheck = new QCheckBox("位置固定", frame);
    alignedCheck->setAccessibleName(QStringLiteral("Aligned clone sampling"));
    alignedCheck->setAccessibleDescription(
        QStringLiteral("Keeps the sampled source position fixed while painting"));
    alignedCheck->setMinimumHeight(Artifact::Accessibility::scaledSize(24));
    ly->addWidget(alignedCheck);

    cloneTimeOffsetSpin = makeSpin(frame, -10000, 10000, "frames");
    cloneTimeOffsetSpin->setValue(0);
    cloneTimeOffsetSpin->setAccessibleName(QStringLiteral("Clone time offset"));
    cloneTimeOffsetSpin->setAccessibleDescription(
        QStringLiteral("Offsets the sampled source frame relative to the current frame"));
    cloneTimeOffsetSpin->setMinimumHeight(Artifact::Accessibility::scaledSize(24));
    ly->addWidget(cloneTimeOffsetSpin);

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
    eraserSizeSpin->setAccessibleName(QStringLiteral("Eraser diameter"));
    eraserSizeSpin->setToolTip(QStringLiteral("Eraser diameter in pixels"));
    ly->addWidget(eraserSizeSpin);

    eraserOpacitySpin = makeSpin(frame, 0, 100, "%");
    eraserOpacitySpin->setAccessibleName(QStringLiteral("Eraser opacity"));
    eraserOpacitySpin->setToolTip(QStringLiteral("Eraser opacity"));
    ly->addWidget(eraserOpacitySpin);

    eraserHardnessSpin = makeSpin(frame, 0, 100, "%");
    eraserHardnessSpin->setValue(100);
    eraserHardnessSpin->setAccessibleName(QStringLiteral("Eraser hardness"));
    eraserHardnessSpin->setToolTip(QStringLiteral("Eraser hardness"));
    ly->addWidget(eraserHardnessSpin);

    eraserAngleSpin = makeSpin(frame, 0, 360, "°");
    eraserAngleSpin->setAccessibleName(QStringLiteral("Eraser angle"));
    eraserAngleSpin->setToolTip(QStringLiteral("Shared brush tip angle"));
    ly->addWidget(eraserAngleSpin);

    eraserRoundnessSpin = makeSpin(frame, 1, 100, "%");
    eraserRoundnessSpin->setValue(100);
    eraserRoundnessSpin->setAccessibleName(QStringLiteral("Eraser roundness"));
    eraserRoundnessSpin->setToolTip(QStringLiteral("Shared brush tip roundness"));
    ly->addWidget(eraserRoundnessSpin);

    eraserStrengthSpin = makeSpin(frame, 0, 100, "%");
    eraserStrengthSpin->setValue(100);
    eraserStrengthSpin->setAccessibleName(QStringLiteral("Eraser strength"));
    eraserStrengthSpin->setToolTip(QStringLiteral("Eraser strength"));
    ly->addWidget(eraserStrengthSpin);
    eraserLastStrokeCheck = new QCheckBox(QStringLiteral("Last Stroke Only"), frame);
    eraserLastStrokeCheck->setAccessibleName(
        QStringLiteral("Erase last stroke only"));
    eraserLastStrokeCheck->setAccessibleDescription(
        QStringLiteral("Remove only the most recent paint stroke"));
    eraserLastStrokeCheck->setToolTip(
        QStringLiteral("Undo only the most recent paint stroke"));
    ly->addWidget(eraserLastStrokeCheck);
    eraserModeCombo = new QComboBox(frame);
    eraserModeCombo->addItem(QStringLiteral("Paint Eraser"), 0);
    eraserModeCombo->addItem(QStringLiteral("Layer Eraser"), 1);
    eraserModeCombo->addItem(QStringLiteral("Last Stroke Only"), 2);
    eraserModeCombo->setAccessibleName(QStringLiteral("Eraser mode"));
    eraserModeCombo->setAccessibleDescription(
        QStringLiteral("Choose paint, layer, or last-stroke erasing"));
    ly->addWidget(eraserModeCombo);

    ly->addStretch();
    optionFrames[EraserTool] = frame;
    parentLayout->addWidget(frame);
    frame->setVisible(false);
  }

  const std::array<std::pair<const char *, const char *>, OptionCount> frameA11y = {{
      {"Select tool options", "Configure selection and snapping options"},
      {"Transform tool options", "Configure transform origin and numeric input"},
      {"Pen tool options", "Configure pen curve and control point options"},
      {"Shape tool options", "Configure shape geometry and stroke options"},
      {"Text tool options", "Configure font, alignment, and text layout options"},
      {"Brush tool options", "Configure brush size, opacity, and hardness"},
      {"Clone tool options", "Configure copy stamp radius and alignment"},
      {"Eraser tool options", "Configure eraser size and opacity"},
  }};
  for (int i = 0; i < OptionCount; ++i) {
    if (optionFrames[i]) {
      optionFrames[i]->setAccessibleName(QString::fromLatin1(frameA11y[i].first));
      optionFrames[i]->setAccessibleDescription(
          QString::fromLatin1(frameA11y[i].second));
      optionFrames[i]->setMinimumHeight(Artifact::Accessibility::scaledSize(32));
    }
  }
}

void ArtifactToolOptionsBar::Impl::connectSignals() {
  auto emitOpt = [this](const QString &tool, const QString &key,
                        const QVariant &v) {
    toolOptionsBar->optionChanged(tool, key, v);
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

  if (strokeCapCombo)
    connect(strokeCapCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("シェイプ", "strokeCap",
                      strokeCapCombo->itemData(i).toInt());
            });

  if (strokeJoinCombo)
    connect(strokeJoinCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("シェイプ", "strokeJoin",
                      strokeJoinCombo->itemData(i).toInt());
            });

  if (strokeAlignCombo)
    connect(strokeAlignCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            toolOptionsBar, [this, emitOpt](int i) {
              emitOpt("シェイプ", "strokeAlign",
                      strokeAlignCombo->itemData(i).toInt());
            });

  if (dashEdit)
    connect(dashEdit, &QLineEdit::textChanged, toolOptionsBar,
            [emitOpt](const QString &text) {
              emitOpt("シェイプ", "dashPattern", text);
            });

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

  if (brushFlowSpin)
    connect(brushFlowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushFlow", v); });

  if (brushHardnessSpin)
    connect(brushHardnessSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushHardness", v); });

  if (brushSpacingSpin)
    connect(brushSpacingSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushSpacing", v); });

  if (brushAngleSpin)
    connect(brushAngleSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushAngle", v); });

  if (brushRoundnessSpin)
    connect(brushRoundnessSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "brushRoundness", v); });

  if (brushSizeJitterSpin)
    connect(brushSizeJitterSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "sizeJitter", v); });

  if (brushOpacityJitterSpin)
    connect(brushOpacityJitterSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "opacityJitter", v); });

  if (brushScatterSpin)
    connect(brushScatterSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "scatter", v); });

  if (brushAngleJitterSpin)
    connect(brushAngleJitterSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "angleJitter", v); });

  if (brushRoundnessJitterSpin)
    connect(brushRoundnessJitterSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "roundnessJitter", v); });

  if (brushFlowJitterSpin)
    connect(brushFlowJitterSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("ブラシ", "flowJitter", v); });

  if (brushPressureFlowCheck)
    connect(brushPressureFlowCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("ブラシ", "pressureAffectsFlow", enabled);
            });

  if (brushPressureSizeCheck)
    connect(brushPressureSizeCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("ブラシ", "pressureAffectsSize", enabled);
            });

  if (brushPressureOpacityCheck)
    connect(brushPressureOpacityCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("ブラシ", "pressureAffectsOpacity", enabled);
            });

  if (brushTiltAngleCheck)
    connect(brushTiltAngleCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("ブラシ", "tiltAffectsAngle", enabled);
            });

  if (brushTiltRoundnessCheck)
    connect(brushTiltRoundnessCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("ブラシ", "tiltAffectsRoundness", enabled);
            });

  if (brushColorButton)
    connect(brushColorButton, &QToolButton::clicked, toolOptionsBar,
            [this, emitOpt]() {
              ArtifactWidgets::FloatColorPicker picker(toolOptionsBar);
              picker.setInitialColor(brushColor);
              picker.setColor(brushColor);
              if (picker.exec() != QDialog::Accepted) {
                return;
              }
              const auto color = picker.getColor();
              brushColor = color;
              QPalette palette = brushColorButton->palette();
              palette.setColor(
                  QPalette::Button,
                  QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));
              const float luminance = 0.2126f * color.r() +
                                      0.7152f * color.g() +
                                      0.0722f * color.b();
              palette.setColor(QPalette::ButtonText,
                               luminance > 0.52f ? QColor(0, 0, 0)
                                                : QColor(255, 255, 255));
              brushColorButton->setPalette(palette);
              emitOpt("ブラシ", "brushColor",
                      QStringLiteral("%1,%2,%3,%4")
                          .arg(color.r(), 0, 'f', 6)
                          .arg(color.g(), 0, 'f', 6)
                          .arg(color.b(), 0, 'f', 6)
                          .arg(color.a(), 0, 'f', 6));
            });

  if (cloneRadiusSpin)
    connect(cloneRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("コピースタンプ", "radius", v); });

  if (alignedCheck)
    connect(alignedCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("コピースタンプ", "aligned", enabled);
            });

  if (cloneTimeOffsetSpin)
    connect(cloneTimeOffsetSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("コピースタンプ", "timeOffset", v); });

  if (eraserSizeSpin)
    connect(eraserSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "size", v); });

  if (eraserOpacitySpin)
    connect(eraserOpacitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "opacity", v); });

  if (eraserHardnessSpin)
    connect(eraserHardnessSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "hardness", v); });

  if (eraserAngleSpin)
    connect(eraserAngleSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "angle", v); });

  if (eraserRoundnessSpin)
    connect(eraserRoundnessSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "roundness", v); });

  if (eraserStrengthSpin)
    connect(eraserStrengthSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) { emitOpt("消しゴム", "strength", v); });

  if (eraserLastStrokeCheck)
    connect(eraserLastStrokeCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("消しゴム", "lastStrokeOnly", enabled);
            });

  if (eraserModeCombo)
    connect(eraserModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), toolOptionsBar,
            [this, emitOpt](int index) {
              emitOpt("消しゴム", "mode", eraserModeCombo->itemData(index));
            });

  if (motionSmoothingSpin)
    connect(motionSmoothingSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) {
              emitOpt("モーションスケッチ", "smoothing", v);
            });

  if (motionSampleRateSpin)
    connect(motionSampleRateSpin,
            QOverload<int>::of(&QSpinBox::valueChanged), toolOptionsBar,
            [emitOpt](int v) {
              emitOpt("モーションスケッチ", "sampleRate", v);
            });

  if (motionWireframeCheck)
    connect(motionWireframeCheck, &QCheckBox::toggled, toolOptionsBar,
            [emitOpt](bool enabled) {
              emitOpt("モーションスケッチ", "showWireframe", enabled);
            });
}

ArtifactToolOptionsBar::ArtifactToolOptionsBar(QWidget *parent)
    : QWidget(parent), impl_(new Impl(this)) {
  setAccessibleName(QStringLiteral("Tool Options"));
  setAccessibleDescription(QStringLiteral("Adjust options for the active editing tool"));
  setMinimumHeight(Artifact::Accessibility::scaledSize(32));
  setMaximumHeight(Artifact::Accessibility::scaledSize(40));

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
  } else if (toolName == "シェイプ" || toolName == "楕円") {
    impl_->optionFrames[ShapeTool]->setVisible(true);
    impl_->currentRow = ShapeTool;
  } else if (toolName == "テキスト") {
    impl_->optionFrames[TextTool]->setVisible(true);
    impl_->currentRow = TextTool;
  } else if (toolName == "モーションスケッチ") {
    impl_->optionFrames[MotionSketchTool]->setVisible(true);
    impl_->currentRow = MotionSketchTool;
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

void ArtifactToolOptionsBar::syncBrushOptionsFromTool() {
  if (!impl_) {
    return;
  }
  auto *app = ArtifactApplicationManager::instance();
  auto *brush = app ? app->brushTool() : nullptr;
  if (!brush) {
    return;
  }
  const QSignalBlocker sizeBlocker(impl_->brushSizeSpin);
  const QSignalBlocker opacityBlocker(impl_->brushOpacitySpin);
  const QSignalBlocker flowBlocker(impl_->brushFlowSpin);
  const QSignalBlocker hardnessBlocker(impl_->brushHardnessSpin);
  const QSignalBlocker spacingBlocker(impl_->brushSpacingSpin);
  const QSignalBlocker angleBlocker(impl_->brushAngleSpin);
  const QSignalBlocker roundnessBlocker(impl_->brushRoundnessSpin);
  const QSignalBlocker sizeJitterBlocker(impl_->brushSizeJitterSpin);
  const QSignalBlocker opacityJitterBlocker(impl_->brushOpacityJitterSpin);
  const QSignalBlocker scatterBlocker(impl_->brushScatterSpin);
  const QSignalBlocker angleJitterBlocker(impl_->brushAngleJitterSpin);
  const QSignalBlocker roundnessJitterBlocker(impl_->brushRoundnessJitterSpin);
  const QSignalBlocker flowJitterBlocker(impl_->brushFlowJitterSpin);
  const QSignalBlocker pressureFlowBlocker(impl_->brushPressureFlowCheck);
  const QSignalBlocker pressureSizeBlocker(impl_->brushPressureSizeCheck);
  const QSignalBlocker pressureOpacityBlocker(impl_->brushPressureOpacityCheck);
  const QSignalBlocker tiltAngleBlocker(impl_->brushTiltAngleCheck);
  const QSignalBlocker tiltRoundnessBlocker(impl_->brushTiltRoundnessCheck);
  const QSignalBlocker eraserSizeBlocker(impl_->eraserSizeSpin);
  const QSignalBlocker eraserOpacityBlocker(impl_->eraserOpacitySpin);
  const QSignalBlocker eraserHardnessBlocker(impl_->eraserHardnessSpin);
  const QSignalBlocker eraserAngleBlocker(impl_->eraserAngleSpin);
  const QSignalBlocker eraserRoundnessBlocker(impl_->eraserRoundnessSpin);
  const QSignalBlocker eraserStrengthBlocker(impl_->eraserStrengthSpin);
  const QSignalBlocker eraserLastBlocker(impl_->eraserLastStrokeCheck);
  const QSignalBlocker eraserModeBlocker(impl_->eraserModeCombo);
  const QSignalBlocker cloneRadiusBlocker(impl_->cloneRadiusSpin);
  const QSignalBlocker cloneAlignedBlocker(impl_->alignedCheck);
  const QSignalBlocker cloneOffsetBlocker(impl_->cloneTimeOffsetSpin);

  impl_->brushSizeSpin->setValue(static_cast<int>(std::lround(brush->radius())));
  impl_->brushOpacitySpin->setValue(static_cast<int>(std::lround(brush->opacity() * 100.0f)));
  impl_->brushFlowSpin->setValue(static_cast<int>(std::lround(brush->flow() * 100.0f)));
  impl_->brushHardnessSpin->setValue(static_cast<int>(std::lround(brush->hardness() * 100.0f)));
  impl_->brushSpacingSpin->setValue(static_cast<int>(std::lround(brush->spacing() * 100.0f)));
  impl_->brushAngleSpin->setValue(static_cast<int>(std::lround(brush->angle())));
  impl_->brushRoundnessSpin->setValue(static_cast<int>(std::lround(brush->roundness() * 100.0f)));
  impl_->brushSizeJitterSpin->setValue(static_cast<int>(std::lround(brush->sizeJitter() * 100.0f)));
  impl_->brushOpacityJitterSpin->setValue(static_cast<int>(std::lround(brush->opacityJitter() * 100.0f)));
  impl_->brushScatterSpin->setValue(static_cast<int>(std::lround(brush->scatter() * 100.0f)));
  impl_->brushAngleJitterSpin->setValue(static_cast<int>(std::lround(brush->angleJitter() * 100.0f)));
  impl_->brushRoundnessJitterSpin->setValue(static_cast<int>(std::lround(brush->roundnessJitter() * 100.0f)));
  impl_->brushFlowJitterSpin->setValue(static_cast<int>(std::lround(brush->flowJitter() * 100.0f)));
  impl_->brushPressureFlowCheck->setChecked(brush->pressureAffectsFlow());
  impl_->brushPressureSizeCheck->setChecked(brush->pressureAffectsSize());
  impl_->brushPressureOpacityCheck->setChecked(brush->pressureAffectsOpacity());
  impl_->brushTiltAngleCheck->setChecked(brush->tiltAffectsAngle());
  impl_->brushTiltRoundnessCheck->setChecked(brush->tiltAffectsRoundness());
  impl_->eraserSizeSpin->setValue(static_cast<int>(std::lround(brush->radius())));
  impl_->eraserOpacitySpin->setValue(static_cast<int>(std::lround(brush->opacity() * 100.0f)));
  impl_->eraserHardnessSpin->setValue(static_cast<int>(std::lround(brush->hardness() * 100.0f)));
  impl_->eraserAngleSpin->setValue(static_cast<int>(std::lround(brush->angle())));
  impl_->eraserRoundnessSpin->setValue(static_cast<int>(std::lround(brush->roundness() * 100.0f)));
  impl_->eraserStrengthSpin->setValue(static_cast<int>(std::lround(brush->opacity() * 100.0f)));
  impl_->eraserLastStrokeCheck->setChecked(brush->lastStrokeOnly());
  if (impl_->eraserModeCombo) {
    const int modeIndex = impl_->eraserModeCombo->findData(brush->eraserModeKind());
    if (modeIndex >= 0) {
      impl_->eraserModeCombo->setCurrentIndex(modeIndex);
    }
  }
  impl_->cloneRadiusSpin->setValue(static_cast<int>(std::lround(brush->radius())));
  impl_->alignedCheck->setChecked(brush->cloneAligned());
  impl_->cloneTimeOffsetSpin->setValue(brush->cloneTimeOffset());
  impl_->brushColor = brush->color();
  if (impl_->brushColorButton) {
    QPalette palette = impl_->brushColorButton->palette();
    palette.setColor(
        QPalette::Button,
        QColor::fromRgbF(impl_->brushColor.r(), impl_->brushColor.g(),
                         impl_->brushColor.b(), impl_->brushColor.a()));
    const float luminance = 0.2126f * impl_->brushColor.r() +
                            0.7152f * impl_->brushColor.g() +
                            0.0722f * impl_->brushColor.b();
    palette.setColor(QPalette::ButtonText,
                     luminance > 0.52f ? QColor(0, 0, 0)
                                      : QColor(255, 255, 255));
    impl_->brushColorButton->setPalette(palette);
  }
}

void ArtifactToolOptionsBar::syncMotionSketchOptionsFromTool() {
  if (!impl_) {
    return;
  }
  auto *app = ArtifactApplicationManager::instance();
  auto *motion = app ? app->motionSketchTool() : nullptr;
  if (!motion) {
    return;
  }
  if (impl_->motionSmoothingSpin) {
    const QSignalBlocker blocker(impl_->motionSmoothingSpin);
    impl_->motionSmoothingSpin->setValue(
        std::clamp(static_cast<int>(std::lround(motion->smoothing() * 100.0f)), 0, 100));
  }
  if (impl_->motionSampleRateSpin) {
    const QSignalBlocker blocker(impl_->motionSampleRateSpin);
    impl_->motionSampleRateSpin->setValue(
        std::clamp(static_cast<int>(std::lround(motion->sampleRate())), 1, 60));
  }
  if (impl_->motionWireframeCheck) {
    const QSignalBlocker blocker(impl_->motionWireframeCheck);
    impl_->motionWireframeCheck->setChecked(motion->showWireframe());
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

void ArtifactToolOptionsBar::setShapeOptions(
    int shapeType, int width, int height, bool fillEnabled, bool strokeEnabled,
    int strokeWidth, int strokeCap, int strokeJoin, int strokeAlign,
    const QString &dashPattern, int cornerRadius, int starPoints,
    int starInnerRadiusPercent, int polygonSides, bool enabled) {
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

  const bool isLine =
      static_cast<Artifact::ShapeType>(shapeType) == Artifact::ShapeType::Line;

  if (impl_->shapeWidthSpin) {
    QSignalBlocker blocker(*impl_->shapeWidthSpin);
    impl_->shapeWidthSpin->setValue(std::clamp(width, 1, 8192));
    impl_->shapeWidthSpin->setSuffix(isLine ? QStringLiteral("L")
                                            : QStringLiteral("W"));
    impl_->shapeWidthSpin->setEnabled(enabled);
  }

  if (impl_->shapeHeightSpin) {
    QSignalBlocker blocker(*impl_->shapeHeightSpin);
    impl_->shapeHeightSpin->setValue(std::clamp(height, 1, 8192));
    // A primitive line has no independent height; its thickness is Stroke Width.
    impl_->shapeHeightSpin->setEnabled(enabled && !isLine);
  }

  if (impl_->shapeFillCheck) {
    QSignalBlocker blocker(*impl_->shapeFillCheck);
    impl_->shapeFillCheck->setChecked(fillEnabled);
    // Lines are stroke-only primitives.
    impl_->shapeFillCheck->setEnabled(enabled && !isLine);
  }

  if (impl_->shapeStrokeCheck) {
    QSignalBlocker blocker(*impl_->shapeStrokeCheck);
    impl_->shapeStrokeCheck->setChecked(strokeEnabled);
    // Keep Line stroke enabled; users edit its width/style below.
    impl_->shapeStrokeCheck->setEnabled(enabled && !isLine);
  }

  if (impl_->shapeStrokeWidthSpin) {
    QSignalBlocker blocker(*impl_->shapeStrokeWidthSpin);
    impl_->shapeStrokeWidthSpin->setValue(std::clamp(strokeWidth, 0, 512));
    impl_->shapeStrokeWidthSpin->setEnabled(enabled);
  }

  if (impl_->strokeCapCombo) {
    QSignalBlocker blocker(*impl_->strokeCapCombo);
    const int idx = impl_->strokeCapCombo->findData(strokeCap);
    if (idx >= 0) impl_->strokeCapCombo->setCurrentIndex(idx);
    impl_->strokeCapCombo->setEnabled(enabled);
  }

  if (impl_->strokeJoinCombo) {
    QSignalBlocker blocker(*impl_->strokeJoinCombo);
    const int idx = impl_->strokeJoinCombo->findData(strokeJoin);
    if (idx >= 0) impl_->strokeJoinCombo->setCurrentIndex(idx);
    // Open lines have no join; the renderer's join setting is for corners.
    impl_->strokeJoinCombo->setEnabled(enabled && !isLine);
  }

  if (impl_->strokeAlignCombo) {
    QSignalBlocker blocker(*impl_->strokeAlignCombo);
    const int idx = impl_->strokeAlignCombo->findData(strokeAlign);
    if (idx >= 0) impl_->strokeAlignCombo->setCurrentIndex(idx);
    // GPU PolylineStyle currently renders centered strokes only.
    impl_->strokeAlignCombo->setEnabled(enabled && !isLine);
  }

  if (impl_->dashEdit) {
    QSignalBlocker blocker(*impl_->dashEdit);
    impl_->dashEdit->setText(dashPattern);
    impl_->dashEdit->setEnabled(enabled);
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
                  false, 0,
                  static_cast<int>(Artifact::StrokeCap::Flat),
                  static_cast<int>(Artifact::StrokeJoin::Miter),
                  static_cast<int>(Artifact::StrokeAlign::Center),
                  QString(), 0, 5, 38, 6, false);
}

} // namespace Artifact
