module;

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QMap>

module Translation.Manager;

namespace Artifact
{

class TranslationManager::Impl
{
public:
 QString locale_ = QStringLiteral("en");
 QMap<QString, QString> strings_;       // key -> translated value
 QMap<QString, QString> fallbacks_;     // key -> fallback value (usually English)
 QStringList availableLocales_;
 QString loadedDir_;

 // Flatten nested JSON object into dot-separated keys
 // { "menu": { "file": { "open": "Open" } } } -> { "menu.file.open": "Open" }
 static void flattenJson(const QJsonObject& obj, const QString& prefix, QMap<QString, QString>& out)
 {
  for (auto it = obj.begin(); it != obj.end(); ++it) {
   const QString key = prefix.isEmpty() ? it.key() : prefix + QStringLiteral(".") + it.key();
   if (it->isObject()) {
    flattenJson(it->toObject(), key, out);
   } else if (it->isString()) {
    out.insert(key, it->toString());
   } else if (it->isDouble()) {
    out.insert(key, QString::number(it->toDouble()));
   } else if (it->isBool()) {
    out.insert(key, it->toBool() ? QStringLiteral("true") : QStringLiteral("false"));
   }
  }
 }

 bool loadLocaleFile(const QString& filePath, QMap<QString, QString>& target)
 {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
   qWarning() << "[TranslationManager] cannot open:" << filePath;
   return false;
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  if (err.error != QJsonParseError::NoError) {
   qWarning() << "[TranslationManager] JSON parse error:" << err.errorString() << "in" << filePath;
   return false;
  }

  if (!doc.isObject()) {
   qWarning() << "[TranslationManager] root is not an object:" << filePath;
   return false;
  }

  flattenJson(doc.object(), QString(), target);
  return true;
 }

 void scanAvailableLocales(const QString& dirPath)
 {
  availableLocales_.clear();
  QDir dir(dirPath);
  const QFileInfoList entries = dir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files);
  for (const QFileInfo& fi : entries) {
   availableLocales_.append(fi.baseName());
  }
 }
};

TranslationManager::TranslationManager()
 : impl_(std::make_unique<Impl>())
{
}

TranslationManager::~TranslationManager() = default;

TranslationManager& TranslationManager::instance()
{
 static TranslationManager mgr;
 return mgr;
}

bool TranslationManager::loadFromDirectory(const QString& dirPath)
{
 QDir dir(dirPath);
 if (!dir.exists()) {
  qWarning() << "[TranslationManager] directory not found:" << dirPath;
  return false;
 }

 impl_->loadedDir_ = dirPath;
 impl_->scanAvailableLocales(dirPath);

 // Load fallback (English)
 const QString fallbackPath = dir.filePath(QStringLiteral("en.json"));
 if (QFile::exists(fallbackPath)) {
  impl_->loadLocaleFile(fallbackPath, impl_->fallbacks_);
 }

 // Load current locale
 const QString localePath = dir.filePath(impl_->locale_ + QStringLiteral(".json"));
 if (QFile::exists(localePath)) {
  impl_->strings_.clear();
  impl_->loadLocaleFile(localePath, impl_->strings_);
  qDebug() << "[TranslationManager] loaded" << impl_->strings_.size() << "strings for locale" << impl_->locale_;
 } else {
  qWarning() << "[TranslationManager] locale file not found:" << localePath;
 }

 return true;
}

bool TranslationManager::loadFromFile(const QString& filePath)
{
 impl_->strings_.clear();
 return impl_->loadLocaleFile(filePath, impl_->strings_);
}

void TranslationManager::setLocale(const QString& locale)
{
 if (locale == impl_->locale_) return;
 impl_->locale_ = locale;

 if (!impl_->loadedDir_.isEmpty()) {
  impl_->strings_.clear();
  const QString localePath = QDir(impl_->loadedDir_).filePath(locale + QStringLiteral(".json"));
  if (QFile::exists(localePath)) {
   impl_->loadLocaleFile(localePath, impl_->strings_);
   qDebug() << "[TranslationManager] switched to locale" << locale << "(" << impl_->strings_.size() << "strings)";
  } else {
   qWarning() << "[TranslationManager] locale file not found:" << localePath;
  }
 }
}

QString TranslationManager::locale() const
{
 return impl_->locale_;
}

QString TranslationManager::tr(const QString& key) const
{
 auto it = impl_->strings_.constFind(key);
 if (it != impl_->strings_.constEnd()) {
  return it.value();
 }
 auto fit = impl_->fallbacks_.constFind(key);
 if (fit != impl_->fallbacks_.constEnd()) {
  return fit.value();
 }
 return key;
}

QString TranslationManager::tr(const QString& key, const QString& fallback) const
{
 auto it = impl_->strings_.constFind(key);
 if (it != impl_->strings_.constEnd()) {
  return it.value();
 }
 auto fit = impl_->fallbacks_.constFind(key);
 if (fit != impl_->fallbacks_.constEnd()) {
  return fit.value();
 }
 return fallback;
}

QString TranslationManager::tr(const QString& key, const QStringList& args) const
{
 QString result = tr(key);
 for (int i = 0; i < args.size(); ++i) {
  result.replace(QStringLiteral("{%1}").arg(i), args[i]);
 }
 return result;
}

bool TranslationManager::hasKey(const QString& key) const
{
 return impl_->strings_.contains(key) || impl_->fallbacks_.contains(key);
}

QStringList TranslationManager::availableLocales() const
{
 return impl_->availableLocales_;
}

QStringList TranslationManager::loadedKeys() const
{
 QStringList keys;
 for (auto it = impl_->strings_.constBegin(); it != impl_->strings_.constEnd(); ++it) {
  keys.append(it.key());
 }
 keys.sort();
 return keys;
}

void TranslationManager::clear()
{
 impl_->strings_.clear();
 impl_->fallbacks_.clear();
 impl_->availableLocales_.clear();
 impl_->loadedDir_.clear();
 impl_->locale_ = QStringLiteral("en");
}

}
