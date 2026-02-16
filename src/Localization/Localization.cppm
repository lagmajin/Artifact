module;
#include <QString>
#include <unordered_map>
#include <string>

module Localization.Localization;

import Localization.Localization;
import std;
import Utils.String.UniString;

namespace ArtifactCore {

class LocalizationManager::Impl {
public:
    std::string currentLocale = "eng";
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data;

    Impl() {
        // minimal built-in dictionaries
        data["eng"]["menu.file"] = "File";
        data["eng"]["menu.edit"] = "Edit";
        data["eng"]["menu.view"] = "View";
        data["eng"]["menu.help"] = "Help";

        data["jp"]["menu.file"] = "ファイル";
        data["jp"]["menu.edit"] = "編集";
        data["jp"]["menu.view"] = "表示";
        data["jp"]["menu.help"] = "ヘルプ";
    }
};

LocalizationManager::LocalizationManager(): impl_(new Impl()) {}
LocalizationManager::~LocalizationManager(){ delete impl_; }

LocalizationManager* LocalizationManager::instance() {
    static LocalizationManager inst;
    return &inst;
}

void LocalizationManager::setLocale(const UniString& locale) {
    impl_->currentLocale = locale.toQString().toStdString();
}

UniString LocalizationManager::locale() const {
    return UniString(QString::fromStdString(impl_->currentLocale));
}

UniString LocalizationManager::translate(const UniString& key) const {
    QString qk = key.toQString();
    std::string k = qk.toStdString();
    auto it = impl_->data.find(impl_->currentLocale);
    if (it != impl_->data.end()) {
        auto it2 = it->second.find(k);
        if (it2 != it->second.end()) {
            return UniString(QString::fromStdString(it2->second));
        }
    }
    // fallback: return key
    return key;
}

}
