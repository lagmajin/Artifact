module;
#include <cstdint>
#include <algorithm>
#include <cstdlib>
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
#include <QGridLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <QPalette>
#include <QRectF>
#include <QSizePolicy>
#include <QObject>
#include <wobjectimpl.h>

module Artifact.Widgets.CreateNoiseLayerDialog;

import ImageProcessing.ProceduralTexture;

namespace Artifact {

W_OBJECT_IMPL(CreateNoiseLayerDialog)

namespace {

const QColor kDialogBackground(30, 32, 36);
const QColor kPanelBackground(35, 38, 43);
const QColor kPreviewBackground(20, 22, 25);
const QColor kBorderColor(61, 66, 74);
const QColor kPrimaryText(224, 226, 231);
const QColor kMutedText(145, 151, 161);
const QColor kAccentColor(225, 151, 63);

void setWindowColor(QWidget* widget, const QColor& color)
{
  widget->setAutoFillBackground(true);
  QPalette palette = widget->palette();
  palette.setColor(QPalette::Window, color);
  widget->setPalette(palette);
}

void setTextColor(QLabel* label, const QColor& color)
{
  QPalette palette = label->palette();
  palette.setColor(QPalette::WindowText, color);
  label->setPalette(palette);
}

QLabel* makeSectionLabel(const QString& text, QWidget* parent)
{
  auto* label = new QLabel(text, parent);
  QFont font = label->font();
  font.setBold(true);
  font.setPointSize(9);
  label->setFont(font);
  setTextColor(label, kAccentColor);
  return label;
}

class NoisePreviewWidget final : public QWidget {
public:
  explicit NoisePreviewWidget(QWidget* parent = nullptr) : QWidget(parent)
  {
    setMinimumSize(260, 210);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(QStringLiteral("Noise preview"));
    setAccessibleDescription(QStringLiteral("Preview updated only when noise settings change"));
  }

  void setSources(QComboBox* kind, QSpinBox* seed)
  {
    kind_ = kind;
    seed_ = seed;
    update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    const int kind = kind_ ? kind_->currentData().toInt() : 0;
    const std::uint32_t seed = seed_ ? static_cast<std::uint32_t>(seed_->value()) : 42u;
    if (!valid_ || kind != cachedKind_ || seed != cachedSeed_) {
      rebuild(kind, seed);
    }

    QPainter painter(this);
    painter.fillRect(rect(), kPreviewBackground);
    const QRect target = rect().adjusted(1, 1, -1, -1);
    const qreal cellWidth = static_cast<qreal>(target.width()) / kSide;
    const qreal cellHeight = static_cast<qreal>(target.height()) / kSide;
    painter.setPen(Qt::NoPen);
    for (int y = 0; y < kSide; ++y) {
      for (int x = 0; x < kSide; ++x) {
        const int value = pixels_[static_cast<std::size_t>(y * kSide + x)];
        painter.setBrush(QColor(value, value, value));
        painter.drawRect(QRectF(target.left() + x * cellWidth,
                                target.top() + y * cellHeight,
                                cellWidth + 0.5, cellHeight + 0.5));
      }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(kBorderColor);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }

private:
  static constexpr int kSide = 48;
  std::uint8_t pixels_[kSide * kSide] {};
  QComboBox* kind_ = nullptr;
  QSpinBox* seed_ = nullptr;
  int cachedKind_ = -1;
  std::uint32_t cachedSeed_ = 0;
  bool valid_ = false;

  static std::uint32_t hash(std::uint32_t value)
  {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
  }

  void rebuild(int kind, std::uint32_t seed)
  {
    cachedKind_ = kind;
    cachedSeed_ = seed;
    valid_ = true;
    for (int y = 0; y < kSide; ++y) {
      for (int x = 0; x < kSide; ++x) {
        const int coarseX = x >> 2;
        const int coarseY = y >> 2;
        const auto sample = [seed, kind](int sx, int sy) {
          const std::uint32_t key = seed
              ^ (static_cast<std::uint32_t>(sx) * 0x9e3779b9u)
              ^ (static_cast<std::uint32_t>(sy) * 0x85ebca6bu)
              ^ (static_cast<std::uint32_t>(kind + 1) * 0xc2b2ae35u);
          return static_cast<int>((hash(key) >> 24) & 0xffu);
        };
        int value = sample(coarseX, coarseY);
        if (kind == static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::White)) {
          value = sample(x, y);
        } else if (kind == static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::Gradient)) {
          value = (x * 255) / (kSide - 1);
        } else if (kind == static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::Voronoi)) {
          value = 255 - std::min(255, std::abs((x % 12) - 6) * 28 + std::abs((y % 12) - 6) * 28);
        } else {
          const int nextX = sample(coarseX + 1, coarseY);
          const int nextY = sample(coarseX, coarseY + 1);
          value = (value * 2 + nextX + nextY) / 4;
        }
        pixels_[static_cast<std::size_t>(y * kSide + x)] = static_cast<std::uint8_t>(value);
      }
    }
  }
};

class NoisePreviewRefreshFilter final : public QObject {
public:
  explicit NoisePreviewRefreshFilter(NoisePreviewWidget* preview, QObject* parent)
      : QObject(parent), preview_(preview) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    const auto type = event->type();
    if (type == QEvent::KeyRelease || type == QEvent::MouseButtonRelease ||
        type == QEvent::Wheel || type == QEvent::FocusOut) {
      preview_->update();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  NoisePreviewWidget* preview_ = nullptr;
};

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
  NoisePreviewWidget* preview = nullptr;

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
  setWindowTitle(QStringLiteral("Create Noise Layer"));
  setMinimumSize(720, 430);
  setWindowColor(this, kDialogBackground);

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header bar
  auto* header = new QWidget(this);
  header->setFixedHeight(50);
  header->setAutoFillBackground(true);
  {
    QPalette pal = header->palette();
    pal.setColor(QPalette::Window, kPanelBackground);
    header->setPalette(pal);
  }
  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(15, 0, 10, 0);

  auto* titleLabel = new QLabel(QStringLiteral("Create Noise Layer"), header);
  {
    QFont font = titleLabel->font();
    font.setBold(true);
    font.setPointSize(13);
    titleLabel->setFont(font);
    QPalette pal = titleLabel->palette();
    pal.setColor(QPalette::WindowText, kPrimaryText);
    titleLabel->setPalette(pal);
  }

  auto* closeButton = new DialogCloseButton(header);

  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(closeButton);
  mainLayout->addWidget(header);

  // Settings and preview
  auto* content = new QWidget(this);
  setWindowColor(content, kDialogBackground);
  auto* contentLayout = new QHBoxLayout(content);
  contentLayout->setContentsMargins(24, 20, 24, 18);
  contentLayout->setSpacing(24);

  auto* settingsPanel = new QWidget(content);
  settingsPanel->setMinimumWidth(300);
  auto* settingsLayout = new QVBoxLayout(settingsPanel);
  settingsLayout->setContentsMargins(0, 0, 0, 0);
  settingsLayout->setSpacing(10);
  settingsLayout->addWidget(makeSectionLabel(QStringLiteral("NOISE SETTINGS"), settingsPanel));
  auto* form = new QFormLayout();
  form->setContentsMargins(0, 4, 0, 0);
  form->setHorizontalSpacing(14);
  form->setVerticalSpacing(12);
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  impl_->nameEdit = new QLineEdit(QStringLiteral("Noise Layer 1"), content);
  form->addRow(QStringLiteral("Name"), impl_->nameEdit);

  impl_->kindCombo = new QComboBox(content);
  for (int i = 0; i <= static_cast<int>(ArtifactCore::ProceduralTextureGeneratorKind::Gradient); ++i) {
    const auto kind = static_cast<ArtifactCore::ProceduralTextureGeneratorKind>(i);
    impl_->kindCombo->addItem(generatorKindLabel(kind), i);
  }
  form->addRow(QStringLiteral("Type"), impl_->kindCombo);

  impl_->seedSpin = new QSpinBox(content);
  impl_->seedSpin->setRange(0, 9999);
  impl_->seedSpin->setValue(42);
  form->addRow(QStringLiteral("Seed"), impl_->seedSpin);

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
  form->addRow(QStringLiteral("Size"), sizeRow);
  settingsLayout->addLayout(form);
  settingsLayout->addStretch();

  auto* previewPanel = new QWidget(content);
  auto* previewLayout = new QVBoxLayout(previewPanel);
  previewLayout->setContentsMargins(0, 0, 0, 0);
  previewLayout->setSpacing(8);
  previewLayout->addWidget(makeSectionLabel(QStringLiteral("PREVIEW"), previewPanel));
  impl_->preview = new NoisePreviewWidget(previewPanel);
  impl_->preview->setSources(impl_->kindCombo, impl_->seedSpin);
  auto* previewRefreshFilter = new NoisePreviewRefreshFilter(impl_->preview, this);
  impl_->kindCombo->installEventFilter(previewRefreshFilter);
  impl_->seedSpin->installEventFilter(previewRefreshFilter);
  previewLayout->addWidget(impl_->preview, 1);
  auto* previewHint = new QLabel(QStringLiteral("Preview updates when Type or Seed changes."), previewPanel);
  setTextColor(previewHint, kMutedText);
  previewLayout->addWidget(previewHint);

  contentLayout->addWidget(settingsPanel);
  contentLayout->addWidget(previewPanel, 1);

  mainLayout->addWidget(content, 1);

  // Footer
  auto* footer = new QWidget(this);
  auto* footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(15, 10, 15, 12);

  auto* okBtn = new QPushButton("OK", footer);
  okBtn->setFixedSize(80, 28);
  auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), footer);
  cancelBtn->setFixedSize(80, 28);
  footerLayout->addStretch();
  footerLayout->addWidget(okBtn);
  footerLayout->addWidget(cancelBtn);
  mainLayout->addWidget(footer);

  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

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
  if (impl_->preview) {
    impl_->preview->update();
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
