module;

#include <algorithm>

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QProgressBar>
#include <QScreen>
#include <QVBoxLayout>

module Artifact.Widgets.SplashScreen;

namespace Artifact {

class ArtifactSplashScreen::Impl
{
public:
  QLabel* wordmark = nullptr;
  QLabel* status = nullptr;
  QLabel* percent = nullptr;
  QProgressBar* progressBar = nullptr;
  int progress = 0;
};

ArtifactSplashScreen::ArtifactSplashScreen(QWidget* parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint)
    , impl_(new Impl())
{
  setAttribute(Qt::WA_DeleteOnClose, false);
  setFixedSize(520, 300);
  setWindowTitle(QStringLiteral("Artifact"));

  QPalette splashPalette = palette();
  splashPalette.setColor(QPalette::Window, QColor(QStringLiteral("#17191D")));
  splashPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F5F7")));
  splashPalette.setColor(QPalette::Base, QColor(QStringLiteral("#272B31")));
  splashPalette.setColor(QPalette::Highlight, QColor(QStringLiteral("#D6A84A")));
  splashPalette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#17191D")));
  setPalette(splashPalette);
  setAutoFillBackground(true);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(46, 40, 46, 34);
  layout->setSpacing(0);

  // Isolated wordmark surface: it can later be replaced with an owner-drawn
  // cutout without changing the splash screen's progress/status contract.
  impl_->wordmark = new QLabel(QStringLiteral("ARTIFACT"), this);
  QFont wordmarkFont = font();
  wordmarkFont.setPointSizeF(33.0);
  wordmarkFont.setWeight(QFont::DemiBold);
  wordmarkFont.setLetterSpacing(QFont::AbsoluteSpacing, 6.0);
  impl_->wordmark->setFont(wordmarkFont);
  impl_->wordmark->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  QPalette wordmarkPalette = impl_->wordmark->palette();
  wordmarkPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F8F4E8")));
  impl_->wordmark->setPalette(wordmarkPalette);
  layout->addWidget(impl_->wordmark, 1);

  auto* loadingLabel = new QLabel(QStringLiteral("STARTING STUDIO"), this);
  QFont loadingFont = font();
  loadingFont.setPointSizeF(8.5);
  loadingFont.setWeight(QFont::DemiBold);
  loadingFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
  loadingLabel->setFont(loadingFont);
  loadingLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  QPalette mutedPalette = loadingLabel->palette();
  mutedPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#AAB0B8")));
  loadingLabel->setPalette(mutedPalette);
  layout->addWidget(loadingLabel);
  layout->addSpacing(10);

  impl_->progressBar = new QProgressBar(this);
  impl_->progressBar->setRange(0, 100);
  impl_->progressBar->setValue(0);
  impl_->progressBar->setTextVisible(false);
  impl_->progressBar->setFixedHeight(6);
  layout->addWidget(impl_->progressBar);
  layout->addSpacing(12);

  auto* footer = new QHBoxLayout();
  footer->setContentsMargins(0, 0, 0, 0);
  footer->setSpacing(12);
  impl_->status = new QLabel(QStringLiteral("Preparing workspace"), this);
  impl_->status->setFont(loadingFont);
  impl_->status->setPalette(mutedPalette);
  footer->addWidget(impl_->status, 1);
  impl_->percent = new QLabel(QStringLiteral("0%"), this);
  impl_->percent->setFont(loadingFont);
  impl_->percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  impl_->percent->setPalette(mutedPalette);
  footer->addWidget(impl_->percent);
  layout->addLayout(footer);

  if (QScreen* screen = QGuiApplication::primaryScreen()) {
    move(screen->availableGeometry().center() - rect().center());
  }
}

ArtifactSplashScreen::~ArtifactSplashScreen()
{
  delete impl_;
  impl_ = nullptr;
}

void ArtifactSplashScreen::setProgress(int percent)
{
  const int clamped = std::clamp(percent, 0, 100);
  if (impl_->progress == clamped) {
    return;
  }
  impl_->progress = clamped;
  impl_->progressBar->setValue(clamped);
  impl_->percent->setText(QStringLiteral("%1%").arg(clamped));
}

int ArtifactSplashScreen::progress() const
{
  return impl_->progress;
}

void ArtifactSplashScreen::setStatusText(const QString& text)
{
  impl_->status->setText(text);
}

} // namespace Artifact
