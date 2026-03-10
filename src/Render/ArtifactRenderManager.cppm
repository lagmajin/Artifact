module;
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QStandardPaths>
#include <QVector>

module Artifact.Render.Manager;

namespace Artifact {

 class ArtifactRenderManager::Impl {
 public:
  QVector<DummyRenderRequest> queue_;

  static QString defaultOutputDirectory()
  {
   QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
   if (base.isEmpty()) {
    base = QDir::homePath();
   }
   QDir root(base);
   root.mkpath(QStringLiteral("ArtifactRenders"));
   return root.filePath(QStringLiteral("ArtifactRenders"));
  }

  static QString normalizeName(QString name)
  {
   name = name.trimmed();
   if (name.isEmpty()) {
    name = QStringLiteral("Composition");
   }
   name.replace(QChar('/'), QChar('_'));
   name.replace(QChar('\\'), QChar('_'));
   name.replace(QChar(':'), QChar('_'));
   name.replace(QChar('*'), QChar('_'));
   name.replace(QChar('?'), QChar('_'));
   name.replace(QChar('"'), QChar('_'));
   name.replace(QChar('<'), QChar('_'));
   name.replace(QChar('>'), QChar('_'));
   name.replace(QChar('|'), QChar('_'));
   return name;
  }
 };

 ArtifactRenderManager::ArtifactRenderManager()
  : impl_(new Impl())
 {
 }

 ArtifactRenderManager::~ArtifactRenderManager()
 {
  delete impl_;
 }

 ArtifactRenderManager& ArtifactRenderManager::instance()
 {
  static ArtifactRenderManager manager;
  return manager;
 }

 void ArtifactRenderManager::enqueue(const DummyRenderRequest& request)
 {
  if (!impl_) return;
  impl_->queue_.push_back(request);
 }

 int ArtifactRenderManager::queueSize() const
 {
  if (!impl_) return 0;
  return impl_->queue_.size();
 }

 void ArtifactRenderManager::clearQueue()
 {
  if (!impl_) return;
  impl_->queue_.clear();
 }

 DummyRenderResult ArtifactRenderManager::renderDummyImage(const DummyRenderRequest& request)
 {
  DummyRenderResult result;
  if (!impl_) {
   result.message = QStringLiteral("Render manager is not initialized.");
   return result;
  }

  QSize size = request.frameSize;
  if (size.width() <= 0 || size.height() <= 0) {
   size = QSize(1920, 1080);
  }

  const QString outDirPath = request.outputDirectory.isEmpty()
   ? Impl::defaultOutputDirectory()
   : request.outputDirectory;
  QDir outDir(outDirPath);
  if (!outDir.exists() && !outDir.mkpath(QStringLiteral("."))) {
   result.message = QStringLiteral("Failed to create output directory: %1").arg(outDirPath);
   return result;
  }

  const QString safeName = Impl::normalizeName(request.compositionName);
  const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
  const QString fileName = QStringLiteral("%1_%2_dummy.png").arg(safeName, stamp);
  const QString outputPath = outDir.filePath(fileName);

  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(24, 28, 36));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QLinearGradient grad(0, 0, 0, static_cast<qreal>(size.height()));
  grad.setColorAt(0.0, QColor(56, 94, 171));
  grad.setColorAt(1.0, QColor(22, 26, 34));
  painter.fillRect(image.rect(), grad);

  painter.setPen(QPen(QColor(255, 255, 255, 220), 2));
  painter.drawRect(image.rect().adjusted(12, 12, -13, -13));

  painter.setPen(QColor(240, 240, 240));
  QFont title = painter.font();
  title.setPointSize(18);
  title.setBold(true);
  painter.setFont(title);
  painter.drawText(QRect(24, 20, size.width() - 48, 44),
   Qt::AlignLeft | Qt::AlignVCenter,
   QStringLiteral("Artifact Dummy Render"));

  QFont body = painter.font();
  body.setPointSize(11);
  body.setBold(false);
  painter.setFont(body);
  painter.drawText(QRect(24, 70, size.width() - 48, size.height() - 96),
   Qt::TextWordWrap,
   QStringLiteral("Composition: %1\nComposition ID: %2\nResolution: %3 x %4\nGenerated: %5")
    .arg(safeName)
    .arg(request.compositionId)
    .arg(size.width())
    .arg(size.height())
    .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

  painter.end();

  if (!image.save(outputPath)) {
   result.message = QStringLiteral("Failed to save dummy render image: %1").arg(outputPath);
   return result;
  }

  result.success = true;
  result.outputPath = outputPath;
  result.message = QStringLiteral("Dummy render succeeded.");
  return result;
 }

};
