module;
#include <utility>
#include <memory>
#include <QString>
#include <QStringList>
#include <QJsonObject>

export module Translation.Manager;


export namespace Artifact
{

 class TranslationManager
 {
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  TranslationManager();
  ~TranslationManager();

 public:
  static TranslationManager& instance();

  TranslationManager(const TranslationManager&) = delete;
  TranslationManager& operator=(const TranslationManager&) = delete;

  bool loadFromDirectory(const QString& dirPath);
  bool loadFromFile(const QString& filePath);

  void setLocale(const QString& locale);
  QString locale() const;

  QString tr(const QString& key) const;
  QString tr(const QString& key, const QString& fallback) const;
  QString tr(const QString& key, const QStringList& args) const;

  bool hasKey(const QString& key) const;

  QStringList availableLocales() const;
  QStringList loadedKeys() const;

  void clear();
 };

};
