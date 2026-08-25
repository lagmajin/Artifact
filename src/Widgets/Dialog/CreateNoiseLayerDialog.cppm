module;
#include <cstdint>
#include <utility>
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QFrame>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QFont>
#include <QPalette>
#include <wobjectimpl.h>

module Artifact.Widgets.CreateNoiseLayerDialog;

import ImageProcessing.ProceduralTexture;

namespace Artifact {

W_OBJECT_IMPL(CreateNoiseLayerDialog)

namespace {

class DialogCloseButton final : public QPushButton {
public:
  explicit DialogCloseButton(QWidget* parent = nullptr) : QPushButton(u8"×", parent) {
    setFixedSize(30, 30);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
  }
protected:
  bool event(QEvent* event) override {
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
      update();
    }
    return QPushButton::event(event);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor textCol = underMouse() ? QColor(0xff, 0x44, 0x44) : QColor(0xaa, 0xaa, 0xaa);
    painter.setPen(textCol);
    QFont font = this->font();
    font.setPointSize(18);
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, text());
  }
};

QString generatorKindLabel(ArtifactCore::ProceduralTextureGeneratorKind kind)
{
  switch (kind) {
    case ArtifactCore::ProceduralTextureGeneratorKind::Perlin:  return QStringLiteral("Perlin");
    case ArtifactCore::ProceduralTextureGeneratorKind::Simplex: return QStringLiteral("Simplex");
    case ArtifactCore::ProceduralTextureGeneratorKind::FBM:     return QStringLiteral("FBM");
    case ArtifactCore::ProceduralTextureGeneratorKind::Voronoi: return QStringLiteral("Voronoi");
    case ArtifactCore::ProceduralTextureGeneratorKind::White:   return QStringLiteral("White");
    case ArtifactCore::ProceduralTextureGeneratorKind::Value:   return QStringLiteral("Value");
    case ArtifactCore::ProceduralTextureGeneratorKind::Gradient:return QStringLiteral("Gradient");
  }
  return QStringLiteral("Perlin");
}

} // namespace

class CreateNoiseLayerDialog::Impl
{
public:
  QLineEdit* nameEdit = nullptr;
  QComboBox* kindCombo = nullptr;
  QSpinBox* seedSpin = nullptr;
  QSpinBox* widthSpin = nullptr;
  QSpinBox* heightSpin = nullptr;

  QPoint dragPos;
  bool dragging = false;

  static constexpr int kMinSize = 8;
  static constexpr int kMaxSize = 8192;
};

CreateNoiseLayerDialog::CreateNoiseLayerDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl)
{
  setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  setModal(true);

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header bar
  auto* header = new QWidget(this);
  header->setFixedHeight(50);
  header->setAutoFillBackground(true);
  {
    QPalette pal = header->palette();
    pal.setColor(QPalette::Window, QColor("#2a2a2a"));
    header->setPalette(pal);
  }
  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(15, 0, 10, 0);

  auto* titleLabel = new QLabel(u8"ノイズ設定", header);
  {
    QFont font = titleLabel->font();
    font.setBold(true);
    font.setPointSize(13);
    titleLabel->setFont(font);
    QPalette pal = titleLabel->palette();
    pal.setColor(QPalette::WindowText, QColor("#e0e0e0"));
    titleLabel->setPalette(pal);
  }

  auto* closeButton = new DialogCloseButton(header);

  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(closeButton);
  mainLayout->addWidget(header);

  // Form
  auto* content = new QWidget(this);
  auto* form = new QFormLayout(content);
  form->setContentsMargins(20, 15, 20, 15);
  form->setSpacing(10);

  impl_->nameEdit = new QLineEdit(QStringLiteral("Noise Layer 1"), content);
  form->addRow(u8"名前", impl_->nameEdit);

  impl_->kindCombo = new QComboBox(content);
  for (int i = 0; i <= static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::Gradient); ++i) {
    const auto kind = static_cast<ArtifactCore::ProceduralTextureGeneratorKind>(i);
    impl_->kindCombo->addItem(generatorKindLabel(kind), i);
  }
  form->addRow(u8"種別", impl_->kindCombo);

  impl_->seedSpin = new QSpinBox(content);
  impl_->seedSpin->setRange(0, 9999);
  impl_->seedSpin->setValue(42);
  form->addRow(u8"シード", impl_->seedSpin);

  auto* sizeRow = new QWidget(content);
  auto* sizeLayout = new QHBoxLayout(sizeRow);
  sizeLayout->setContentsMargins(0, 0, 0, 0);
  sizeLayout->setSpacing(6);
  impl_->widthSpin = new QSpinBox(sizeRow);
  impl_->widthSpin->setRange(Impl::kMinSize, Impl::kMaxSize);
  impl_->heightSpin = new QSpinBox(sizeRow);
  impl_->heightSpin->setRange(Impl::kMinSize, Impl::kMaxSize);
  sizeLayout->addWidget(impl_->widthSpin);
  sizeLayout->addWidget(new QLabel(u8"×", sizeRow));
  sizeLayout->addWidget(impl_->heightSpin);
  sizeLayout->addStretch();
  form->addRow(u8"サイズ", sizeRow);

  mainLayout->addWidget(content, 1);

  // Footer
  auto* footer = new QWidget(this);
  auto* footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(15, 10, 15, 12);

  auto* okBtn = new QPushButton("OK", footer);
  okBtn->setFixedSize(80, 28);
  auto* cancelBtn = new QPushButton(u8"キャンセル", footer);
  cancelBtn->setFixedSize(80, 28);
  footerLayout->addStretch();
  footerLayout->addWidget(okBtn);
  footerLayout->addWidget(cancelBtn);
  mainLayout->addWidget(footer);

  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

  adjustSize();
  setMinimumSize(size());
}

CreateNoiseLayerDialog::~CreateNoiseLayerDialog()
{
  delete impl_;
  impl_ = nullptr;
}

void CreateNoiseLayerDialog::setCompositionSize(int width, int height)
{
  if (impl_->widthSpin && width >= Impl::kMinSize && width <= Impl::kMaxSize) {
    impl_->widthSpin->setValue(width);
  }
  if (impl_->heightSpin && height >= Impl::kMinSize && height <= Impl::kMaxSize) {
    impl_->heightSpin->setValue(height);
  }
}

QString CreateNoiseLayerDialog::layerName() const
{
  return impl_->nameEdit ? impl_->nameEdit->text().trimmed() : QString();
}

ArtifactCore::ProceduralTextureGeneratorKind CreateNoiseLayerDialog::kind() const
{
  if (!impl_->kindCombo) {
    return ArtifactCore::ProceduralTextureGeneratorKind::Perlin;
  }
  const int value = impl_->kindCombo->currentData().toInt();
  if (value < 0 ||
      value > static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::Gradient)) {
    return ArtifactCore::ProceduralTextureGeneratorKind::Perlin;
  }
  return static_cast<ArtifactCore::ProceduralTextureGeneratorKind>(value);
}

std::uint32_t CreateNoiseLayerDialog::seed() const
{
  return impl_->seedSpin ? static_cast<std::uint32_t>(impl_->seedSpin->value()) : 42u;
}

int CreateNoiseLayerDialog::width() const
{
  return impl_->widthSpin ? impl_->widthSpin->value() : 1920;
}

int CreateNoiseLayerDialog::height() const
{
  return impl_->heightSpin ? impl_->heightSpin->value() : 1080;
}
// ── Events ───────────────────────────────────────────────────────────────────
void CreateNoiseLayerDialog::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape) {
    reject();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    accept();
    event->accept();
    return;
  }
  QDialog::keyPressEvent(event);
}

void CreateNoiseLayerDialog::mousePressEvent(QMouseEvent* e)
{
  if (e->button() == Qt::LeftButton) {
    impl_->dragPos  = e->globalPosition().toPoint() - frameGeometry().topLeft();
    impl_->dragging = true;
    e->accept();
    return;
  }
  QDialog::mousePressEvent(e);
}

void CreateNoiseLayerDialog::mouseReleaseEvent(QMouseEvent* e)
{
  if (impl_->dragging && e->button() == Qt::LeftButton) {
    impl_->dragging = false;
    e->accept();
    return;
  }
  QDialog::mouseReleaseEvent(e);
}

void CreateNoiseLayerDialog::mouseMoveEvent(QMouseEvent* e)
{
  if (impl_->dragging && (e->buttons() & Qt::LeftButton)) {
    move(e->globalPosition().toPoint() - impl_->dragPos);
    e->accept();
    return;
  }
  QDialog::mouseMoveEvent(e);
}

void CreateNoiseLayerDialog::showEvent(QShowEvent* e)
{
  QDialog::showEvent(e);
  QWidget* anchor = parentWidget() ? parentWidget()->window() : QApplication::activeWindow();
  QPoint pos;
  if (anchor) {
    pos = anchor->mapToGlobal(anchor->rect().center()) - QPoint(width() / 2, height() / 2);
  } else {
    pos = QGuiApplication::primaryScreen()->availableGeometry().center()
          - QPoint(width() / 2, height() / 2);
  }
  move(pos);
}



} // namespace Artifact
