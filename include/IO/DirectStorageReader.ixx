module;
#include <cstddef>
#include <cstdint>
#include <QByteArray>
#include <QString>

export module Artifact.IO.DirectStorageReader;

export namespace Artifact {

struct DirectStorageReadResult {
    QByteArray bytes;
    QString error;
    bool usedDirectStorage = false;

    bool succeeded() const { return error.isEmpty(); }
};

struct DirectStorageReaderStats {
    std::uint64_t directStorageReads = 0;
    std::uint64_t portableReads = 0;
    std::uint64_t fallbackReads = 0;
    std::uint64_t failedReads = 0;
    std::uint64_t bytesRead = 0;
};

class DirectStorageReader final {
public:
    DirectStorageReader();
    ~DirectStorageReader();

    DirectStorageReader(const DirectStorageReader&) = delete;
    DirectStorageReader& operator=(const DirectStorageReader&) = delete;
    DirectStorageReader(DirectStorageReader&&) = delete;
    DirectStorageReader& operator=(DirectStorageReader&&) = delete;

    bool compiledWithDirectStorage() const;
    bool directStorageAvailable() const;
    QString backendDescription() const;

    DirectStorageReadResult readFile(const QString& path,
                                     std::uint64_t offset = 0,
                                     std::size_t size = 0);
    DirectStorageReaderStats stats() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact
