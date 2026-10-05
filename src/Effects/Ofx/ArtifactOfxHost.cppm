module;
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QCoreApplication>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFile>
#include <QHash>
#include <QSettings>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <ofx/ofxCore.h>
#include <ofx/ofxImageEffect.h>
#include <ofx/ofxParam.h>
#include <ofx/ofxProperty.h>
#include <ofx/ofxMemory.h>
#include <ofx/ofxTimeLine.h>
#include <ofx/ofxMessage.h>
#include <ofx/ofxProgress.h>
#include <ofx/ofxInteract.h>

// kOfxImageEffectPropStatusMessage is part of the OFX spec (an outArgs key the
// render action may set) but is absent from the vendored 1.5 headers, which
// only cover the deprecated kOfxStatusMessage naming. Defined locally so the
// spec key can be passed through unchanged.
#ifndef kOfxImageEffectPropStatusMessage
#define kOfxImageEffectPropStatusMessage "OfxImageEffectPropStatusMessage"
#endif

export module Artifact.Effect.Ofx.Host;

import Property.Abstract;
import Time.Rational;
import Utils.String.UniString;
import Artifact.Effect.Context;
import Memory.SharedPtr;

namespace Artifact {
namespace Ofx {

struct OfxPluginDescriptor;
export struct ImageEffectState;
class ArtifactOfxHost;

using namespace ArtifactCore;

// Opaque handle for a loaded plugin binary. The three helpers below abstract
// the platform loader (LoadLibrary/FreeLibrary/GetProcAddress on Win32,
// dlopen/dlclose/dlsym elsewhere) so the scanner and the effect bridge never
// touch a platform API directly.
#ifdef _WIN32
using OfxLibraryHandle = HMODULE;
#else
using OfxLibraryHandle = void *;
#endif

// Opens a plugin binary. Returns nullptr when the file cannot be mapped.
OfxLibraryHandle openPluginLibrary(const QString &binaryPath) {
#ifdef _WIN32
  const auto *widePath = reinterpret_cast<LPCWSTR>(binaryPath.utf16());
  return LoadLibraryW(widePath);
#else
  // dlopen needs a NUL-terminated native path; QString::utf8 is not guaranteed
  // to be NUL-terminated, so go through QByteArray.
  const QByteArray nativePath = QFile::encodeName(binaryPath);
  return dlopen(nativePath.constData(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void closePluginLibrary(OfxLibraryHandle handle) {
  if (!handle) {
    return;
  }
#ifdef _WIN32
  FreeLibrary(handle);
#else
  dlclose(handle);
#endif
}

// Resolves an exported symbol, or nullptr when it is absent.
// Exported so the effect bridge re-resolves the OFX entry points through the
// same portable path instead of calling GetProcAddress directly.
export void *resolvePluginSymbol(OfxLibraryHandle handle,
                                  const char *symbolName) {
  if (!handle || !symbolName) {
    return nullptr;
  }
#ifdef _WIN32
  return reinterpret_cast<void *>(GetProcAddress(handle, symbolName));
#else
  return dlsym(handle, symbolName);
#endif
}

export struct OfxPropertySetStruct {
  enum class Kind {
    Unknown,
    Pointer,
    String,
    Double,
    Int
  };

  struct Entry {
    Kind kind = Kind::Unknown;
    std::vector<QVariant> values;
    std::vector<QVariant> defaults;
    std::vector<std::string> stringStorage;
  };

  std::unordered_map<std::string, Entry> entries;
};

export struct ParamState {
  OfxPropertySetStruct properties;
  QString paramType;
  std::vector<QVariant> currentValues;
  QString currentStringValue;
  std::string currentUtf8Value;
  // The editor-facing property this OFX parameter is bridged to. The OFX
  // keyframe API is implemented on top of AbstractProperty's own keyframe
  // track, so the suite functions operate on that property rather than
  // maintaining a second set of keys.
  //
  // Stored by value: previewProperties is a vector whose elements move when it
  // grows, so a pointer into it would dangle before the first render.
  AbstractProperty property;
  // Time base used to convert between OFX times (seconds) and the property's
  // rational time. Defaults to the host's declared frame rate.
  double frameRate = 30.0;
};

export struct ParamSetState {
  OfxPropertySetStruct properties;
  std::unordered_map<std::string, std::unique_ptr<ParamState>> params;
  std::vector<std::string> paramOrder;
};

export struct ClipState {
  OfxPropertySetStruct properties;
  // A clip handle is only meaningful together with the effect instance that
  // owns it. The host hands plugin code an OfxImageClipHandle that must be
  // resolvable back to that instance during a render action, so the ownership
  // link lives in the clip itself rather than being recovered by scanning the
  // loaded-plugin list (which only ever contains descriptor clips).
  ImageEffectState *owner = nullptr;
  QString clipName;
};

export struct ImageMemoryState {
  std::vector<unsigned char> bytes;
  int lockCount = 0;
};

export struct ImageEffectState {
  OfxPropertySetStruct properties;
  ParamSetState paramSet;
  std::unordered_map<std::string, std::unique_ptr<ClipState>> clips;
  bool abortRequested = false;

  // Render-time state: pixel data from the host for clipGetImage
  struct RenderFrameData {
    const unsigned char* srcPixelData = nullptr;
    int srcWidth = 0;
    int srcHeight = 0;
    int srcRowBytes = 0;
    const unsigned char* dstPixelData = nullptr;
    int dstRowBytes = 0;
    // The frame time the instance is currently being evaluated at, exposed to
    // plugins through the TimeLine suite.
    double currentTime = 0.0;
  };
  RenderFrameData renderFrame;

  // Scratch property sets returned by clipGetImage. The OFX spec says an
  // image handle is valid only for the duration of the clipGetImage call, but
  // plugins routinely hold on to the handle they got for the current input and
  // then fetch the output clip. One buffer per clip keeps those two handles
  // distinct instead of aliasing a single shared set.
  std::unordered_map<std::string, OfxPropertySetStruct> clipImageScratch;

  // Called once when the host is about to hand this instance to a render
  // action, so the source/destination buffers match the frame actually being
  // processed. Set by the bridge before each pluginActionRender call.
  void (*prepareRenderFrame)(ImageEffectState *state,
                             const unsigned char *srcPixelData,
                             int srcWidth, int srcHeight,
                             int srcRowBytes,
                             unsigned char *dstPixelData,
                             int dstRowBytes) = nullptr;

  // Identity contract used by clipGetImage to decide whether a clip carries
  // the host input (source) or the render target (output).
  bool isOutputClip(const char *clipName) const;
};

bool ImageEffectState::isOutputClip(const char *clipName) const {
  if (!clipName) {
    return false;
  }
  // OFX reserves the exact name "Output" for the output clip of an image
  // effect. Compare case-insensitively because plugins are inconsistent about
  // the casing they pass back through paramGetHandle/clipGetHandle.
  return QString::fromUtf8(clipName).compare(QStringLiteral("Output"),
                                             Qt::CaseInsensitive) == 0;
}

std::unique_ptr<ParamState> cloneParamState(const ParamState &src) {
  auto dst = std::make_unique<ParamState>();
  dst->properties = src.properties;
  dst->paramType = src.paramType;
  dst->currentValues = src.currentValues;
  dst->currentStringValue = src.currentStringValue;
  dst->currentUtf8Value = src.currentUtf8Value;
  // The bridged property is copied along with its keyframes so a render
  // instance starts from the same authored state as the descriptor.
  dst->property = src.property;
  dst->frameRate = src.frameRate;
  return dst;
}

ParamSetState cloneParamSetState(const ParamSetState &src) {
  ParamSetState dst;
  dst.properties = src.properties;
  dst.paramOrder = src.paramOrder;
  for (const auto &[name, param] : src.params) {
    dst.params.emplace(name, param ? cloneParamState(*param) : nullptr);
  }
  return dst;
}

export struct OfxPluginDescriptor {
  UniString pluginPath;
  UniString identifier;
  UniString version;
  QStringList supportedContexts;
  std::vector<AbstractProperty> previewProperties;
  SharedPtr<ImageEffectState> descriptorState;
  OfxLibraryHandle libraryHandle = nullptr;
  // Host scan generation this descriptor was produced by. A rescan bumps the
  // host generation and frees the previous libraries, so any effect object that
  // captured a descriptor can detect that its library handle went stale instead
  // of calling GetProcAddress on unmapped memory.
  std::uint64_t generation = 0;
};

// Number of times a plugin may fault before it is blacklisted.
inline constexpr int kOfxCrashLimit = 3;

// Identifier of the plugin currently executing inside the crash guard, or null
// when no plugin call is in flight. Read by the abort reporter below so a CRT
// abort can name the plugin that triggered it, and so a plugin that aborts is
// still attributed for blacklisting.
inline const char *&activePluginIdentifier() {
  static const char *value = nullptr;
  return value;
}

// Installs a handler that records a CRT abort / fast-fail, which __try/__except
// cannot catch because it is raised by abort() rather than as an exception. The
// report names the plugin that was executing so a bad plugin can be blacklisted
// instead of silently taking the process down.
//
// Heap corruption is NOT handled here: it is detected by the CRT long after the
// offending write, during unrelated allocator activity, so in-process recovery
// is not possible without a debug heap that is unavailable in release builds.
// Only the plugin isolation path can contain it, which is not implemented.
void installOfxAbortReporter();

#ifdef _WIN32
// Abort paths reach us through the vectored handler rather than __try/__except:
// abort() raises no exception, so a fast-fail or CRT abort would otherwise end
// the process with no record of which plugin caused it. Reporting from a vectored
// handler is risky, so this only stamps a marker file and leaves termination to
// the default handler.
LONG CALLBACK ofxVectoredCrashReporter(EXCEPTION_POINTERS *exceptionInfo) {
  if (!exceptionInfo) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  const DWORD code = exceptionInfo->ExceptionRecord
                         ? exceptionInfo->ExceptionRecord->ExceptionCode
                         : 0;
  const bool isAbortLike =
      code == 0x40000015 /* STATUS_FATAL_APP_EXIT */ ||
      code == 0xC0000409 /* STATUS_STACK_BUFFER_OVERRUN */ ||
      code == 0xC0000374 /* STATUS_HEAP_CORRUPTION */;
  if (!isAbortLike) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  const char *identifier = activePluginIdentifier();
  if (identifier) {
    // Write only an identifier, no allocation: this runs with a corrupted heap
    // and may be the last thing that executes.
    OutputDebugStringA("[OFX] abort-like fault inside plugin: ");
    OutputDebugStringA(identifier);
    OutputDebugStringA("\n");
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void installOfxAbortReporter() {
#ifdef _WIN32
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;
  // The core CrashHandler installs an unhandled filter; a vectored handler runs
  // first and only observes, so both mechanisms stay in play.
  AddVectoredExceptionHandler(1, &ofxVectoredCrashReporter);
#endif
}

// Invokes a plugin entry point with a structured exception guard.
//
// A third-party plugin that faults inside its render action would otherwise take
// the whole application down. __try/__except turns an access violation into a
// failure status so the caller can disable the offending plugin and continue.
//
// SEH and C++ object unwinding cannot be combined in one function, so this
// helper only touches POD state and defers all real work to the caller's
// arguments; it deliberately owns no non-trivial locals.
//
// Returns true when the call completed (status is written to `outStatus`),
// false when the plugin faulted.
bool callPluginEntryPoint(OfxPluginEntryPoint *entryPoint, const char *action,
                          const void *instance, OfxPropertySetHandle inArgs,
                          OfxPropertySetHandle outArgs, OfxStatus *outStatus) {
  if (!entryPoint || !outStatus) {
    return false;
  }
#ifdef _WIN32
  __try {
    *outStatus = (*entryPoint)(action, instance, inArgs, outArgs);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Deliberately swallow: the caller records the failure against the plugin
    // identifier and the plugin gets disabled once it exceeds the crash limit.
    *outStatus = kOfxStatFailed;
    return false;
  }
#else
  // Non-Windows builds have no SEH; run unguarded so behaviour matches the
  // platform's own crash reporting.
  *outStatus = (*entryPoint)(action, instance, inArgs, outArgs);
  return true;
#endif
}

// Declared here, outside the anonymous namespace, and defined after the
// ArtifactOfxHost class body. The action helpers between the anonymous
// namespace and the class need them, and a declaration inside the anonymous
// namespace would name a different entity from the out-of-class definition.
// `identifier` may be null for actions issued before the plugin is fully
// described, in which case a fault is counted but not attributed.
OfxStatus dispatchOfxAction(OfxPlugin *plugin, const char *identifier,
                            const char *action, const void *instance,
                            OfxPropertySetHandle inArgs,
                            OfxPropertySetHandle outArgs);

bool ofxPluginIsBlacklisted(const char *identifier);

OfxStatus pluginActionLoad(OfxPlugin *plugin);

OfxStatus pluginActionDescribe(OfxPlugin *plugin,
                               ImageEffectState &descriptorState);

ImageEffectState *imageEffectForClip(const ClipState *clip);

namespace {

using PropertySet = OfxPropertySetStruct;
using PropertyEntry = OfxPropertySetStruct::Entry;
using PropertyKind = OfxPropertySetStruct::Kind;

using OfxGetNumberOfPluginsFn = int (*)();
using OfxGetPluginFn = OfxPlugin *(*)(int);

std::vector<QVariant> defaultValuesForOfxParamType(const QString &paramType);
OfxStatus paramGetValueImpl(OfxParamHandle paramHandle, va_list args);
OfxStatus paramSetValueImpl(OfxParamHandle paramHandle, va_list args);
int paramComponentCount(const QString &paramType);
RationalTime toRationalTime(const ParamState &param, OfxTime time);
OfxTime toOfxTime(const ParamState &param, const RationalTime &time);
// Returns component `index` of a parameter's value: the evaluated value when
// one was produced for this frame, otherwise the stored current value.
QVariant resolveComponent(const ParamState &param, const QVariant &evaluated,
                          bool haveEvaluated, int index);
double sampleComponentAt(const ParamState &param, OfxTime time, int index);
bool supportsNumericAnalysis(const QString &paramType);

QString toQString(const char *value) {
  return value ? QString::fromUtf8(value) : QString();
}

std::string toStdString(const QString &value) {
  return value.toUtf8().toStdString();
}

PropertySet *asSet(OfxPropertySetHandle properties) {
  return reinterpret_cast<PropertySet *>(properties);
}

PropertyEntry *ensureEntry(PropertySet *set, const char *property,
                           PropertyKind kind) {
  if (!set || !property) {
    return nullptr;
  }

  auto &entry = set->entries[std::string(property)];
  if (entry.kind == PropertyKind::Unknown) {
    entry.kind = kind;
  }
  return &entry;
}

const PropertyEntry *findEntry(const PropertySet *set, const char *property) {
  if (!set || !property) {
    return nullptr;
  }
  const auto it = set->entries.find(std::string(property));
  if (it == set->entries.end()) {
    return nullptr;
  }
  return &it->second;
}

void syncDefaults(PropertyEntry &entry) {
  if (entry.defaults.empty()) {
    entry.defaults = entry.values;
  }
}

void syncStringStorage(PropertyEntry &entry) {
  if (entry.kind != PropertyKind::String) {
    return;
  }
  entry.stringStorage.resize(entry.values.size());
  for (size_t i = 0; i < entry.values.size(); ++i) {
    entry.stringStorage[i] = entry.values[i].toString().toUtf8().toStdString();
  }
}

OfxStatus setPointerValue(PropertySet *set, const char *property, int index,
                          void *value) {
  if (index < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *entry = ensureEntry(set, property, PropertyKind::Pointer);
  if (!entry) {
    return kOfxStatErrBadHandle;
  }
  if (static_cast<size_t>(index) >= entry->values.size()) {
    entry->values.resize(static_cast<size_t>(index) + 1);
  }
  entry->values[static_cast<size_t>(index)] =
      QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(value));
  syncDefaults(*entry);
  return kOfxStatOK;
}

OfxStatus setStringValue(PropertySet *set, const char *property, int index,
                         const char *value) {
  if (index < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *entry = ensureEntry(set, property, PropertyKind::String);
  if (!entry) {
    return kOfxStatErrBadHandle;
  }
  if (static_cast<size_t>(index) >= entry->values.size()) {
    entry->values.resize(static_cast<size_t>(index) + 1);
    entry->stringStorage.resize(static_cast<size_t>(index) + 1);
  }
  const std::string text = value ? value : "";
  entry->stringStorage[static_cast<size_t>(index)] = text;
  entry->values[static_cast<size_t>(index)] = QString::fromUtf8(text.data(),
                                                                static_cast<int>(text.size()));
  syncDefaults(*entry);
  syncStringStorage(*entry);
  return kOfxStatOK;
}

OfxStatus setDoubleValue(PropertySet *set, const char *property, int index,
                         double value) {
  if (index < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *entry = ensureEntry(set, property, PropertyKind::Double);
  if (!entry) {
    return kOfxStatErrBadHandle;
  }
  if (static_cast<size_t>(index) >= entry->values.size()) {
    entry->values.resize(static_cast<size_t>(index) + 1);
  }
  entry->values[static_cast<size_t>(index)] = value;
  syncDefaults(*entry);
  return kOfxStatOK;
}

OfxStatus setIntValue(PropertySet *set, const char *property, int index,
                      int value) {
  if (index < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *entry = ensureEntry(set, property, PropertyKind::Int);
  if (!entry) {
    return kOfxStatErrBadHandle;
  }
  if (static_cast<size_t>(index) >= entry->values.size()) {
    entry->values.resize(static_cast<size_t>(index) + 1);
  }
  entry->values[static_cast<size_t>(index)] = value;
  syncDefaults(*entry);
  return kOfxStatOK;
}

template <typename Getter>
OfxStatus fillValues(const PropertySet *set, const char *property, int index,
                     Getter &&getter) {
  if (index < 0) {
    return kOfxStatErrBadIndex;
  }
  const auto *entry = findEntry(set, property);
  if (!entry) {
    return kOfxStatErrUnknown;
  }
  if (static_cast<size_t>(index) >= entry->values.size()) {
    return kOfxStatErrBadIndex;
  }
  return getter(*entry, static_cast<size_t>(index));
}

OfxStatus resetEntry(PropertySet *set, const char *property) {
  auto *entry = ensureEntry(set, property, PropertyKind::Unknown);
  if (!entry) {
    return kOfxStatErrBadHandle;
  }
  if (!entry->defaults.empty()) {
    entry->values = entry->defaults;
    syncStringStorage(*entry);
  } else {
    entry->values.clear();
    entry->stringStorage.clear();
  }
  return kOfxStatOK;
}

OfxStatus getDimension(const PropertySet *set, const char *property,
                       int *count) {
  if (!count) {
    return kOfxStatErrBadHandle;
  }
  const auto *entry = findEntry(set, property);
  if (!entry) {
    *count = 0;
    return kOfxStatErrUnknown;
  }
  *count = static_cast<int>(entry->values.size());
  return kOfxStatOK;
}

OfxPropertySuiteV1 makePropertySuite();
const OfxPropertySuiteV1 *propertySuite();

// Logs the message a plugin passes to the message suite. This is also the only
// diagnostic channel available to clipGetImage, which has no outArgs.
OfxStatus ofxMessageFunc(void * /*handle*/, const char * /*type*/,
                         const char * /*messageId*/, const char *format, ...) {
  if (!format) {
    return kOfxStatOK;
  }
  // Walk the format string substituting printf-style %s arguments. Logging the
  // raw template would drop every substitution the plugin supplied; anything
  // other than %s is copied through verbatim.
  QString message;
  va_list args;
  va_start(args, format);
  const QByteArray pattern = QByteArray(format);
  for (int i = 0; i < pattern.size();) {
    if (pattern[i] == '%' && i + 1 < pattern.size() && pattern[i + 1] == 's') {
      const char *value = va_arg(args, const char *);
      message += QString::fromUtf8(value ? value : "(null)");
      i += 2;
      continue;
    }
    message += QChar::fromLatin1(pattern[i]);
    ++i;
  }
  va_end(args);
  qWarning("[OFX] %s", message.toUtf8().constData());
  return kOfxStatOK;
}

OfxStatus propSetPointer(OfxPropertySetHandle properties, const char *property,
                         int index, void *value) {
  return setPointerValue(asSet(properties), property, index, value);
}

OfxStatus propSetString(OfxPropertySetHandle properties, const char *property,
                        int index, const char *value) {
  return setStringValue(asSet(properties), property, index, value);
}

OfxStatus propSetDouble(OfxPropertySetHandle properties, const char *property,
                        int index, double value) {
  return setDoubleValue(asSet(properties), property, index, value);
}

OfxStatus propSetInt(OfxPropertySetHandle properties, const char *property,
                     int index, int value) {
  return setIntValue(asSet(properties), property, index, value);
}

OfxStatus propSetPointerN(OfxPropertySetHandle properties, const char *property,
                          int count, void *const *value) {
  if (count < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *set = asSet(properties);
  for (int i = 0; i < count; ++i) {
    const OfxStatus status = setPointerValue(set, property, i, value ? value[i] : nullptr);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propSetStringN(OfxPropertySetHandle properties, const char *property,
                         int count, const char *const *value) {
  if (count < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *set = asSet(properties);
  for (int i = 0; i < count; ++i) {
    const OfxStatus status = setStringValue(set, property, i, value ? value[i] : nullptr);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propSetDoubleN(OfxPropertySetHandle properties, const char *property,
                         int count, const double *value) {
  if (count < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *set = asSet(properties);
  for (int i = 0; i < count; ++i) {
    const OfxStatus status = setDoubleValue(set, property, i, value ? value[i] : 0.0);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propSetIntN(OfxPropertySetHandle properties, const char *property,
                      int count, const int *value) {
  if (count < 0) {
    return kOfxStatErrBadIndex;
  }
  auto *set = asSet(properties);
  for (int i = 0; i < count; ++i) {
    const OfxStatus status = setIntValue(set, property, i, value ? value[i] : 0);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propGetPointer(OfxPropertySetHandle properties, const char *property,
                         int index, void **value) {
  return fillValues(asSet(properties), property, index,
                    [value](const PropertyEntry &entry, size_t idx) {
                      if (!value) {
                        return kOfxStatErrBadHandle;
                      }
                      *value = reinterpret_cast<void *>(
                          entry.values[idx].toULongLong());
                      return kOfxStatOK;
                    });
}

OfxStatus propGetString(OfxPropertySetHandle properties, const char *property,
                        int index, char **value) {
  return fillValues(asSet(properties), property, index,
                    [value](const PropertyEntry &entry, size_t idx) {
                      if (!value) {
                        return kOfxStatErrBadHandle;
                      }
                      if (idx >= entry.stringStorage.size()) {
                        return kOfxStatErrBadIndex;
                      }
                      *value = const_cast<char *>(entry.stringStorage[idx].c_str());
                      return kOfxStatOK;
                    });
}

OfxStatus propGetDouble(OfxPropertySetHandle properties, const char *property,
                        int index, double *value) {
  return fillValues(asSet(properties), property, index,
                    [value](const PropertyEntry &entry, size_t idx) {
                      if (!value) {
                        return kOfxStatErrBadHandle;
                      }
                      *value = entry.values[idx].toDouble();
                      return kOfxStatOK;
                    });
}

OfxStatus propGetInt(OfxPropertySetHandle properties, const char *property,
                     int index, int *value) {
  return fillValues(asSet(properties), property, index,
                    [value](const PropertyEntry &entry, size_t idx) {
                      if (!value) {
                        return kOfxStatErrBadHandle;
                      }
                      *value = entry.values[idx].toInt();
                      return kOfxStatOK;
                    });
}

OfxStatus propGetPointerN(OfxPropertySetHandle properties, const char *property,
                          int count, void **value) {
  if (count < 0 || !value) {
    return kOfxStatErrBadHandle;
  }
  for (int i = 0; i < count; ++i) {
    OfxStatus status = propGetPointer(properties, property, i, &value[i]);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propGetStringN(OfxPropertySetHandle properties, const char *property,
                         int count, char **value) {
  if (count < 0 || !value) {
    return kOfxStatErrBadHandle;
  }
  for (int i = 0; i < count; ++i) {
    OfxStatus status = propGetString(properties, property, i, &value[i]);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propGetDoubleN(OfxPropertySetHandle properties, const char *property,
                         int count, double *value) {
  if (count < 0 || !value) {
    return kOfxStatErrBadHandle;
  }
  for (int i = 0; i < count; ++i) {
    OfxStatus status = propGetDouble(properties, property, i, &value[i]);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propGetIntN(OfxPropertySetHandle properties, const char *property,
                      int count, int *value) {
  if (count < 0 || !value) {
    return kOfxStatErrBadHandle;
  }
  for (int i = 0; i < count; ++i) {
    OfxStatus status = propGetInt(properties, property, i, &value[i]);
    if (status != kOfxStatOK) {
      return status;
    }
  }
  return kOfxStatOK;
}

OfxStatus propReset(OfxPropertySetHandle properties, const char *property) {
  return resetEntry(asSet(properties), property);
}

OfxStatus propGetDimension(OfxPropertySetHandle properties, const char *property,
                           int *count) {
  return getDimension(asSet(properties), property, count);
}

void setStringProperty(PropertySet &set, const char *property, const char *value) {
  setStringValue(&set, property, 0, value);
}

void setIntProperty(PropertySet &set, const char *property, int value) {
  setIntValue(&set, property, 0, value);
}

void setIntPropertyN(PropertySet &set, const char *property, int count,
                     std::initializer_list<int> values) {
  std::vector<int> buffer(values);
  if (count > static_cast<int>(buffer.size())) {
    buffer.resize(static_cast<size_t>(count), 0);
  }
  propSetIntN(reinterpret_cast<OfxPropertySetHandle>(&set), property, count,
              buffer.data());
}

void setDoubleProperty(PropertySet &set, const char *property, double value) {
  setDoubleValue(&set, property, 0, value);
}

void setDoublePropertyN(PropertySet &set, const char *property, int count,
                        std::initializer_list<double> values) {
  std::vector<double> buffer(values);
  if (count > static_cast<int>(buffer.size())) {
    buffer.resize(static_cast<size_t>(count), 0.0);
  }
  propSetDoubleN(reinterpret_cast<OfxPropertySetHandle>(&set), property, count,
                 buffer.data());
}

void setPointerProperty(PropertySet &set, const char *property, void *value) {
  setPointerValue(&set, property, 0, value);
}

ParamSetState *asParamSet(OfxParamSetHandle handle) {
  return reinterpret_cast<ParamSetState *>(handle);
}

ParamState *asParam(OfxParamHandle handle) {
  return reinterpret_cast<ParamState *>(handle);
}

ClipState *asClip(OfxImageClipHandle handle) {
  return reinterpret_cast<ClipState *>(handle);
}

ImageEffectState *asImageEffect(OfxImageEffectHandle handle) {
  return reinterpret_cast<ImageEffectState *>(handle);
}

ImageMemoryState *asMemory(OfxImageMemoryHandle handle) {
  return reinterpret_cast<ImageMemoryState *>(handle);
}

ParamState *ensureParam(ParamSetState *set, const char *name) {
  if (!set || !name) {
    return nullptr;
  }
  auto &entry = set->params[std::string(name)];
  if (!entry) {
    entry = std::make_unique<ParamState>();
    auto &properties = entry->properties;
    setStringProperty(properties, kOfxPropType, kOfxTypeParameter);
    setStringProperty(properties, kOfxPropName, name);
    setStringProperty(properties, kOfxPropLabel, name);
  }
  return entry.get();
}

ClipState *ensureClip(ImageEffectState *effect, const char *name) {
  if (!effect || !name) {
    return nullptr;
  }
  auto &entry = effect->clips[std::string(name)];
  if (!entry) {
    entry = std::make_unique<ClipState>();
    auto &properties = entry->properties;
    setStringProperty(properties, kOfxPropType, kOfxTypeClip);
    setStringProperty(properties, kOfxPropName, name);
    setStringProperty(properties, kOfxPropLabel, name);
  }
  entry->owner = effect;
  entry->clipName = QString::fromUtf8(name);
  return entry.get();
}

void initEffectProperties(PropertySet &properties, const char *name,
                          const char *type) {
  setStringProperty(properties, kOfxPropType, type);
  if (name) {
    setStringProperty(properties, kOfxPropName, name);
    setStringProperty(properties, kOfxPropLabel, name);
  }
}

OfxStatus unsupportedStatus() {
  qWarning() << "[OFX] unsupported effect operation requested; bypassing with warning";
  return kOfxStatErrUnsupported;
}

OfxStatus okStatus() {
  return kOfxStatOK;
}

OfxStatus badHandleStatus() {
  return kOfxStatErrBadHandle;
}

OfxPropertySuiteV1 makePropertySuite() {
  OfxPropertySuiteV1 suite{};
  suite.propSetPointer = &propSetPointer;
  suite.propSetString = &propSetString;
  suite.propSetDouble = &propSetDouble;
  suite.propSetInt = &propSetInt;
  suite.propSetPointerN = &propSetPointerN;
  suite.propSetStringN = &propSetStringN;
  suite.propSetDoubleN = &propSetDoubleN;
  suite.propSetIntN = &propSetIntN;
  suite.propGetPointer = &propGetPointer;
  suite.propGetString = &propGetString;
  suite.propGetDouble = &propGetDouble;
  suite.propGetInt = &propGetInt;
  suite.propGetPointerN = &propGetPointerN;
  suite.propGetStringN = &propGetStringN;
  suite.propGetDoubleN = &propGetDoubleN;
  suite.propGetIntN = &propGetIntN;
  suite.propReset = &propReset;
  suite.propGetDimension = &propGetDimension;
  return suite;
}

const OfxPropertySuiteV1 *propertySuite() {
  static const OfxPropertySuiteV1 suite = makePropertySuite();
  return &suite;
}

OfxStatus effectGetPropertySet(OfxImageEffectHandle imageEffect,
                               OfxPropertySetHandle *propHandle) {
  if (!imageEffect || !propHandle) {
    return kOfxStatErrBadHandle;
  }
  *propHandle = reinterpret_cast<OfxPropertySetHandle>(&asImageEffect(imageEffect)->properties);
  return kOfxStatOK;
}

OfxStatus effectGetParamSet(OfxImageEffectHandle imageEffect,
                            OfxParamSetHandle *paramSet) {
  if (!imageEffect || !paramSet) {
    return kOfxStatErrBadHandle;
  }
  *paramSet = reinterpret_cast<OfxParamSetHandle>(&asImageEffect(imageEffect)->paramSet);
  return kOfxStatOK;
}

OfxStatus effectClipDefine(OfxImageEffectHandle imageEffect, const char *name,
                           OfxPropertySetHandle *propertySet) {
  if (!imageEffect || !name) {
    return kOfxStatErrBadHandle;
  }
  auto *effect = asImageEffect(imageEffect);
  ClipState *clip = ensureClip(effect, name);
  if (!clip) {
    return kOfxStatErrBadHandle;
  }
  if (propertySet) {
    *propertySet = reinterpret_cast<OfxPropertySetHandle>(&clip->properties);
  }
  return kOfxStatOK;
}

OfxStatus effectClipGetHandle(OfxImageEffectHandle imageEffect, const char *name,
                              OfxImageClipHandle *clip,
                              OfxPropertySetHandle *propertySet) {
  if (!imageEffect || !name || !clip) {
    return kOfxStatErrBadHandle;
  }
  auto *effect = asImageEffect(imageEffect);
  ClipState *clipState = ensureClip(effect, name);
  if (!clipState) {
    return kOfxStatErrBadHandle;
  }
  *clip = reinterpret_cast<OfxImageClipHandle>(clipState);
  if (propertySet) {
    *propertySet = reinterpret_cast<OfxPropertySetHandle>(&clipState->properties);
  }
  return kOfxStatOK;
}

OfxStatus effectClipGetPropertySet(OfxImageClipHandle clip,
                                   OfxPropertySetHandle *propHandle) {
  if (!clip || !propHandle) {
    return kOfxStatErrBadHandle;
  }
  *propHandle = reinterpret_cast<OfxPropertySetHandle>(&asClip(clip)->properties);
  return kOfxStatOK;
}

OfxStatus effectClipGetImage(OfxImageClipHandle clip, OfxTime /*time*/,
                             const OfxRectD * /*region*/,
                             OfxPropertySetHandle *imageHandle) {
  if (!clip || !imageHandle) {
    if (imageHandle) *imageHandle = nullptr;
    return kOfxStatFailed;
  }
  auto *effect = imageEffectForClip(asClip(clip));
  if (!effect) {
    *imageHandle = nullptr;
    return kOfxStatFailed;
  }
  auto &rf = effect->renderFrame;

  // The output clip must hand back the destination buffer so the plugin can
  // push its pixels; every other clip hands back the host input. Returning the
  // source buffer for "Output" made every effect a no-op and let plugins
  // scribble over the read-only input.
  auto *clipState = asClip(clip);
  const QByteArray clipNameUtf8 = clipState->clipName.toUtf8();
  const bool isOutput = effect->isOutputClip(clipNameUtf8.constData());
  const unsigned char *pixelData = isOutput ? rf.dstPixelData : rf.srcPixelData;
  const int rowBytes = isOutput ? rf.dstRowBytes : rf.srcRowBytes;
  if (!pixelData) {
    // No outArgs channel exists on clipGetImage, so the diagnostic goes out
    // through the message suite instead of being dropped silently.
    ofxMessageFunc(nullptr, kOfxMessageError, kOfxImageEffectPropStatusMessage,
                   "Artifact OFX host: clip '%s' has no image available",
                   clipNameUtf8.constData());
    *imageHandle = nullptr;
    return kOfxStatFailed;
  }

  // One scratch buffer per clip: a plugin holding the source handle while
  // fetching the output clip no longer sees the source properties replaced.
  auto &imageProps = effect->clipImageScratch[std::string(clipNameUtf8.constData())];
  imageProps.entries.clear();
  setStringProperty(imageProps, kOfxPropType, kOfxTypeImage);
  setPointerProperty(imageProps, kOfxImagePropData,
                     const_cast<unsigned char *>(pixelData));
  setDoubleProperty(imageProps, kOfxImagePropPixelAspectRatio, 1.0);
  setStringProperty(imageProps, kOfxImagePropField, kOfxImageFieldNone);
  setStringProperty(imageProps, kOfxImageEffectPropPixelDepth, kOfxBitDepthFloat);
  setStringProperty(imageProps, kOfxImageEffectPropComponents, kOfxImageComponentRGBA);
  setIntProperty(imageProps, kOfxImagePropRowBytes, rowBytes);
  setIntPropertyN(imageProps, kOfxImagePropBounds, 4,
                  {0, 0, rf.srcWidth, rf.srcHeight});
  setDoublePropertyN(imageProps, kOfxImagePropRegionOfDefinition, 4,
                     {0.0, 0.0, (double)rf.srcWidth, (double)rf.srcHeight});
  setIntProperty(imageProps, kOfxImagePropUniqueIdentifier, 1);
  *imageHandle = reinterpret_cast<OfxPropertySetHandle>(&imageProps);
  return kOfxStatOK;
}

OfxStatus effectClipReleaseImage(OfxPropertySetHandle /*imageHandle*/) {
  return kOfxStatOK;
}

OfxStatus effectClipGetRod(OfxImageClipHandle clip, OfxTime /*time*/,
                           OfxRectD *bounds) {
  if (!bounds) return kOfxStatErrBadHandle;
  auto *clipState = asClip(clip);
  auto *effect = imageEffectForClip(clipState);
  if (!effect) {
    bounds->x1 = 0.0; bounds->y1 = 0.0;
    bounds->x2 = 0.0; bounds->y2 = 0.0;
    return kOfxStatFailed;
  }
  auto &rf = effect->renderFrame;
  const bool isOutput = effect->isOutputClip(clipState->clipName.toUtf8().constData());
  const unsigned char *pixelData = isOutput ? rf.dstPixelData : rf.srcPixelData;
  if (!pixelData) {
    bounds->x1 = 0.0; bounds->y1 = 0.0;
    bounds->x2 = 0.0; bounds->y2 = 0.0;
    return kOfxStatFailed;
  }
  bounds->x1 = 0.0;
  bounds->y1 = 0.0;
  bounds->x2 = (double)rf.srcWidth;
  bounds->y2 = (double)rf.srcHeight;
  return kOfxStatOK;
}

OfxStatus ofxMemoryAlloc(void * /*handle*/, size_t nBytes,
                         void **allocatedData) {
  if (!allocatedData) {
    return kOfxStatErrBadHandle;
  }
  // Aligned like malloc so plugins may treat the block as ordinary memory.
  void *block = std::malloc(nBytes);
  if (!block) {
    *allocatedData = nullptr;
    return kOfxStatErrMemory;
  }
  *allocatedData = block;
  return kOfxStatOK;
}

OfxStatus ofxMemoryFree(void *allocatedData) {
  if (!allocatedData) {
    return kOfxStatErrBadHandle;
  }
  std::free(allocatedData);
  return kOfxStatOK;
}

OfxStatus ofxProgressStart(void * /*effectInstance*/, const char *label) {
  // The host renders on its own schedule and owns no progress UI for plugins,
  // so the request is accepted and the caller's update/end cycle becomes a
  // no-op. Returning Failed here makes many plugins treat progress as an
  // error and abort their work.
  if (label) {
    qInfo("[OFX] progress: %s", label);
  }
  return kOfxStatOK;
}

OfxStatus ofxProgressUpdate(void * /*effectInstance*/, double progress) {
  return progress < 0.0 || progress > 1.0 ? kOfxStatErrValue : kOfxStatOK;
}

OfxStatus ofxProgressEnd(void * /*effectInstance*/) {
  return kOfxStatOK;
}

OfxProgressSuiteV1 makeProgressSuite() {
  OfxProgressSuiteV1 suite{};
  suite.progressStart = &ofxProgressStart;
  suite.progressUpdate = &ofxProgressUpdate;
  suite.progressEnd = &ofxProgressEnd;
  return suite;
}

const OfxProgressSuiteV1 *progressSuite() {
  static const OfxProgressSuiteV1 suite = makeProgressSuite();
  return &suite;
}

OfxStatus ofxProgressStartV2(void *effectInstance, const char *message,
                             const char *messageId) {
  Q_UNUSED(messageId);
  return ofxProgressStart(effectInstance, message);
}

OfxProgressSuiteV2 makeProgressSuiteV2() {
  OfxProgressSuiteV2 suite{};
  suite.progressStart = &ofxProgressStartV2;
  suite.progressUpdate = &ofxProgressUpdate;
  suite.progressEnd = &ofxProgressEnd;
  return suite;
}

const OfxProgressSuiteV2 *progressSuiteV2() {
  static const OfxProgressSuiteV2 suite = makeProgressSuiteV2();
  return &suite;
}

OfxStatus ofxInteractSwapBuffers(OfxInteractHandle /*interactInstance*/) {
  // Custom interact GUIs draw into an OpenGL context the host does not own;
  // the host already declares SupportsCustomInteract = 0, so this is only
  // reached by plugins that ignored that declaration.
  return kOfxStatFailed;
}

OfxStatus ofxInteractRedraw(OfxInteractHandle /*interactInstance*/) {
  return kOfxStatFailed;
}

OfxStatus ofxInteractGetPropertySet(OfxInteractHandle /*interactInstance*/,
                                    OfxPropertySetHandle *property) {
  if (!property) {
    return kOfxStatErrBadHandle;
  }
  *property = nullptr;
  return kOfxStatFailed;
}

OfxInteractSuiteV1 makeInteractSuite() {
  OfxInteractSuiteV1 suite{};
  suite.interactSwapBuffers = &ofxInteractSwapBuffers;
  suite.interactRedraw = &ofxInteractRedraw;
  suite.interactGetPropertySet = &ofxInteractGetPropertySet;
  return suite;
}

const OfxInteractSuiteV1 *interactSuite() {
  static const OfxInteractSuiteV1 suite = makeInteractSuite();
  return &suite;
}

OfxMemorySuiteV1 makeMemorySuite() {
  OfxMemorySuiteV1 suite{};
  suite.memoryAlloc = &ofxMemoryAlloc;
  suite.memoryFree = &ofxMemoryFree;
  return suite;
}

const OfxMemorySuiteV1 *memorySuite() {
  static const OfxMemorySuiteV1 suite = makeMemorySuite();
  return &suite;
}

OfxStatus ofxTimelineGetTime(void *instance, double *time) {
  if (!time) {
    return kOfxStatErrBadHandle;
  }
  auto *effect = static_cast<ImageEffectState *>(instance);
  // The host renders synchronously and owns time; report the frame the effect
  // instance is currently being evaluated at.
  *time = effect ? effect->renderFrame.currentTime : 0.0;
  return kOfxStatOK;
}

OfxStatus ofxTimelineGotoTime(void * /*instance*/, double /*time*/) {
  // The host has no timeline view a plugin can drive; changing time is the
  // host's decision, not the plugin's.
  return kOfxStatFailed;
}

OfxStatus ofxTimelineGetTimeBounds(void * /*instance*/, double *firstTime,
                                   double *lastTime) {
  if (!firstTime || !lastTime) {
    return kOfxStatErrBadHandle;
  }
  *firstTime = 0.0;
  *lastTime = 0.0;
  return kOfxStatFailed;
}

OfxTimeLineSuiteV1 makeTimelineSuite() {
  OfxTimeLineSuiteV1 suite{};
  suite.getTime = &ofxTimelineGetTime;
  suite.gotoTime = &ofxTimelineGotoTime;
  suite.getTimeBounds = &ofxTimelineGetTimeBounds;
  return suite;
}

const OfxTimeLineSuiteV1 *timelineSuite() {
  static const OfxTimeLineSuiteV1 suite = makeTimelineSuite();
  return &suite;
}

OfxStatus effectAbort(OfxImageEffectHandle imageEffect) {
  if (!imageEffect) {
    return 1;
  }
  return asImageEffect(imageEffect)->abortRequested ? 1 : 0;
}

OfxStatus effectImageMemoryAlloc(OfxImageEffectHandle /*instanceHandle*/,
                                 size_t nBytes,
                                 OfxImageMemoryHandle *memoryHandle) {
  if (!memoryHandle) {
    return kOfxStatErrBadHandle;
  }
  auto *memory = new ImageMemoryState();
  memory->bytes.resize(nBytes);
  *memoryHandle = reinterpret_cast<OfxImageMemoryHandle>(memory);
  return kOfxStatOK;
}

OfxStatus effectImageMemoryFree(OfxImageMemoryHandle memoryHandle) {
  if (!memoryHandle) {
    return kOfxStatErrBadHandle;
  }
  delete asMemory(memoryHandle);
  return kOfxStatOK;
}

OfxStatus effectImageMemoryLock(OfxImageMemoryHandle memoryHandle,
                                void **returnedPtr) {
  if (!memoryHandle || !returnedPtr) {
    return kOfxStatErrBadHandle;
  }
  auto *memory = asMemory(memoryHandle);
  ++memory->lockCount;
  *returnedPtr = memory->bytes.empty() ? nullptr : memory->bytes.data();
  return *returnedPtr ? kOfxStatOK : kOfxStatErrMemory;
}

OfxStatus effectImageMemoryUnlock(OfxImageMemoryHandle memoryHandle) {
  if (!memoryHandle) {
    return kOfxStatErrBadHandle;
  }
  auto *memory = asMemory(memoryHandle);
  if (memory->lockCount > 0) {
    --memory->lockCount;
  }
  return kOfxStatOK;
}

OfxImageEffectSuiteV1 makeImageEffectSuite() {
  OfxImageEffectSuiteV1 suite{};
  suite.getPropertySet = &effectGetPropertySet;
  suite.getParamSet = &effectGetParamSet;
  suite.clipDefine = &effectClipDefine;
  suite.clipGetHandle = &effectClipGetHandle;
  suite.clipGetPropertySet = &effectClipGetPropertySet;
  suite.clipGetImage = &effectClipGetImage;
  suite.clipReleaseImage = &effectClipReleaseImage;
  suite.clipGetRegionOfDefinition = &effectClipGetRod;
  suite.abort = &effectAbort;
  suite.imageMemoryAlloc = &effectImageMemoryAlloc;
  suite.imageMemoryFree = &effectImageMemoryFree;
  suite.imageMemoryLock = &effectImageMemoryLock;
  suite.imageMemoryUnlock = &effectImageMemoryUnlock;
  return suite;
}

const OfxImageEffectSuiteV1 *imageEffectSuite() {
  static const OfxImageEffectSuiteV1 suite = makeImageEffectSuite();
  return &suite;
}

OfxStatus paramDefine(OfxParamSetHandle paramSet, const char *paramType,
                      const char *name, OfxPropertySetHandle *propertySet) {
  if (!paramSet || !paramType || !name) {
    return kOfxStatErrBadHandle;
  }
  auto *set = asParamSet(paramSet);
  auto &entry = set->params[std::string(name)];
  if (entry) {
    return kOfxStatErrExists;
  }
  entry = std::make_unique<ParamState>();
  entry->paramType = QString::fromLatin1(paramType);
  entry->currentValues = defaultValuesForOfxParamType(entry->paramType);
  entry->currentStringValue.clear();
  entry->currentUtf8Value.clear();
  set->paramOrder.push_back(std::string(name));
  auto &properties = entry->properties;
  setStringProperty(properties, kOfxPropType, kOfxTypeParameter);
  setStringProperty(properties, kOfxPropName, name);
  setStringProperty(properties, kOfxPropLabel, name);
  setStringProperty(properties, kOfxParamPropType, paramType);
  // The host now implements the keyframe suite on top of AbstractProperty's
  // own track, so parameters advertise themselves as animatable and plugins
  // may rely on it.
  setIntProperty(properties, kOfxParamPropAnimates, 1);
  setIntProperty(properties, kOfxParamPropCanUndo, 1);
  setIntProperty(properties, kOfxParamPropPersistant, 1);
  setIntProperty(properties, kOfxParamPropEvaluateOnChange, 1);
  if (propertySet) {
    *propertySet = reinterpret_cast<OfxPropertySetHandle>(&properties);
  }
  return kOfxStatOK;
}

OfxStatus paramGetHandle(OfxParamSetHandle paramSet, const char *name,
                         OfxParamHandle *param,
                         OfxPropertySetHandle *propertySet) {
  if (!paramSet || !name || !param) {
    return kOfxStatErrBadHandle;
  }
  auto *set = asParamSet(paramSet);
  const auto it = set->params.find(std::string(name));
  if (it == set->params.end() || !it->second) {
    return kOfxStatErrUnknown;
  }
  *param = reinterpret_cast<OfxParamHandle>(it->second.get());
  if (propertySet) {
    *propertySet = reinterpret_cast<OfxPropertySetHandle>(&it->second->properties);
  }
  return kOfxStatOK;
}

OfxStatus paramSetGetPropertySet(OfxParamSetHandle paramSet,
                                 OfxPropertySetHandle *propHandle) {
  if (!paramSet || !propHandle) {
    return kOfxStatErrBadHandle;
  }
  *propHandle = reinterpret_cast<OfxPropertySetHandle>(&asParamSet(paramSet)->properties);
  return kOfxStatOK;
}

OfxStatus paramGetPropertySet(OfxParamHandle param,
                              OfxPropertySetHandle *propHandle) {
  if (!param || !propHandle) {
    return kOfxStatErrBadHandle;
  }
  *propHandle = reinterpret_cast<OfxPropertySetHandle>(&asParam(param)->properties);
  return kOfxStatOK;
}

OfxStatus paramGetValue(OfxParamHandle paramHandle, ...) {
  va_list args;
  va_start(args, paramHandle);
  const OfxStatus status = paramGetValueImpl(paramHandle, args);
  va_end(args);
  return status;
}

OfxStatus paramGetValueAtTime(OfxParamHandle paramHandle, OfxTime time,
                              ...) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }

  // With keys present, answer from the property's own interpolation at the
  // requested time; otherwise fall through to the single current value.
  QVariant evaluated;
  bool haveEvaluated = false;
  if (param->property.hasKeyFrames()) {
    evaluated = param->property.interpolateValue(toRationalTime(*param, time));
    haveEvaluated = true;
  }

  const QString paramType = param->paramType;
  // The variadic list must be started before any va_arg and ended on every
  // exit path; a single result holder keeps the lambda free of returns that
  // could skip va_end.
  OfxStatus result = kOfxStatErrUnsupported;
  va_list args;
  va_start(args, time);
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    const char **out = va_arg(args, const char **);
    if (!out) {
      result = kOfxStatErrBadHandle;
    } else {
      if (haveEvaluated) {
        // The plugin owns the returned buffer only until the next call, so
        // hand back the parameter's stable storage after refreshing it.
        param->currentStringValue = evaluated.toString();
        param->currentUtf8Value = param->currentStringValue.toUtf8().toStdString();
      }
      *out = param->currentUtf8Value.c_str();
      result = kOfxStatOK;
    }
  } else {
    const bool asDouble =
        paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0;
    const bool asInt =
        paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0;
    if (asDouble || asInt) {
      result = kOfxStatOK;
      const int count = paramComponentCount(paramType);
      for (int i = 0; i < count; ++i) {
        if (asDouble) {
          double *out = va_arg(args, double *);
          if (!out) {
            result = kOfxStatErrBadHandle;
            break;
          }
          *out = resolveComponent(*param, evaluated, haveEvaluated, i).toDouble();
        } else {
          int *out = va_arg(args, int *);
          if (!out) {
            result = kOfxStatErrBadHandle;
            break;
          }
          *out = resolveComponent(*param, evaluated, haveEvaluated, i).toInt();
        }
      }
    }
  }
  va_end(args);
  return result;
}

OfxStatus paramGetDerivative(OfxParamHandle paramHandle, OfxTime time, ...) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }
  if (!supportsNumericAnalysis(param->paramType)) {
    // The spec limits derivatives to double and colour params; other types have
    // no defined derivative rather than a zero one.
    return kOfxStatErrUnsupported;
  }

  const int count = paramComponentCount(param->paramType);
  // Central difference over one frame. toRationalTime rounds to whole frames,
  // so any smaller step would collapse onto the same frame and read zero.
  const double rate = param->frameRate > 0.0 ? param->frameRate : 30.0;
  const double h = 1.0 / rate;

  // A parameter with no keys is constant, so its derivative is zero.
  const bool constant = !param->property.hasKeyFrames();

  OfxStatus result = kOfxStatOK;
  va_list args;
  va_start(args, time);
  for (int i = 0; i < count; ++i) {
    double *out = va_arg(args, double *);
    if (!out) {
      result = kOfxStatErrBadHandle;
      break;
    }
    *out = constant ? 0.0
                    : (sampleComponentAt(*param, time + h, i) -
                       sampleComponentAt(*param, time - h, i)) /
                          (2.0 * h);
  }
  va_end(args);
  return result;
}

OfxStatus paramGetIntegral(OfxParamHandle paramHandle, OfxTime time1,
                           OfxTime time2, ...) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }
  if (!supportsNumericAnalysis(param->paramType)) {
    return kOfxStatErrUnsupported;
  }

  const int count = paramComponentCount(param->paramType);
  // Signed integral from time1 to time2, so a reversed range yields the negative
  // of the forward one. The spec does not define the sign convention.
  const double span = time2 - time1;

  OfxStatus result = kOfxStatOK;
  va_list args;
  va_start(args, time2);
  for (int i = 0; i < count; ++i) {
    double *out = va_arg(args, double *);
    if (!out) {
      result = kOfxStatErrBadHandle;
      break;
    }
    if (span == 0.0) {
      *out = 0.0;
      continue;
    }
    if (!param->property.hasKeyFrames()) {
      // Constant curve: the exact integral is value * span.
      *out = sampleComponentAt(*param, time1, i) * span;
      continue;
    }

    // Composite Simpson over the interval, split at every keyframe boundary so
    // that hold and step segments stay exact instead of being smoothed across.
    const auto keys = param->property.getKeyFrames();
    QVector<double> breaks;
    breaks.reserve(static_cast<int>(keys.size()) + 2);
    breaks.push_back(std::min(time1, time2));
    for (const KeyFrame &key : keys) {
      const double keyTime = toOfxTime(*param, key.time);
      if (keyTime > std::min(time1, time2) && keyTime < std::max(time1, time2)) {
        breaks.push_back(keyTime);
      }
    }
    breaks.push_back(std::max(time1, time2));
    std::sort(breaks.begin(), breaks.end());

    double total = 0.0;
    for (int s = 0; s + 1 < breaks.size(); ++s) {
      const double a = breaks.at(s);
      const double b = breaks.at(s + 1);
      const double h = b - a;
      if (h == 0.0) {
        continue;
      }
      // Three-point Simpson is exact for cubics and cheap; a two-point
      // trapezoid is used when the segment is too short to sample a midpoint.
      if (h > 1e-9) {
        const double mid = 0.5 * (a + b);
        total += (h / 6.0) * (sampleComponentAt(*param, a, i) +
                              4.0 * sampleComponentAt(*param, mid, i) +
                              sampleComponentAt(*param, b, i));
      } else {
        total += h * 0.5 * (sampleComponentAt(*param, a, i) +
                            sampleComponentAt(*param, b, i));
      }
    }
    // The interval was normalised to ascending order above; restore the sign.
    *out = span < 0.0 ? -total : total;
  }
  va_end(args);
  return result;
}

OfxStatus paramSetValue(OfxParamHandle paramHandle, ...) {
  va_list args;
  va_start(args, paramHandle);
  const OfxStatus status = paramSetValueImpl(paramHandle, args);
  va_end(args);
  return status;
}

OfxStatus paramSetValueAtTime(OfxParamHandle paramHandle, OfxTime time, ...) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }

  // Read the incoming value positionally into the same currentValues layout
  // the untimed setter uses, then commit it as a keyframe.
  va_list args;
  va_start(args, time);
  const QString paramType = param->paramType;

  QVariant commitValue;
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    const char *value = va_arg(args, const char *);
    commitValue = value ? QString::fromUtf8(value) : QString();
  } else {
    const int count = paramComponentCount(paramType);
    const bool asDouble =
        paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0;
    QVariantList list;
    for (int i = 0; i < count; ++i) {
      if (asDouble) {
        list.push_back(va_arg(args, double));
      } else {
        list.push_back(va_arg(args, int));
      }
    }
    // Preserve the property's own component type (color vs. plain float) so
    // the keyframe written below evaluates back into the right shape.
    if (param->property.getType() == PropertyType::Color &&
        count >= 3) {
      commitValue = QColor::fromRgbF(
          static_cast<float>(list.value(0).toDouble()),
          static_cast<float>(list.value(1).toDouble()),
          static_cast<float>(list.value(2).toDouble()),
          count >= 4 ? static_cast<float>(list.value(3).toDouble()) : 1.0f);
    } else {
      commitValue = QVariant(list);
    }
  }
  va_end(args);

  // Update the stored current value so an immediate paramGetValue still sees
  // what the plugin just wrote.
  const OfxStatus status = [&]() -> OfxStatus {
    if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
        paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
      param->currentStringValue = commitValue.toString();
      param->currentUtf8Value = param->currentStringValue.toUtf8().toStdString();
      param->currentValues = {param->currentStringValue};
      return kOfxStatOK;
    }
    const QVariantList list = commitValue.toList();
    param->currentValues.assign(list.cbegin(), list.cend());
    if (param->currentValues.empty()) {
      param->currentValues = {commitValue};
    }
    return kOfxStatOK;
  }();
  if (status != kOfxStatOK) {
    return status;
  }

  // Write the keyframe only when the parameter is bridged to an animatable
  // property; otherwise the value above is still recorded as the static value.
  if (param->property.isAnimatable()) {
    param->property.addKeyFrame(toRationalTime(*param, time), commitValue);
  }
  return kOfxStatOK;
}

// --- OFX <-> AbstractProperty time conversion -------------------------
//
// OFX times are seconds; AbstractProperty keys are RationalTime at the
// parameter's frame rate. Every keyframe entry point converts through here so
// the two scales cannot drift apart.

RationalTime toRationalTime(const ParamState &param, OfxTime time) {
  const double rate = param.frameRate > 0.0 ? param.frameRate : 30.0;
  const int64_t frames = static_cast<int64_t>(std::llround(time * rate));
  return RationalTime::fromFrameCount(frames, static_cast<int64_t>(rate));
}

OfxTime toOfxTime(const ParamState &param, const RationalTime &time) {
  const double rate = param.frameRate > 0.0 ? param.frameRate : 30.0;
  const int64_t frames = time.rescaledTo(static_cast<int64_t>(rate));
  return static_cast<OfxTime>(static_cast<double>(frames) / rate);
}

QVariant resolveComponent(const ParamState &param, const QVariant &evaluated,
                          bool haveEvaluated, int index) {
  if (haveEvaluated) {
    // Multi-component parameters store their vector as a QVariantList; single
    // component parameters carry the value directly.
    const QVariantList list = evaluated.toList();
    if (index < list.size()) {
      return list.at(index);
    }
    if (list.isEmpty()) {
      return evaluated;
    }
    return QVariant();
  }
  if (index < param.currentValues.size()) {
    return param.currentValues[static_cast<size_t>(index)];
  }
  return QVariant();
}

// Samples one component of the parameter's curve at an arbitrary OFX time.
// Unlike resolveComponent this ignores the stored static value and always reads
// through interpolateValue, so a parameter with no keys correctly reports its
// constant value at every time.
double sampleComponentAt(const ParamState &param, OfxTime time, int index) {
  const QVariant evaluated =
      param.property.interpolateValue(toRationalTime(param, time));
  const QVariantList list = evaluated.toList();
  if (list.isEmpty()) {
    return index == 0 ? evaluated.toDouble() : 0.0;
  }
  return index < list.size() ? list.at(index).toDouble() : 0.0;
}

// True for the parameter types the spec says can carry a derivative or an
// integral (ofxParam.h: "Only double and colour params can have their
// derivatives found" / "can be integrated").
bool supportsNumericAnalysis(const QString &paramType) {
  return paramType.compare(QStringLiteral("OfxParamTypeDouble"), Qt::CaseInsensitive) == 0 ||
         paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
         paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0 ||
         paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
         paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0;
}

OfxStatus paramGetNumKeys(OfxParamHandle paramHandle,
                          unsigned int *numberOfKeys) {
  if (!numberOfKeys) {
    return kOfxStatErrBadHandle;
  }
  auto *param = asParam(paramHandle);
  if (!param) {
    *numberOfKeys = 0;
    return kOfxStatErrUnsupported;
  }
  *numberOfKeys =
      static_cast<unsigned int>(param->property.keyFrameCount());
  return kOfxStatOK;
}

OfxStatus paramGetKeyTime(OfxParamHandle paramHandle, unsigned int nthKey,
                          OfxTime *time) {
  if (!time) {
    return kOfxStatErrBadHandle;
  }
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrUnsupported;
  }
  const auto keys = param->property.getKeyFrames();
  if (nthKey >= keys.size()) {
    return kOfxStatErrValue;
  }
  *time = toOfxTime(*param, keys[nthKey].time);
  return kOfxStatOK;
}

OfxStatus paramGetKeyIndex(OfxParamHandle paramHandle, OfxTime time,
                           int direction, int *index) {
  if (!index) {
    return kOfxStatErrBadHandle;
  }
  auto *param = asParam(paramHandle);
  if (!param) {
    *index = -1;
    return kOfxStatErrUnsupported;
  }
  const auto keys = param->property.getKeyFrames();
  const RationalTime target = toRationalTime(*param, time);

  // direction follows the OFX convention: 0 searches backwards from the key
  // exactly at `time`, 1 searches forwards.
  int found = -1;
  if (direction == 0) {
    for (int i = static_cast<int>(keys.size()) - 1; i >= 0; --i) {
      if (keys[static_cast<size_t>(i)].time <= target) {
        found = i;
        break;
      }
    }
  } else {
    for (size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].time >= target) {
        found = static_cast<int>(i);
        break;
      }
    }
  }
  *index = found;
  return kOfxStatOK;
}

OfxStatus paramDeleteKey(OfxParamHandle paramHandle, OfxTime time) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrUnsupported;
  }
  param->property.removeKeyFrame(toRationalTime(*param, time));
  return kOfxStatOK;
}

OfxStatus paramDeleteAllKeys(OfxParamHandle paramHandle) {
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrUnsupported;
  }
  param->property.clearKeyFrames();
  return kOfxStatOK;
}

OfxStatus paramCopy(OfxParamHandle paramTo, OfxParamHandle paramFrom,
                    OfxTime dstOffset, const OfxRangeD *frameRange) {
  auto *to = asParam(paramTo);
  auto *from = asParam(paramFrom);
  if (!to || !from) {
    return kOfxStatErrBadHandle;
  }
  // The spec requires both parameters to be the same type.
  if (to->paramType.compare(from->paramType, Qt::CaseInsensitive) != 0) {
    return kOfxStatErrBadHandle;
  }

  // "All the previous values in paramTo will be lost": drop the existing track
  // before writing the copied one.
  to->property.clearKeyFrames();

  // frameRange selects a sub-range of keys; the spec uses [0,0] as the
  // sentinel meaning "all animation", so a zero range is not a literal
  // zero-length window.
  const bool copyAllKeys = !frameRange || (frameRange->min == 0.0 && frameRange->max == 0.0);

  const auto keys = from->property.getKeyFrames();
  const double rate = from->frameRate > 0.0 ? from->frameRate : 30.0;
  const double toRate = to->frameRate > 0.0 ? to->frameRate : 30.0;
  // Offset is expressed in OFX seconds; convert it to whole frames so the
  // destination lands on the same time grid the source used.
  const auto offsetFrames = static_cast<int64_t>(std::llround(dstOffset * rate));

  for (const KeyFrame &key : keys) {
    const double keyTime = toOfxTime(*from, key.time);
    if (!copyAllKeys && (keyTime < frameRange->min || keyTime > frameRange->max)) {
      continue;
    }
    const int64_t frames = key.time.value() + offsetFrames;
    // Preserve the curve shape, not just the value: the two-argument
    // addKeyFrame would reset every key to linear.
    to->property.addKeyFrame(
        RationalTime::fromFrameCount(frames, static_cast<int64_t>(toRate)),
        key.value, key.interpolation, key.cp1_x, key.cp1_y, key.cp2_x,
        key.cp2_y, key.roving);
    if (key.anchor != KeyFrame::Anchor::Absolute) {
      to->property.setKeyFrameAnchorAt(
          RationalTime::fromFrameCount(frames, static_cast<int64_t>(toRate)),
          key.anchor);
    }
    if (key.colorLabel != KeyFrame::ColorLabel::None) {
      to->property.setKeyFrameColorLabelAt(
          RationalTime::fromFrameCount(frames, static_cast<int64_t>(toRate)),
          key.colorLabel);
    }
    to->property.setKeyFrameSoftAt(
        RationalTime::fromFrameCount(frames, static_cast<int64_t>(toRate)), key.soft);
  }

  // Carry the static value across so a subsequent paramGetValue on the
  // destination is consistent with the copied animation.
  to->currentValues = from->currentValues;
  to->currentStringValue = from->currentStringValue;
  to->currentUtf8Value = from->currentUtf8Value;
  return kOfxStatOK;
}

PropertyType toPropertyType(const QString &paramType) {
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0) {
    return PropertyType::Integer;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0) {
    return PropertyType::Boolean;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeBytes"), Qt::CaseInsensitive) == 0) {
    return PropertyType::String;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0) {
    return PropertyType::Color;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0) {
    return PropertyType::Float;
  }
  return PropertyType::Float;
}

QVariant defaultValueForOfxParamType(const QString &paramType) {
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0) {
    return 0;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0) {
    return false;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    return QString();
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0) {
    return QColor::fromRgbF(0.0, 0.0, 0.0, 1.0);
  }
  return 0.0;
}

int paramComponentCount(const QString &paramType) {
  if (paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0) {
    return 4;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0) {
    return 3;
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0) {
    return 2;
  }
  return 1;
}

std::vector<QVariant> defaultValuesForOfxParamType(const QString &paramType) {
  if (paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0) {
    return {0.0, 0.0, 0.0, 1.0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0) {
    return {0.0, 0.0, 0.0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0) {
    return {0.0, 0.0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0) {
    return {0, 0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0) {
    return {0.0, 0.0, 0.0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0) {
    return {0, 0, 0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0) {
    return {0};
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    return {QString()};
  }
  return {0.0};
}

OfxStatus paramGetValueImpl(OfxParamHandle paramHandle, va_list args) {
  if (!paramHandle) {
    return kOfxStatErrBadHandle;
  }
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }

  const QString paramType = param->paramType;
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    const char **out = va_arg(args, const char **);
    if (!out) {
      return kOfxStatErrBadHandle;
    }
    *out = param->currentUtf8Value.c_str();
    return kOfxStatOK;
  }

  const auto writeInt = [&](int count) -> OfxStatus {
    for (int i = 0; i < count; ++i) {
      int *out = va_arg(args, int *);
      if (!out) {
        return kOfxStatErrBadHandle;
      }
      *out = i < static_cast<int>(param->currentValues.size())
                 ? param->currentValues[static_cast<size_t>(i)].toInt()
                 : 0;
    }
    return kOfxStatOK;
  };

  const auto writeDouble = [&](int count) -> OfxStatus {
    for (int i = 0; i < count; ++i) {
      double *out = va_arg(args, double *);
      if (!out) {
        return kOfxStatErrBadHandle;
      }
      *out = i < static_cast<int>(param->currentValues.size())
                 ? param->currentValues[static_cast<size_t>(i)].toDouble()
                 : 0.0;
    }
    return kOfxStatOK;
  };

  if (paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0) {
    return writeInt(1);
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0) {
    return writeInt(paramComponentCount(paramType));
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0) {
    return writeDouble(paramComponentCount(paramType));
  }

  return kOfxStatErrUnsupported;
}

OfxStatus paramSetValueImpl(OfxParamHandle paramHandle, va_list args) {
  if (!paramHandle) {
    return kOfxStatErrBadHandle;
  }
  auto *param = asParam(paramHandle);
  if (!param) {
    return kOfxStatErrBadHandle;
  }

  const QString paramType = param->paramType;
  if (paramType.compare(QStringLiteral("OfxParamTypeString"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeCustom"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeStrChoice"), Qt::CaseInsensitive) == 0) {
    const char *value = va_arg(args, const char *);
    param->currentStringValue = value ? QString::fromUtf8(value) : QString();
    param->currentUtf8Value = param->currentStringValue.toUtf8().toStdString();
    param->currentValues = {param->currentStringValue};
    return kOfxStatOK;
  }

  const auto readInt = [&](int count) -> OfxStatus {
    std::vector<QVariant> values;
    values.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      values.push_back(va_arg(args, int));
    }
    param->currentValues = std::move(values);
    return kOfxStatOK;
  };

  const auto readDouble = [&](int count) -> OfxStatus {
    std::vector<QVariant> values;
    values.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      values.push_back(va_arg(args, double));
    }
    param->currentValues = std::move(values);
    return kOfxStatOK;
  };

  if (paramType.compare(QStringLiteral("OfxParamTypeBoolean"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeChoice"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger"), Qt::CaseInsensitive) == 0) {
    return readInt(1);
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeInteger2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeInteger3D"), Qt::CaseInsensitive) == 0) {
    return readInt(paramComponentCount(paramType));
  }
  if (paramType.compare(QStringLiteral("OfxParamTypeRGB"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeRGBA"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble2D"), Qt::CaseInsensitive) == 0 ||
      paramType.compare(QStringLiteral("OfxParamTypeDouble3D"), Qt::CaseInsensitive) == 0) {
    return readDouble(paramComponentCount(paramType));
  }

  return kOfxStatErrUnsupported;
}

QString readStringProperty(const PropertySet &set, const char *property) {
  const auto it = set.entries.find(std::string(property));
  if (it == set.entries.end() || it->second.values.empty()) {
    return {};
  }
  return it->second.values[0].toString();
}

bool readBoolProperty(const PropertySet &set, const char *property, bool fallback = false) {
  const auto it = set.entries.find(std::string(property));
  if (it == set.entries.end() || it->second.values.empty()) {
    return fallback;
  }
  return it->second.values[0].toInt() != 0;
}

bool isContainerParamType(const QString &paramType) {
  return paramType.compare(QStringLiteral("OfxParamTypeGroup"), Qt::CaseInsensitive) == 0 ||
         paramType.compare(QStringLiteral("OfxParamTypePage"), Qt::CaseInsensitive) == 0;
}

AbstractProperty toAbstractProperty(const ParamState &state, const QString &name) {
  AbstractProperty prop;
  const QVariant defaultValue = defaultValueForOfxParamType(state.paramType);
  const QString parentPath = readStringProperty(state.properties, kOfxParamPropParent);
  const QString scriptName = readStringProperty(state.properties, kOfxParamPropScriptName);
  const QString displayLabel = readStringProperty(state.properties, kOfxPropLabel);
  const QString hint = readStringProperty(state.properties, kOfxParamPropHint);
  const QString fullName = parentPath.isEmpty() ? name : QStringLiteral("%1/%2").arg(parentPath, name);

  prop.setName(fullName);
  prop.setDisplayLabel(displayLabel.isEmpty() ? (scriptName.isEmpty() ? name : scriptName)
                                              : displayLabel);
  prop.setType(toPropertyType(state.paramType));
  prop.setDefaultValue(defaultValue);
  if (prop.getType() == PropertyType::Color) {
    prop.setColorValue(defaultValue.value<QColor>());
  } else {
    prop.setValue(defaultValue);
  }
  // A single assignment: the previous pair set this to true and then
  // immediately overwrote it with the host property, so the first call was
  // dead code.
  prop.setAnimatable(readBoolProperty(state.properties, kOfxParamPropAnimates, false));
  if (!hint.isEmpty()) {
    prop.setTooltip(hint);
  }
  if (const auto it = state.properties.entries.find(std::string(kOfxParamPropType));
      it != state.properties.entries.end() && !it->second.values.empty()) {
    const QString paramType = it->second.values[0].toString();
    prop.setType(toPropertyType(paramType));
    const QVariant typedDefaultValue = defaultValueForOfxParamType(paramType);
    prop.setDefaultValue(typedDefaultValue);
    if (prop.getType() == PropertyType::Color) {
      prop.setColorValue(typedDefaultValue.value<QColor>());
    } else {
      prop.setValue(typedDefaultValue);
    }
  }

  // The plugin's own default overrides the type-derived zero. Multi-component
  // parameters store theirs as a bundle of components.
  {
    const auto it = state.properties.entries.find(std::string(kOfxParamPropDefault));
    if (it != state.properties.entries.end() && !it->second.values.empty()) {
      const int components = paramComponentCount(state.paramType);
      QVariantList parts;
      for (const QVariant &value : it->second.values) {
        parts.push_back(value);
      }
      if (prop.getType() == PropertyType::Color && parts.size() >= 3) {
        prop.setDefaultValue(QColor::fromRgbF(
            static_cast<float>(parts.value(0).toDouble()),
            static_cast<float>(parts.value(1).toDouble()),
            static_cast<float>(parts.value(2).toDouble()),
            parts.size() >= 4 ? static_cast<float>(parts.value(3).toDouble()) : 1.0f));
        prop.setColorValue(prop.getDefaultValue().value<QColor>());
      } else if (components > 1 && parts.size() >= components) {
        prop.setDefaultValue(parts);
        prop.setValue(parts);
      } else if (!parts.isEmpty()) {
        prop.setDefaultValue(parts.first());
        prop.setValue(parts.first());
      }
    }
  }

  // Min / Max / DisplayMin / DisplayMax / Increment: the plugin defines the
  // control's range and the editor was ignoring all of it, so every OFX
  // slider ran with the generic fallback range.
  {
    const auto readDoubleEntry = [&](const char *key, int index, double fallback) {
      const auto it = state.properties.entries.find(std::string(key));
      if (it == state.properties.entries.end() || it->second.values.empty()) {
        return fallback;
      }
      if (index < it->second.values.size()) {
        return it->second.values[static_cast<size_t>(index)].toDouble();
      }
      return fallback;
    };
    const auto hasEntry = [&](const char *key) {
      const auto it = state.properties.entries.find(std::string(key));
      return it != state.properties.entries.end() && !it->second.values.empty();
    };

    double minValue = 0.0;
    double maxValue = 0.0;
    const bool hasMin = hasEntry(kOfxParamPropMin);
    const bool hasMax = hasEntry(kOfxParamPropMax);
    if (hasMin) {
      minValue = readDoubleEntry(kOfxParamPropMin, 0, 0.0);
    }
    if (hasMax) {
      maxValue = readDoubleEntry(kOfxParamPropMax, 0, 0.0);
    }
    if (hasMin && hasMax && maxValue > minValue) {
      prop.setHardRange(minValue, maxValue);
    } else if (hasMin) {
      prop.setMinValue(minValue);
    } else if (hasMax) {
      prop.setMaxValue(maxValue);
    }

    if (hasEntry(kOfxParamPropDisplayMin) && hasEntry(kOfxParamPropDisplayMax)) {
      const double displayMin = readDoubleEntry(kOfxParamPropDisplayMin, 0, minValue);
      const double displayMax = readDoubleEntry(kOfxParamPropDisplayMax, 0, maxValue);
      if (displayMax > displayMin) {
        prop.setSoftRange(displayMin, displayMax);
      }
    }
    if (hasEntry(kOfxParamPropIncrement)) {
      const double increment = readDoubleEntry(kOfxParamPropIncrement, 0, 0.0);
      if (increment > 0.0) {
        prop.setStep(increment);
      }
    }
  }

  return prop;
}

OfxStatus paramEditBegin(OfxParamSetHandle /*paramSet*/, const char * /*name*/) {
  return kOfxStatOK;
}

OfxStatus paramEditEnd(OfxParamSetHandle /*paramSet*/) {
  return kOfxStatOK;
}

OfxParameterSuiteV1 makeParameterSuite() {
  OfxParameterSuiteV1 suite{};
  suite.paramDefine = &paramDefine;
  suite.paramGetHandle = &paramGetHandle;
  suite.paramSetGetPropertySet = &paramSetGetPropertySet;
  suite.paramGetPropertySet = &paramGetPropertySet;
  suite.paramGetValue = &paramGetValue;
  suite.paramGetValueAtTime = &paramGetValueAtTime;
  suite.paramGetDerivative = &paramGetDerivative;
  suite.paramGetIntegral = &paramGetIntegral;
  suite.paramSetValue = &paramSetValue;
  suite.paramSetValueAtTime = &paramSetValueAtTime;
  suite.paramGetNumKeys = &paramGetNumKeys;
  suite.paramGetKeyTime = &paramGetKeyTime;
  suite.paramGetKeyIndex = &paramGetKeyIndex;
  suite.paramDeleteKey = &paramDeleteKey;
  suite.paramDeleteAllKeys = &paramDeleteAllKeys;
  suite.paramCopy = &paramCopy;
  suite.paramEditBegin = &paramEditBegin;
  suite.paramEditEnd = &paramEditEnd;
  return suite;
}

const OfxParameterSuiteV1 *parameterSuite() {
  static const OfxParameterSuiteV1 suite = makeParameterSuite();
  return &suite;
}

SharedPtr<ImageEffectState> makeDescriptorState(const OfxPlugin &plugin,
                                                       const QString &bundlePath) {
  auto state = ArtifactCore::makeShared<ImageEffectState>();
  initEffectProperties(state->properties, plugin.pluginIdentifier, kOfxTypeImageEffect);
  setStringProperty(state->properties, kOfxPluginPropFilePath, bundlePath.toUtf8().constData());
  setPointerProperty(state->properties, kOfxImageEffectPropPluginHandle, state.get());
  return state;
}

QStringList readSupportedContexts(const ImageEffectState &descriptorState) {
  QStringList contexts;
  const auto *entries = &descriptorState.properties.entries;
  const auto it = entries->find(std::string(kOfxImageEffectPropSupportedContexts));
  if (it == entries->end()) {
    return contexts;
  }
  const int count = static_cast<int>(it->second.values.size());
  for (int i = 0; i < count; ++i) {
    const QString context = it->second.values[static_cast<size_t>(i)].toString();
    if (!context.isEmpty()) {
      contexts.push_back(context);
    }
  }
  return contexts;
}

bool describePlugin(OfxPlugin *plugin, const QString &bundlePath,
                    OfxPluginDescriptor &descriptor) {
  if (!plugin || !plugin->mainEntry) {
    return false;
  }

  // Skip a plugin that crashed enough times to be blacklisted. Doing it here
  // means a known-bad plugin is never described, never instantiated, and never
  // reaches the effect catalog that the UI reads.
  if (plugin->pluginIdentifier &&
      ofxPluginIsBlacklisted(plugin->pluginIdentifier)) {
    qWarning("[OFX] skipping blacklisted plugin '%s'",
             plugin->pluginIdentifier);
    return false;
  }

  const OfxStatus loadStatus = pluginActionLoad(plugin);
  if (loadStatus != kOfxStatOK && loadStatus != kOfxStatReplyDefault) {
    return false;
  }

  if (!descriptor.descriptorState) {
    descriptor.descriptorState = makeDescriptorState(*plugin, bundlePath);
  }

  const OfxStatus describeStatus = pluginActionDescribe(plugin, *descriptor.descriptorState);
  if (describeStatus != kOfxStatOK && describeStatus != kOfxStatReplyDefault) {
    return false;
  }

  QStringList contexts = readSupportedContexts(*descriptor.descriptorState);
  if (contexts.isEmpty()) {
    contexts << QString::fromLatin1(kOfxImageEffectContextGeneral);
  }

  bool acceptedAnyContext = false;
  QStringList acceptedContexts;
  for (const QString &context : contexts) {
    PropertySet contextArgs;
    setStringProperty(contextArgs, kOfxImageEffectPropContext,
                      context.toUtf8().constData());
    const OfxStatus contextStatus = dispatchOfxAction(
        plugin, plugin->pluginIdentifier,
        kOfxImageEffectActionDescribeInContext,
        descriptor.descriptorState.get(),
        reinterpret_cast<OfxPropertySetHandle>(&contextArgs), nullptr);
    if (contextStatus == kOfxStatOK || contextStatus == kOfxStatReplyDefault) {
      acceptedAnyContext = true;
      acceptedContexts.push_back(context);
    }
  }

  if (!acceptedAnyContext) {
    return false;
  }

  descriptor.supportedContexts = acceptedContexts;
  descriptor.previewProperties.clear();
  for (const auto &paramName : descriptor.descriptorState->paramSet.paramOrder) {
    const auto it = descriptor.descriptorState->paramSet.params.find(paramName);
    if (it == descriptor.descriptorState->paramSet.params.end() || !it->second) {
      continue;
    }
    const QString paramType = it->second->paramType;
    if (isContainerParamType(paramType) ||
        readBoolProperty(it->second->properties, kOfxParamPropSecret, false)) {
      continue;
    }
    descriptor.previewProperties.push_back(
        toAbstractProperty(*it->second, QString::fromStdString(paramName)));
    // The parameter keeps its own copy of the bridged property. A pointer into
    // previewProperties would dangle as soon as the vector reallocates, and the
    // keyframe suite needs a stable object; the bridge keeps the two in sync on
    // every user edit (see ArtifactOfxEffect::setPropertyValue).
    it->second->property = descriptor.previewProperties.back();
  }
  return true;
}

bool isPluginBinaryFile(const QFileInfo &info) {
  if (!info.isFile()) {
    return false;
  }

  const QString suffix = info.suffix().toLower();
#ifdef _WIN32
  return suffix == QStringLiteral("dll") || suffix == QStringLiteral("ofx");
#else
  return suffix == QStringLiteral("so") || suffix == QStringLiteral("dylib") ||
         suffix == QStringLiteral("ofx");
#endif
}

bool isOfxBundleDirectory(const QFileInfo &info) {
  if (!info.isDir()) {
    return false;
  }

  const QString name = info.fileName().toLower();
  return name.endsWith(QStringLiteral(".ofx")) ||
         name.endsWith(QStringLiteral(".ofx.bundle"));
}

QString bundleDisplayPath(const QString &bundlePath, const QString &binaryPath) {
  return bundlePath.isEmpty() ? binaryPath : bundlePath;
}

QString stripBundleSuffix(QString name) {
  const QString lower = name.toLower();
  if (lower.endsWith(QStringLiteral(".ofx.bundle"))) {
    name.chop(QStringLiteral(".ofx.bundle").size());
  } else if (lower.endsWith(QStringLiteral(".bundle"))) {
    name.chop(QStringLiteral(".bundle").size());
  } else if (lower.endsWith(QStringLiteral(".ofx"))) {
    name.chop(QStringLiteral(".ofx").size());
  }
  return name;
}

} // namespace

export OfxStatus pluginActionCreateInstance(OfxPlugin *plugin, ImageEffectState &instanceState) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  return dispatchOfxAction(
      plugin, plugin->pluginIdentifier, kOfxActionCreateInstance,
      &instanceState, nullptr, nullptr);
}

export OfxStatus pluginActionDestroyInstance(OfxPlugin *plugin, ImageEffectState &instanceState) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  return dispatchOfxAction(
      plugin, plugin->pluginIdentifier, kOfxActionDestroyInstance,
      &instanceState, nullptr, nullptr);
}

export OfxStatus pluginActionBeginSequenceRender(OfxPlugin *plugin, ImageEffectState &instanceState,
                                           const OfxPointD &renderScale) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  PropertySet inArgs;
  setDoublePropertyN(inArgs, kOfxImageEffectPropRenderScale, 2, {renderScale.x, renderScale.y});
  return dispatchOfxAction(
      plugin, plugin->pluginIdentifier,
      kOfxImageEffectActionBeginSequenceRender, &instanceState,
      reinterpret_cast<OfxPropertySetHandle>(&inArgs), nullptr);
}

export OfxStatus pluginActionRender(OfxPlugin *plugin, ImageEffectState &instanceState,
                              OfxTime time, const OfxPointD &renderScale,
                              QString *failureMessage = nullptr) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  PropertySet inArgs;
  setDoubleProperty(inArgs, kOfxPropTime, time);
  setDoublePropertyN(inArgs, kOfxImageEffectPropRenderScale, 2, {renderScale.x, renderScale.y});
  setStringProperty(inArgs, kOfxImageEffectPropFieldToRender, kOfxImageFieldNone);
  // The render action may write a status message describing why it produced no
  // result; the spec requires the host to surface it rather than swallow it.
  PropertySet outArgs;
  const OfxStatus status = dispatchOfxAction(
      plugin, plugin->pluginIdentifier, kOfxImageEffectActionRender,
      &instanceState, reinterpret_cast<OfxPropertySetHandle>(&inArgs),
      reinterpret_cast<OfxPropertySetHandle>(&outArgs));
  if (failureMessage) {
    *failureMessage = readStringProperty(outArgs, kOfxImageEffectPropStatusMessage);
  }
  return status;
}

export OfxStatus pluginActionEndSequenceRender(OfxPlugin *plugin, ImageEffectState &instanceState,
                                         const OfxPointD &renderScale) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  PropertySet inArgs;
  setDoublePropertyN(inArgs, kOfxImageEffectPropRenderScale, 2, {renderScale.x, renderScale.y});
  return dispatchOfxAction(
      plugin, plugin->pluginIdentifier,
      kOfxImageEffectActionEndSequenceRender, &instanceState,
      reinterpret_cast<OfxPropertySetHandle>(&inArgs), nullptr);
}

// Depth/component negotiation. The host works in 32-bit float RGBA, so it
// declares exactly what it can deliver and lets the plugin pick from that.
// Returning kOfxStatReplyDefault would leave the clip properties unset, which
// makes plugins fall back to assumptions about depth and pixel layout.
export OfxStatus pluginActionGetClipPreferences(OfxPlugin *plugin,
                                                ImageEffectState &instanceState) {
  if (!plugin || !plugin->mainEntry) return kOfxStatErrBadHandle;
  PropertySet inArgs;
  PropertySet outArgs;
  const OfxStatus status = dispatchOfxAction(
      plugin, plugin->pluginIdentifier,
      kOfxImageEffectActionGetClipPreferences, &instanceState,
      reinterpret_cast<OfxPropertySetHandle>(&inArgs),
      reinterpret_cast<OfxPropertySetHandle>(&outArgs));

  // The host's own capability, written to every clip regardless of what the
  // plugin asked for. Per the spec these keys are the literal property names
  // with the clip name appended ("OfxImageClipPropComponents_<clipName>").
  for (const auto &kv : instanceState.clips) {
    PropertySet &target = kv.second->properties;
    const QByteArray suffix = kv.second->clipName.toUtf8();
    setStringProperty(target,
                      (QStringLiteral("OfxImageClipPropComponents_") +
                       QString::fromUtf8(suffix)).toUtf8().constData(),
                      kOfxImageComponentRGBA);
    setStringProperty(target,
                      (QStringLiteral("OfxImageClipPropDepth_") +
                       QString::fromUtf8(suffix)).toUtf8().constData(),
                      kOfxBitDepthFloat);
    setDoubleProperty(target,
                      (QStringLiteral("OfxImageClipPropPAR_") +
                       QString::fromUtf8(suffix)).toUtf8().constData(),
                      1.0);
  }
  setDoubleProperty(outArgs, kOfxImageEffectPropFrameRate, 30.0);
  setStringProperty(outArgs, kOfxImageClipPropFieldOrder, kOfxImageFieldNone);
  setStringProperty(outArgs, kOfxImageEffectPropPreMultiplication, kOfxImageOpaque);
  setIntProperty(outArgs, kOfxImageClipPropContinuousSamples, 0);
  setIntProperty(outArgs, kOfxImageEffectFrameVarying, 0);
  return status;
}

// Lets a plugin report that it can pass its input through untouched, which the
// host then uses to skip the render call entirely.
export bool pluginActionIsIdentity(OfxPlugin *plugin,
                                   ImageEffectState &instanceState,
                                   OfxTime time,
                                   const OfxPointD &renderScale) {
  if (!plugin || !plugin->mainEntry) return false;
  PropertySet inArgs;
  setDoubleProperty(inArgs, kOfxPropTime, time);
  setDoublePropertyN(inArgs, kOfxImageEffectPropRenderScale, 2, {renderScale.x, renderScale.y});
  setStringProperty(inArgs, kOfxImageEffectPropFieldToRender, kOfxImageFieldNone);
  PropertySet outArgs;
  const OfxStatus status = dispatchOfxAction(
      plugin, plugin->pluginIdentifier, kOfxImageEffectActionIsIdentity,
      &instanceState, reinterpret_cast<OfxPropertySetHandle>(&inArgs),
      reinterpret_cast<OfxPropertySetHandle>(&outArgs));
  if (status != kOfxStatOK) {
    return false;
  }
  const QString identityClip =
      readStringProperty(outArgs, kOfxPropName);
  return !identityClip.isEmpty();
}

OfxMessageSuiteV1 makeMessageSuite() {
  OfxMessageSuiteV1 suite{};
  suite.message = &ofxMessageFunc;
  return suite;
}

const OfxMessageSuiteV1 *messageSuite() {
  static const OfxMessageSuiteV1 suite = makeMessageSuite();
  return &suite;
}

export class ArtifactOfxHost {
public:
  static ArtifactOfxHost &instance() {
    static ArtifactOfxHost s_instance;
    return s_instance;
  }

  // Dispatches an OFX action through the crash guard and records any fault
  // against the plugin's identifier. Every plugin entry point call must go
  // through here; calling mainEntry directly would let a faulty plugin take
  // the whole application down.
  //
  // `identifier` may be null for actions issued before the plugin is fully
  // described, in which case the fault is counted but not attributed.
  //
  // The body is in-class; the free `dispatchOfxAction` wrapper below exists for
  // the action helpers that are declared before the class body is complete.
  OfxStatus dispatchAction(OfxPlugin *plugin, const char *identifier,
                           const char *action, const void *instance,
                           OfxPropertySetHandle inArgs,
                           OfxPropertySetHandle outArgs) {
    OfxStatus status = kOfxStatFailed;
    // Publish the caller for the duration of the call so an abort raised by the
    // plugin can be attributed even though __try/__except will not see it.
    const char *previous = activePluginIdentifier();
    activePluginIdentifier() = identifier;
    const bool completed = callPluginEntryPoint(
        plugin ? plugin->mainEntry : nullptr, action, instance, inArgs, outArgs,
        &status);
    activePluginIdentifier() = previous;
    if (!completed) {
      recordPluginFault(identifier);
    }
    return status;
  }

  inline const std::vector<OfxPluginDescriptor> &getLoadedPlugins() const {
    return plugins_;
  }

  // True when the plugin has faulted often enough to be taken out of service.
  // Consulted before describing a plugin so a known-bad one is never loaded
  // into the effect catalog.
  bool isBlacklisted(const char *identifier) const {
    if (!identifier) {
      return false;
    }
    loadBlacklist();
    return blacklistedIdentifiers_.contains(QString::fromLatin1(identifier));
  }

  // Identifiers removed from service by the crash counter. Not cleared by a
  // rescan; use clearBlacklist() for an explicit user-driven reset.
  QStringList blacklistedPlugins() const {
    loadBlacklist();
    QStringList result = blacklistedIdentifiers_.values();
    result.sort();
    return result;
  }

  void clearBlacklist() {
    loadBlacklist();
    blacklistedIdentifiers_.clear();
    crashCounts_.clear();
    persistBlacklist();
  }

  void initialize() {
    if (initialized_) {
      return;
    }
    initialized_ = true;
    installOfxAbortReporter();

    hostDescriptor_.entries.clear();
    hostDescriptor_.entries[kOfxPropType].kind = PropertyKind::String;
    hostDescriptor_.entries[kOfxPropType].values = {
        QString::fromLatin1(kOfxTypeImageEffectHost)};
    hostDescriptor_.entries[kOfxPropType].defaults =
        hostDescriptor_.entries[kOfxPropType].values;
    hostDescriptor_.entries[kOfxPropType].stringStorage = {
        kOfxTypeImageEffectHost};

    hostDescriptor_.entries[kOfxPropName].kind = PropertyKind::String;
    hostDescriptor_.entries[kOfxPropName].values = {
        QStringLiteral("ArtifactStudio OFX Host")};
    hostDescriptor_.entries[kOfxPropName].defaults =
        hostDescriptor_.entries[kOfxPropName].values;
    hostDescriptor_.entries[kOfxPropName].stringStorage = {
        "ArtifactStudio OFX Host"};

    hostDescriptor_.entries[kOfxPropLabel].kind = PropertyKind::String;
    hostDescriptor_.entries[kOfxPropLabel].values = {
        QStringLiteral("ArtifactStudio OFX Host")};
    hostDescriptor_.entries[kOfxPropLabel].defaults =
        hostDescriptor_.entries[kOfxPropLabel].values;
    hostDescriptor_.entries[kOfxPropLabel].stringStorage = {
        "ArtifactStudio OFX Host"};

    hostDescriptor_.entries[kOfxPropVersion].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxPropVersion].values = {0, 9, 0, 0};
    hostDescriptor_.entries[kOfxPropVersion].defaults =
        hostDescriptor_.entries[kOfxPropVersion].values;

    hostDescriptor_.entries[kOfxPropVersionLabel].kind = PropertyKind::String;
    hostDescriptor_.entries[kOfxPropVersionLabel].values = {
        QStringLiteral("0.9.0")};
    hostDescriptor_.entries[kOfxPropVersionLabel].defaults =
        hostDescriptor_.entries[kOfxPropVersionLabel].values;
    hostDescriptor_.entries[kOfxPropVersionLabel].stringStorage = {"0.9.0"};

    hostDescriptor_.entries[kOfxPropAPIVersion].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxPropAPIVersion].values = {1, 4};
    hostDescriptor_.entries[kOfxPropAPIVersion].defaults =
        hostDescriptor_.entries[kOfxPropAPIVersion].values;

    hostDescriptor_.entries[kOfxImageEffectHostPropIsBackground].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxImageEffectHostPropIsBackground].values = {0};
    hostDescriptor_.entries[kOfxImageEffectHostPropIsBackground].defaults =
        hostDescriptor_.entries[kOfxImageEffectHostPropIsBackground].values;

    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipDepths].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipDepths].values = {1};
    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipDepths].defaults =
        hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipDepths].values;

    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipPARs].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipPARs].values = {1};
    hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipPARs].defaults =
        hostDescriptor_.entries[kOfxImageEffectPropSupportsMultipleClipPARs].values;

    hostDescriptor_.entries[kOfxImageEffectPropSetableFrameRate].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxImageEffectPropSetableFrameRate].values = {1};
    hostDescriptor_.entries[kOfxImageEffectPropSetableFrameRate].defaults =
        hostDescriptor_.entries[kOfxImageEffectPropSetableFrameRate].values;

    hostDescriptor_.entries[kOfxImageEffectPropSetableFielding].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxImageEffectPropSetableFielding].values = {1};
    hostDescriptor_.entries[kOfxImageEffectPropSetableFielding].defaults =
        hostDescriptor_.entries[kOfxImageEffectPropSetableFielding].values;

    // The keyframe suite is now implemented on top of AbstractProperty's own
    // track, so the host can advertise real animation support and plugins will
    // build animated controls that actually evaluate.
    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomAnimation].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomAnimation].values = {1};
    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomAnimation].defaults =
        hostDescriptor_.entries[kOfxParamHostPropSupportsCustomAnimation].values;

    hostDescriptor_.entries[kOfxParamHostPropSupportsStringAnimation].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropSupportsStringAnimation].values = {1};
    hostDescriptor_.entries[kOfxParamHostPropSupportsStringAnimation].defaults =
        hostDescriptor_.entries[kOfxParamHostPropSupportsStringAnimation].values;

    hostDescriptor_.entries[kOfxParamHostPropSupportsBooleanAnimation].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropSupportsBooleanAnimation].values = {1};
    hostDescriptor_.entries[kOfxParamHostPropSupportsBooleanAnimation].defaults =
        hostDescriptor_.entries[kOfxParamHostPropSupportsBooleanAnimation].values;

    hostDescriptor_.entries[kOfxParamHostPropSupportsChoiceAnimation].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropSupportsChoiceAnimation].values = {1};
    hostDescriptor_.entries[kOfxParamHostPropSupportsChoiceAnimation].defaults =
        hostDescriptor_.entries[kOfxParamHostPropSupportsChoiceAnimation].values;

    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomInteract].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomInteract].values = {0};
    hostDescriptor_.entries[kOfxParamHostPropSupportsCustomInteract].defaults =
        hostDescriptor_.entries[kOfxParamHostPropSupportsCustomInteract].values;

    hostDescriptor_.entries[kOfxParamHostPropMaxParameters].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropMaxParameters].values = {-1};
    hostDescriptor_.entries[kOfxParamHostPropMaxParameters].defaults =
        hostDescriptor_.entries[kOfxParamHostPropMaxParameters].values;

    hostDescriptor_.entries[kOfxParamHostPropMaxPages].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropMaxPages].values = {-1};
    hostDescriptor_.entries[kOfxParamHostPropMaxPages].defaults =
        hostDescriptor_.entries[kOfxParamHostPropMaxPages].values;

    hostDescriptor_.entries[kOfxParamHostPropPageRowColumnCount].kind = PropertyKind::Int;
    hostDescriptor_.entries[kOfxParamHostPropPageRowColumnCount].values = {4, 8};
    hostDescriptor_.entries[kOfxParamHostPropPageRowColumnCount].defaults =
        hostDescriptor_.entries[kOfxParamHostPropPageRowColumnCount].values;

    hostStruct_.host = reinterpret_cast<OfxPropertySetHandle>(&hostDescriptor_);
    hostStruct_.fetchSuite = &ArtifactOfxHost::fetchSuiteCallback;

    clearLoadedPlugins();

    QStringList roots;
    const QString appDir = QCoreApplication::applicationDirPath();
    roots << QDir(appDir).filePath(QStringLiteral("plugins/ofx"))
          << QDir(appDir).filePath(QStringLiteral("plugins"));

    const QString envPaths = QString::fromLocal8Bit(qgetenv("OFX_PLUGIN_PATH"));
    if (!envPaths.trimmed().isEmpty()) {
      roots.append(envPaths.split(QDir::listSeparator(), Qt::SkipEmptyParts));
    }

    scanRoots(roots);
  }

  void rescan() {
    initialized_ = false;
    initialize();
  }

  void scanDirectory(const UniString &path) {
    initialized_ = true;
    hostStruct_.host = reinterpret_cast<OfxPropertySetHandle>(&hostDescriptor_);
    hostStruct_.fetchSuite = &ArtifactOfxHost::fetchSuiteCallback;
    clearLoadedPlugins();
    const QString root = path.toQString().trimmed();
    if (root.isEmpty()) {
      return;
    }
    scanRoots(QStringList{root});
  }

  void *getOfxHostStruct() {
    return &hostStruct_;
  }

  OfxPluginDescriptor *findDescriptor(const UniString &identifier) {
    for (auto &desc : plugins_) {
      if (desc.identifier == identifier) return &desc;
    }
    return nullptr;
  }

  SharedPtr<ImageEffectState> createRenderInstance(const UniString &identifier);

  // False once a rescan has invalidated the library handle this descriptor
  // captured. Callers must treat a stale descriptor as "cannot render" and
  // pass the frame through untouched rather than calling into freed code.
  bool isGenerationCurrent(const OfxPluginDescriptor &descriptor) const {
    return descriptor.generation == generation_;
  }

private:
  ArtifactOfxHost() = default;
  ~ArtifactOfxHost() { clearLoadedPlugins(); }

  // --- Crash accounting and blacklist -----------------------------------
  //
  // A plugin that faults is counted against its identifier; once it reaches
  // kOfxCrashLimit the identifier is blacklisted and persisted so the plugin
  // stays disabled across restarts. This is the only crash response available:
  // SEH cannot catch stack overflow, heap corruption detected later, or aborts
  // raised from inside the plugin's own CRT checks.

  void loadBlacklist() const {
    if (blacklistLoaded_) {
      return;
    }
    blacklistLoaded_ = true;
    QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
    settings.beginGroup(QStringLiteral("OfxPlugins"));
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
      blacklistedIdentifiers_.insert(key);
    }
    settings.endGroup();
  }

  void persistBlacklist() const {
    QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("Artifact"));
    settings.beginGroup(QStringLiteral("OfxPlugins"));
    settings.remove(QString());
    for (const QString &identifier : blacklistedIdentifiers_) {
      settings.setValue(identifier, 1);
    }
    settings.endGroup();
  }

  void recordPluginFault(const char *identifier) {
    const QString key = QString::fromLatin1(identifier ? identifier : "");
    if (key.isEmpty()) {
      return;
    }
    const int crashes = ++crashCounts_[key];
    if (crashes < kOfxCrashLimit) {
      qWarning("[OFX] plugin '%s' crashed (%d/%d); disabling it after %d faults",
               key.toUtf8().constData(), crashes, kOfxCrashLimit,
               kOfxCrashLimit);
      return;
    }
    if (blacklistedIdentifiers_.contains(key)) {
      return;
    }
    blacklistedIdentifiers_.insert(key);
    persistBlacklist();
    qWarning("[OFX] plugin '%s' crashed %d times and has been blacklisted",
             key.toUtf8().constData(), crashes);
  }

  bool isPluginBlacklisted(const OfxPluginDescriptor &descriptor) const {
    return isBlacklisted(descriptor.identifier.toQString().toUtf8().constData());
  }

  mutable QSet<QString> blacklistedIdentifiers_;
  mutable QHash<QString, int> crashCounts_;
  mutable bool blacklistLoaded_ = false;

  void clearLoadedPlugins() {
    // Invalidate every descriptor that points at the libraries being released
    // before the handles actually go away.
    ++generation_;
    for (const OfxLibraryHandle handle : loadedLibraries_) {
      closePluginLibrary(handle);
    }
    loadedLibraries_.clear();
    plugins_.clear();
  }

  static const void *fetchSuiteCallback(OfxPropertySetHandle /*host*/,
                                        const char *suiteName,
                                        int suiteVersion) {
    if (!suiteName) {
      return nullptr;
    }
    if (std::strcmp(suiteName, kOfxPropertySuite) == 0) {
      return propertySuite();
    }
    if (std::strcmp(suiteName, kOfxImageEffectSuite) == 0) {
      return imageEffectSuite();
    }
    if (std::strcmp(suiteName, kOfxParameterSuite) == 0) {
      return parameterSuite();
    }
    if (std::strcmp(suiteName, kOfxMessageSuite) == 0) {
      return messageSuite();
    }
    // These two are optional in the Image Effect API, but the OFX C++ support
    // library fetches them unconditionally: a plugin built with it aborts on a
    // null suite. Providing working implementations is cheaper than making
    // every such plugin tolerate a missing host.
    if (std::strcmp(suiteName, kOfxMemorySuite) == 0) {
      return memorySuite();
    }
    if (std::strcmp(suiteName, kOfxTimeLineSuite) == 0) {
      return timelineSuite();
    }
    if (std::strcmp(suiteName, kOfxProgressSuite) == 0) {
      // Both revisions are served: V2 takes a message id as well, and a plugin
      // asks for the version it was compiled against.
      return suiteVersion >= 2 ? progressSuiteV2()
                               : static_cast<const void *>(progressSuite());
    }
    if (std::strcmp(suiteName, kOfxInteractSuite) == 0) {
      return interactSuite();
    }
    return nullptr;
  }

  void scanRoots(const QStringList &roots) {
    std::unordered_set<std::wstring> visitedDirs;

    for (const QString &root : roots) {
      const QFileInfo rootInfo(root);
      if (!rootInfo.exists()) {
        continue;
      }

      if (rootInfo.isDir() && isOfxBundleDirectory(rootInfo)) {
        const QString binaryPath = findBundleBinary(rootInfo.absoluteFilePath());
        if (!binaryPath.isEmpty()) {
          scanBinary(rootInfo.absoluteFilePath(), binaryPath);
        }
        continue;
      }

      if (rootInfo.isFile()) {
        scanBinary(rootInfo.absoluteFilePath(), rootInfo.absoluteFilePath());
        continue;
      }

      std::vector<QString> pendingDirs;
      pendingDirs.push_back(rootInfo.absoluteFilePath());

      while (!pendingDirs.empty()) {
        const QString currentDirPath = pendingDirs.back();
        pendingDirs.pop_back();

        const std::wstring canonicalKey = QDir::cleanPath(currentDirPath).toStdWString();
        if (!visitedDirs.insert(canonicalKey).second) {
          continue;
        }

        const QFileInfo currentInfo(currentDirPath);
        if (currentInfo.isDir() && isOfxBundleDirectory(currentInfo)) {
          const QString binaryPath = findBundleBinary(currentInfo.absoluteFilePath());
          if (!binaryPath.isEmpty()) {
            scanBinary(currentInfo.absoluteFilePath(), binaryPath);
          }
          continue;
        }

        QDir currentDir(currentDirPath);
        const QFileInfoList entries = currentDir.entryInfoList(
            QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs, QDir::Name);

        for (const QFileInfo &entry : entries) {
          if (entry.isDir()) {
            if (isOfxBundleDirectory(entry)) {
              const QString binaryPath = findBundleBinary(entry.absoluteFilePath());
              if (!binaryPath.isEmpty()) {
                scanBinary(entry.absoluteFilePath(), binaryPath);
              }
              continue;
            }

            pendingDirs.push_back(entry.absoluteFilePath());
            continue;
          }

          if (isPluginBinaryFile(entry)) {
            scanBinary(entry.absoluteFilePath(), entry.absoluteFilePath());
          }
        }
      }
    }
  }

  QString findBundleBinary(const QString &bundleDir) const {
    const QFileInfo bundleInfo(bundleDir);
    const QString preferredBase = stripBundleSuffix(bundleInfo.fileName());
    const QStringList preferredNames = {
        preferredBase + QStringLiteral(".dll"),
        preferredBase + QStringLiteral(".ofx"),
        preferredBase + QStringLiteral(".so"),
        preferredBase + QStringLiteral(".dylib"),
    };

    QDir bundle(bundleDir);
    for (const QString &candidateName : preferredNames) {
      const QString candidatePath = bundle.filePath(candidateName);
      if (QFileInfo::exists(candidatePath)) {
        const QFileInfo candidateInfo(candidatePath);
        if (isPluginBinaryFile(candidateInfo)) {
          return candidatePath;
        }
      }
    }

    QDirIterator it(bundleDir, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QFileInfo info(it.next());
      if (isPluginBinaryFile(info)) {
        return info.absoluteFilePath();
      }
    }

    return {};
  }

  void scanBinary(const QString &bundlePath, const QString &binaryPath) {
    const OfxLibraryHandle handle = openPluginLibrary(binaryPath);
    if (handle == nullptr) {
      return;
    }

    const auto getNumber = reinterpret_cast<OfxGetNumberOfPluginsFn>(
        resolvePluginSymbol(handle, "OfxGetNumberOfPlugins"));
    const auto getPlugin = reinterpret_cast<OfxGetPluginFn>(
        resolvePluginSymbol(handle, "OfxGetPlugin"));
    if (!getPlugin || !getNumber) {
      closePluginLibrary(handle);
      return;
    }

    const int pluginCount = std::max(0, getNumber());
    bool acceptedAnyPlugin = false;

    for (int index = 0; index < pluginCount; ++index) {
      OfxPlugin *plugin = getPlugin(index);
      if (!plugin || !plugin->pluginApi || !plugin->pluginIdentifier ||
          std::strcmp(plugin->pluginApi, kOfxImageEffectPluginApi) != 0) {
        continue;
      }

      if (plugin->setHost) {
        plugin->setHost(&hostStruct_);
      }

      OfxPluginDescriptor descriptor;
      descriptor.pluginPath = UniString::fromQString(bundleDisplayPath(bundlePath, binaryPath));
      descriptor.identifier = UniString::fromQString(
          QString::fromLatin1(plugin->pluginIdentifier));
      descriptor.version = UniString::fromQString(
          QStringLiteral("%1.%2")
              .arg(static_cast<unsigned int>(plugin->pluginVersionMajor))
              .arg(static_cast<unsigned int>(plugin->pluginVersionMinor)));
      descriptor.libraryHandle = handle;
      descriptor.generation = generation_;

      if (!describePlugin(plugin, bundleDisplayPath(bundlePath, binaryPath), descriptor)) {
        continue;
      }

      plugins_.push_back(descriptor);
      acceptedAnyPlugin = true;
    }

    if (acceptedAnyPlugin) {
      loadedLibraries_.push_back(handle);
    } else {
      closePluginLibrary(handle);
    }
  }

  std::vector<OfxPluginDescriptor> plugins_;
  std::vector<OfxLibraryHandle> loadedLibraries_;
  PropertySet hostDescriptor_;
  OfxHost hostStruct_{};
  bool initialized_ = false;
  // Bumped on every scan so effect objects holding a descriptor can tell that
  // their library handle was unloaded by a rescan.
  std::uint64_t generation_ = 0;
};

SharedPtr<ImageEffectState> ArtifactOfxHost::createRenderInstance(
    const UniString& identifier) {
  auto *desc = findDescriptor(identifier);
  if (!desc || !desc->descriptorState) return nullptr;
  auto state = ArtifactCore::makeShared<ImageEffectState>();
  state->properties = desc->descriptorState->properties;
  state->paramSet = cloneParamSetState(desc->descriptorState->paramSet);
  state->clips.clear();
  for (const auto &kv : desc->descriptorState->clips) {
    auto cs = std::make_unique<ClipState>();
    cs->properties = kv.second->properties;
    cs->clipName = kv.second->clipName;
    // Re-point the owner at the new instance. Without this the clip handles
    // the plugin receives never resolve back to the instance they came from,
    // so every clipGetImage failed during the render action.
    cs->owner = state.get();
    state->clips[kv.first] = std::move(cs);
  }
  return state;
}

// Defined here rather than inside the class so the load/describe helpers
// declared above the class body can call it without requiring the class to be
// complete at that point in the translation unit.
OfxStatus dispatchOfxAction(OfxPlugin *plugin, const char *identifier,
                            const char *action, const void *instance,
                            OfxPropertySetHandle inArgs,
                            OfxPropertySetHandle outArgs) {
  return ArtifactOfxHost::instance().dispatchAction(plugin, identifier, action,
                                                    instance, inArgs, outArgs);
}

bool ofxPluginIsBlacklisted(const char *identifier) {
  return ArtifactOfxHost::instance().isBlacklisted(identifier);
}

OfxStatus pluginActionLoad(OfxPlugin *plugin) {
  if (!plugin || !plugin->mainEntry) {
    return kOfxStatErrBadHandle;
  }
  return dispatchOfxAction(plugin, plugin->pluginIdentifier, kOfxActionLoad,
                           nullptr, nullptr, nullptr);
}

OfxStatus pluginActionDescribe(OfxPlugin *plugin,
                               ImageEffectState &descriptorState) {
  if (!plugin || !plugin->mainEntry) {
    return kOfxStatErrBadHandle;
  }
  return dispatchOfxAction(plugin, plugin->pluginIdentifier, kOfxActionDescribe,
                           &descriptorState, nullptr, nullptr);
}

ImageEffectState *imageEffectForClip(const ClipState *clip) {
  // The clip carries a direct owner pointer. Walking the loaded-plugin list
  // could never resolve a render instance's clips: createRenderInstance
  // allocates fresh ClipState objects, so their addresses are absent from every
  // descriptor and the reverse lookup failed for all render-time fetches.
  return clip ? clip->owner : nullptr;
}

} // namespace Ofx
} // namespace Artifact
