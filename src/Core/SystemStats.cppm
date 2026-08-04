module;
#include <utility>
#include <algorithm>
#include <chrono>
#if defined(_WIN32)
#include <Windows.h>
#else
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#endif

#include <QtGlobal>

module Core.SystemStats;

import std;

namespace ArtifactCore {

CpuMonitor::CpuMonitor() {}
CpuMonitor::~CpuMonitor() {}

SystemMemoryInfo CpuMonitor::queryMemory() const {
    SystemMemoryInfo info;
#if defined(_WIN32)
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        info.totalBytes = static_cast<uint64_t>(memory.ullTotalPhys);
        info.freeBytes = static_cast<uint64_t>(memory.ullAvailPhys);
    }
#else
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return info;
    char line[256];
    unsigned long long memTotal = 0;
    unsigned long long memAvailable = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &memTotal) == 1) continue;
        if (sscanf(line, "MemAvailable: %llu kB", &memAvailable) == 1) continue;
    }
    fclose(f);
    if (memTotal > 0) {
        info.totalBytes = memTotal * 1024ULL;
        if (memAvailable > 0) info.freeBytes = memAvailable * 1024ULL;
    }
#endif
    return info;
}

ProcessCpuInfo CpuMonitor::queryProcessCpu() {
    ProcessCpuInfo info;
#if defined(_WIN32)
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        const auto toSeconds = [](const FILETIME& value) {
            ULARGE_INTEGER ticks{};
            ticks.LowPart = value.dwLowDateTime;
            ticks.HighPart = value.dwHighDateTime;
            return static_cast<double>(ticks.QuadPart) / 10000000.0;
        };
        const double processSeconds = toSeconds(kernel) + toSeconds(user);
        const double now = std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        static double previousProcessSeconds = 0.0;
        static double previousWallSeconds = 0.0;
        if (previousWallSeconds > 0.0) {
            const double processDelta = processSeconds - previousProcessSeconds;
            const double wallDelta = now - previousWallSeconds;
            if (wallDelta > 0.0 && processDelta >= 0.0) {
                info.processPercent = std::clamp((processDelta / wallDelta) * 100.0,
                                                 0.0, 100.0);
            }
        }
        previousProcessSeconds = processSeconds;
        previousWallSeconds = now;
    }
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        const double procSecs = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6
                              + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
        static double prevProc = 0.0;
        static double prevTime = 0.0;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const double now = ts.tv_sec + ts.tv_nsec / 1e9;
        if (prevTime > 0.0) {
            const double dProc = procSecs - prevProc;
            const double dTime = now - prevTime;
            if (dTime > 0.0) info.processPercent = (dProc / dTime) * 100.0;
        }
        prevProc = procSecs;
        prevTime = now;
    }
#endif
    return info;
}

}
