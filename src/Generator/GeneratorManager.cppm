module;
#include <mutex>
#include <QHash>
#include <algorithm>
#include <utility>

module Generator.Manager;

import Generator.Clone;

namespace Artifact {

namespace {
struct ManagerState {
    mutable std::mutex mutex;
    QHash<QString, std::shared_ptr<CloneGenerator>> generators;
};

ManagerState& state() {
    static ManagerState value;
    return value;
}
}

std::shared_ptr<CloneGenerator> GeneratorManager::create(const QString& id) {
    const QString key = id.trimmed();
    if (key.isEmpty()) return {};
    auto generator = std::make_shared<CloneGenerator>();
    std::lock_guard lock(state().mutex);
    state().generators[key] = generator;
    return generator;
}

bool GeneratorManager::add(const QString& id, std::shared_ptr<CloneGenerator> generator) {
    const QString key = id.trimmed();
    if (key.isEmpty() || !generator) return false;
    std::lock_guard lock(state().mutex);
    if (state().generators.contains(key)) return false;
    state().generators.insert(key, std::move(generator));
    return true;
}

bool GeneratorManager::remove(const QString& id) {
    std::lock_guard lock(state().mutex);
    return state().generators.remove(id.trimmed()) != 0;
}

std::shared_ptr<CloneGenerator> GeneratorManager::get(const QString& id) const {
    std::lock_guard lock(state().mutex);
    const auto it = state().generators.constFind(id.trimmed());
    return it == state().generators.cend() ? std::shared_ptr<CloneGenerator>{} : it.value();
}

bool GeneratorManager::contains(const QString& id) const {
    std::lock_guard lock(state().mutex);
    return state().generators.contains(id.trimmed());
}

std::vector<QString> GeneratorManager::ids() const {
    std::lock_guard lock(state().mutex);
    std::vector<QString> result;
    result.reserve(state().generators.size());
    for (auto it = state().generators.cbegin(); it != state().generators.cend(); ++it)
        result.push_back(it.key());
    std::sort(result.begin(), result.end(), [](const QString& lhs, const QString& rhs) {
        return lhs < rhs;
    });
    return result;
}

void GeneratorManager::clear() {
    std::lock_guard lock(state().mutex);
    state().generators.clear();
}

} // namespace Artifact
