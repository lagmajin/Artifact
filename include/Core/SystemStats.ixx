module;
#include <QtGlobal>
export module Core.SystemStats;

import std;

export namespace ArtifactCore {

struct SystemMemoryInfo {
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
};

struct ProcessCpuInfo {
    double processPercent = 0.0; // percent of one CPU (0-100)
};

class CpuMonitor {
public:
    CpuMonitor();
    ~CpuMonitor();

    SystemMemoryInfo queryMemory() const;
    ProcessCpuInfo queryProcessCpu();
};

}
