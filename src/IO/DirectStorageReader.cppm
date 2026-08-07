module;
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <QFile>
#include <QFileInfo>
#include <QString>

#if defined(_WIN32) && __has_include(<dstorage.h>)
#define ARTIFACT_HAS_DIRECTSTORAGE_SDK 1
#include <windows.h>
#include <wrl/client.h>
#include <dstorage.h>
#else
#define ARTIFACT_HAS_DIRECTSTORAGE_SDK 0
#endif

module Artifact.IO.DirectStorageReader;

namespace Artifact {

namespace {

DirectStorageReadResult portableRead(const QString& path,
                                     std::uint64_t offset,
                                     std::size_t requestedSize)
{
    DirectStorageReadResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    const qint64 fileSize = file.size();
    if (offset > static_cast<std::uint64_t>(std::max<qint64>(0, fileSize))) {
        result.error = QStringLiteral("Read offset is past end of file.");
        return result;
    }
    const std::uint64_t available =
        static_cast<std::uint64_t>(fileSize) - offset;
    const std::uint64_t readSize = requestedSize == 0
        ? available
        : std::min<std::uint64_t>(available, requestedSize);
    if (readSize > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)()) ||
        readSize > static_cast<std::uint64_t>((std::numeric_limits<qint64>::max)())) {
        result.error = QStringLiteral("Requested file range is too large.");
        return result;
    }
    if (!file.seek(static_cast<qint64>(offset))) {
        result.error = file.errorString();
        return result;
    }
    result.bytes = file.read(static_cast<qint64>(readSize));
    if (result.bytes.size() != static_cast<qsizetype>(readSize)) {
        result.error = file.errorString().isEmpty()
            ? QStringLiteral("Short file read.")
            : file.errorString();
        result.bytes.clear();
    }
    return result;
}

#if ARTIFACT_HAS_DIRECTSTORAGE_SDK
using Microsoft::WRL::ComPtr;
using DStorageGetFactoryFn = HRESULT(WINAPI*)(REFIID, void**);

QString hresultText(HRESULT hr)
{
    return QStringLiteral("HRESULT 0x%1")
        .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}
#endif

} // namespace

struct DirectStorageReader::Impl {
    mutable std::mutex mutex;
    DirectStorageReaderStats stats;
    QString backend;

#if ARTIFACT_HAS_DIRECTSTORAGE_SDK
    HMODULE runtime = nullptr;
    ComPtr<IDStorageFactory> factory;
    ComPtr<IDStorageQueue> queue;
    ComPtr<IDStorageStatusArray> status;

    Impl()
    {
        runtime = LoadLibraryW(L"dstorage.dll");
        if (!runtime) {
            backend = QStringLiteral("portable (dstorage.dll unavailable)");
            return;
        }
        const auto getFactory = reinterpret_cast<DStorageGetFactoryFn>(
            GetProcAddress(runtime, "DStorageGetFactory"));
        if (!getFactory) {
            backend = QStringLiteral("portable (DStorageGetFactory unavailable)");
            return;
        }
        HRESULT hr = getFactory(IID_PPV_ARGS(&factory));
        if (FAILED(hr) || !factory) {
            backend = QStringLiteral("portable (%1)").arg(hresultText(hr));
            return;
        }

        DSTORAGE_QUEUE_DESC desc{};
        desc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        desc.Capacity = DSTORAGE_MIN_QUEUE_CAPACITY;
        desc.Priority = DSTORAGE_PRIORITY_NORMAL;
        desc.Name = "Artifact.DirectStorage.FileToMemory";
        desc.Device = nullptr;
        hr = factory->CreateQueue(&desc, IID_PPV_ARGS(&queue));
        if (FAILED(hr) || !queue) {
            backend = QStringLiteral("portable (queue %1)").arg(hresultText(hr));
            return;
        }
        hr = factory->CreateStatusArray(
            1, "Artifact.DirectStorage.Status", IID_PPV_ARGS(&status));
        if (FAILED(hr) || !status) {
            queue.Reset();
            backend = QStringLiteral("portable (status %1)").arg(hresultText(hr));
            return;
        }
        backend = QStringLiteral("DirectStorage file-to-memory");
    }

    ~Impl()
    {
        status.Reset();
        queue.Reset();
        factory.Reset();
        if (runtime) {
            FreeLibrary(runtime);
            runtime = nullptr;
        }
    }

    DirectStorageReadResult directRead(const QString& path,
                                       std::uint64_t offset,
                                       std::size_t requestedSize)
    {
        DirectStorageReadResult result;
        if (!factory || !queue || !status) {
            result.error = QStringLiteral("DirectStorage is unavailable.");
            return result;
        }

        ComPtr<IDStorageFile> file;
        HRESULT hr = factory->OpenFile(
            reinterpret_cast<const wchar_t*>(path.utf16()),
            IID_PPV_ARGS(&file));
        if (FAILED(hr) || !file) {
            result.error = QStringLiteral("DirectStorage OpenFile failed: %1")
                               .arg(hresultText(hr));
            return result;
        }

        BY_HANDLE_FILE_INFORMATION fileInfo{};
        hr = file->GetFileInformation(&fileInfo);
        if (FAILED(hr)) {
            result.error = QStringLiteral("DirectStorage file information failed: %1")
                               .arg(hresultText(hr));
            return result;
        }
        const std::uint64_t fileSize =
            (static_cast<std::uint64_t>(fileInfo.nFileSizeHigh) << 32u) |
            static_cast<std::uint64_t>(fileInfo.nFileSizeLow);
        if (offset > fileSize) {
            result.error = QStringLiteral("Read offset is past end of file.");
            return result;
        }
        const std::uint64_t available = fileSize - offset;
        const std::uint64_t readSize64 = requestedSize == 0
            ? available
            : std::min<std::uint64_t>(available, requestedSize);
        if (readSize64 > (std::numeric_limits<std::uint32_t>::max)() ||
            readSize64 > static_cast<std::uint64_t>((std::numeric_limits<qsizetype>::max)())) {
            result.error = QStringLiteral("DirectStorage request exceeds the 32-bit request limit.");
            return result;
        }
        const auto readSize = static_cast<std::uint32_t>(readSize64);
        result.bytes.resize(static_cast<qsizetype>(readSize));
        if (readSize == 0) {
            result.usedDirectStorage = true;
            return result;
        }

        DSTORAGE_REQUEST request{};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
        request.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
        request.Source.File.Source = file.Get();
        request.Source.File.Offset = offset;
        request.Source.File.Size = readSize;
        request.UncompressedSize = readSize;
        request.Destination.Memory.Buffer = result.bytes.data();
        request.Destination.Memory.Size = readSize;
        request.Name = "Artifact.DirectStorage.Read";

        queue->EnqueueRequest(&request);
        queue->EnqueueStatus(status.Get(), 0);
        queue->Submit();
        while (!status->IsComplete(0)) {
            SwitchToThread();
        }
        hr = status->GetHResult(0);
        if (FAILED(hr)) {
            result.bytes.clear();
            result.error = QStringLiteral("DirectStorage read failed: %1")
                               .arg(hresultText(hr));
            return result;
        }
        result.usedDirectStorage = true;
        return result;
    }
#else
    Impl()
        : backend(QStringLiteral("portable (DirectStorage SDK not compiled)"))
    {
    }
#endif
};

DirectStorageReader::DirectStorageReader()
    : impl_(new Impl())
{
}

DirectStorageReader::~DirectStorageReader()
{
    delete impl_;
    impl_ = nullptr;
}

bool DirectStorageReader::compiledWithDirectStorage() const
{
    return ARTIFACT_HAS_DIRECTSTORAGE_SDK != 0;
}

bool DirectStorageReader::directStorageAvailable() const
{
#if ARTIFACT_HAS_DIRECTSTORAGE_SDK
    return impl_ && impl_->factory && impl_->queue && impl_->status;
#else
    return false;
#endif
}

QString DirectStorageReader::backendDescription() const
{
    return impl_ ? impl_->backend : QStringLiteral("uninitialized");
}

DirectStorageReadResult DirectStorageReader::readFile(const QString& path,
                                                      std::uint64_t offset,
                                                      std::size_t size)
{
    if (!impl_) {
        return {{}, QStringLiteral("DirectStorageReader is uninitialized."), false};
    }
    const QString normalizedPath = QFileInfo(path).absoluteFilePath();

#if ARTIFACT_HAS_DIRECTSTORAGE_SDK
    {
        std::scoped_lock lock(impl_->mutex);
        if (impl_->factory && impl_->queue && impl_->status) {
            DirectStorageReadResult direct =
                impl_->directRead(normalizedPath, offset, size);
            if (direct.succeeded()) {
                ++impl_->stats.directStorageReads;
                impl_->stats.bytesRead +=
                    static_cast<std::uint64_t>(direct.bytes.size());
                return direct;
            }
            ++impl_->stats.fallbackReads;
        }
    }
#endif

    DirectStorageReadResult portable = portableRead(normalizedPath, offset, size);
    {
        std::scoped_lock lock(impl_->mutex);
        if (portable.succeeded()) {
            ++impl_->stats.portableReads;
            impl_->stats.bytesRead +=
                static_cast<std::uint64_t>(portable.bytes.size());
        } else {
            ++impl_->stats.failedReads;
        }
    }
    return portable;
}

DirectStorageReaderStats DirectStorageReader::stats() const
{
    if (!impl_) return {};
    const std::scoped_lock lock(impl_->mutex);
    return impl_->stats;
}

} // namespace Artifact
