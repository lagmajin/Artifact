module;
#include <memory>
#include <QString>
#include <vector>

export module Generator.Manager;

import Generator.Clone;

export namespace Artifact
{

export class GeneratorManager {
public:
    GeneratorManager() = default;
    GeneratorManager(const GeneratorManager&) = delete;
    GeneratorManager& operator=(const GeneratorManager&) = delete;

    std::shared_ptr<CloneGenerator> create(const QString& id);
    bool add(const QString& id, std::shared_ptr<CloneGenerator> generator);
    bool remove(const QString& id);
    std::shared_ptr<CloneGenerator> get(const QString& id) const;
    bool contains(const QString& id) const;
    std::vector<QString> ids() const;
    void clear();
};

};
