module;
#include <utility>
#include <memory>
#include <QString>
#include <QStringList>
#include <QFileInfo>

module Translation.Manager;

import Core.Localization;

namespace Artifact {

class TranslationManager::Impl {};

TranslationManager::TranslationManager() : impl_(std::make_unique<Impl>()) {}
TranslationManager::~TranslationManager() = default;

TranslationManager& TranslationManager::instance() {
 static TranslationManager manager;
 return manager;
}

bool TranslationManager::loadFromDirectory(const QString& dirPath) {
 return ArtifactCore::LocalizationManager::instance().loadFromDirectory(dirPath);
}

bool TranslationManager::loadFromFile(const QString& filePath) {
 auto& manager = ArtifactCore::LocalizationManager::instance();
 manager.setLanguageCode(QFileInfo(filePath).baseName());
 return manager.loadFromFile(filePath, manager.language());
}

void TranslationManager::setLocale(const QString& locale) {
 ArtifactCore::LocalizationManager::instance().setLanguageCode(locale);
}

QString TranslationManager::locale() const {
 return ArtifactCore::LocalizationManager::instance().languageCode();
}

QString TranslationManager::tr(const QString& key) const {
 return ArtifactCore::LocalizationManager::instance().translate(key);
}

QString TranslationManager::tr(const QString& key, const QString& fallback) const {
 const QString value = tr(key);
 return value == key ? fallback : value;
}

QString TranslationManager::tr(const QString& key, const QStringList& args) const {
 QString value = tr(key);
 for (int i = 0; i < args.size(); ++i) {
  value.replace(QStringLiteral("{%1}").arg(i), args.at(i));
 }
 return value;
}

bool TranslationManager::hasKey(const QString& key) const {
 return ArtifactCore::LocalizationManager::instance().loadedKeys().contains(key);
}

QStringList TranslationManager::availableLocales() const {
 return ArtifactCore::LocalizationManager::instance().availableLocales();
}

QStringList TranslationManager::loadedKeys() const {
 return ArtifactCore::LocalizationManager::instance().loadedKeys();
}

QStringList TranslationManager::missingKeys() const {
 return ArtifactCore::LocalizationManager::instance().missingKeys();
}

QStringList TranslationManager::untranslatedKeys() const {
 return ArtifactCore::LocalizationManager::instance().untranslatedKeys();
}

void TranslationManager::clear() {
 ArtifactCore::LocalizationManager::instance().clearTranslations();
}

}
