module;
export module Localization.Localization;

import std;
import Utils.String.UniString;

export namespace ArtifactCore {

using namespace ArtifactCore;

class LocalizationManager {
private:
    class Impl;
    Impl* impl_;
public:
    LocalizationManager();
    ~LocalizationManager();

    // singleton accessor
    static LocalizationManager* instance();

    // Set locale code like "eng" or "jp"
    void setLocale(const UniString& locale);
    UniString locale() const;

    // Translate a key (e.g., "menu.file") into localized string.
    // If not found, returns the key itself.
    UniString translate(const UniString& key) const;
};

}
