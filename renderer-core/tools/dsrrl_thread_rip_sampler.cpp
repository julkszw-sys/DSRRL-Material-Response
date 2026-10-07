#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t k_target_process[] = L"DarkSoulsRemastered.exe";
constexpr unsigned k_duration_seconds = 30u;
constexpr unsigned k_sample_interval_ms = 10u;
constexpr std::uint64_t k_rva_bucket_size = 0x20u;

std::uint64_t filetime_u64(const FILETIME &ft) noexcept
{
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}

DWORD find_process_id(const wchar_t *name) noexcept
{
    HANDLE snapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0u);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0u;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0u;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

std::vector<DWORD> enumerate_threads(DWORD pid)
{
    std::vector<DWORD> tids;
    HANDLE snapshot =
        CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0u);
    if (snapshot == INVALID_HANDLE_VALUE)
        return tids;

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid)
                tids.push_back(entry.th32ThreadID);
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return tids;
}

bool thread_cpu_100ns(
    DWORD tid,
    std::uint64_t &out_cpu) noexcept
{
    HANDLE thread =
        OpenThread(
            THREAD_QUERY_INFORMATION |
                THREAD_QUERY_LIMITED_INFORMATION,
            FALSE,
            tid);
    if (thread == nullptr)
        return false;

    FILETIME create{}, exit{}, kernel{}, user{};
    const bool ok =
        GetThreadTimes(
            thread,
            &create,
            &exit,
            &kernel,
            &user) != FALSE;
    CloseHandle(thread);

    if (!ok)
        return false;

    out_cpu =
        filetime_u64(kernel) +
        filetime_u64(user);
    return true;
}

std::unordered_map<DWORD, std::uint64_t>
snapshot_thread_cpu(DWORD pid)
{
    std::unordered_map<DWORD, std::uint64_t> result;
    const auto tids = enumerate_threads(pid);
    result.reserve(tids.size() * 2u + 1u);

    for (const auto tid : tids) {
        std::uint64_t cpu = 0u;
        if (thread_cpu_100ns(tid, cpu))
            result.emplace(tid, cpu);
    }

    return result;
}

struct cpu_delta {
    DWORD tid = 0u;
    std::uint64_t delta_100ns = 0u;
    double one_core_percent = 0.0;
};

std::vector<cpu_delta> compute_cpu_deltas(
    const std::unordered_map<DWORD, std::uint64_t> &before,
    const std::unordered_map<DWORD, std::uint64_t> &after,
    std::uint64_t wall_100ns)
{
    std::vector<cpu_delta> deltas;
    deltas.reserve(after.size());

    for (const auto &[tid, value] : after) {
        const auto it = before.find(tid);
        if (it == before.end() || value < it->second)
            continue;

        const auto delta = value - it->second;
        cpu_delta row{};
        row.tid = tid;
        row.delta_100ns = delta;
        row.one_core_percent =
            wall_100ns != 0u
                ? (static_cast<double>(delta) * 100.0) /
                      static_cast<double>(wall_100ns)
                : 0.0;
        deltas.push_back(row);
    }

    std::sort(
        deltas.begin(),
        deltas.end(),
        [](const cpu_delta &a, const cpu_delta &b) {
            if (a.delta_100ns != b.delta_100ns)
                return a.delta_100ns > b.delta_100ns;
            return a.tid < b.tid;
        });

    return deltas;
}

struct module_range {
    std::uintptr_t base = 0u;
    std::size_t size = 0u;
    std::wstring name;
};

std::vector<module_range> enumerate_modules(DWORD pid)
{
    std::vector<module_range> modules;
    HANDLE snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE |
                TH32CS_SNAPMODULE32,
            pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return modules;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Module32FirstW(snapshot, &entry)) {
        do {
            module_range mod{};
            mod.base =
                reinterpret_cast<std::uintptr_t>(
                    entry.modBaseAddr);
            mod.size =
                static_cast<std::size_t>(
                    entry.modBaseSize);
            mod.name = entry.szModule;
            modules.push_back(std::move(mod));
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    std::sort(
        modules.begin(),
        modules.end(),
        [](const module_range &a, const module_range &b) {
            return a.base < b.base;
        });
    return modules;
}

const module_range *find_module(
    const std::vector<module_range> &modules,
    std::uintptr_t address) noexcept
{
    for (const auto &mod : modules) {
        const auto end =
            mod.base +
            static_cast<std::uintptr_t>(mod.size);
        if (address >= mod.base && address < end)
            return &mod;
    }
    return nullptr;
}

bool sample_thread_rip(
    DWORD tid,
    std::uintptr_t &out_rip) noexcept
{
    HANDLE thread =
        OpenThread(
            THREAD_SUSPEND_RESUME |
                THREAD_GET_CONTEXT |
                THREAD_QUERY_INFORMATION,
            FALSE,
            tid);
    if (thread == nullptr)
        return false;

    const DWORD suspend_count =
        SuspendThread(thread);
    if (suspend_count == static_cast<DWORD>(-1)) {
        CloseHandle(thread);
        return false;
    }

    CONTEXT context{};
    context.ContextFlags = CONTEXT_CONTROL;
    const bool ok =
        GetThreadContext(
            thread,
            &context) != FALSE;

    const DWORD resume_result =
        ResumeThread(thread);
    CloseHandle(thread);

    if (!ok ||
        resume_result == static_cast<DWORD>(-1))
        return false;

#if defined(_M_X64) || defined(__x86_64__)
    out_rip =
        static_cast<std::uintptr_t>(
            context.Rip);
#else
#error DSRRL thread sampler is x64-only.
#endif
    return true;
}

struct rip_key {
    DWORD tid = 0u;
    std::wstring module;
    std::uint64_t rva_bucket = 0u;

    bool operator<(const rip_key &other) const noexcept
    {
        if (tid != other.tid)
            return tid < other.tid;
        if (module != other.module)
            return module < other.module;
        return rva_bucket < other.rva_bucket;
    }
};

std::wstring hex_u64(std::uint64_t value)
{
    std::wostringstream stream;
    stream << L"0x"
           << std::hex
           << std::uppercase
           << value;
    return stream.str();
}

void write_cpu_rows(
    std::wofstream &log,
    unsigned second_index,
    const std::vector<cpu_delta> &deltas)
{
    log << L"[CPU] second="
        << second_index
        << L" top=";

    const auto count =
        std::min<std::size_t>(
            deltas.size(),
            8u);

    for (std::size_t i = 0u; i < count; ++i) {
        if (i != 0u)
            log << L",";
        log << L"tid:"
            << deltas[i].tid
            << L"/core_pct:"
            << std::fixed
            << std::setprecision(2)
            << deltas[i].one_core_percent;
    }
    log << L"\n";
    log.flush();
}

} // namespace

int wmain()
{
    const DWORD pid =
        find_process_id(k_target_process);
    if (pid == 0u) {
        std::wcerr
            << L"DarkSoulsRemastered.exe is not running.\n"
            << L"Start the game, enter the slow-FPS state, then run this sampler.\n";
        return 2;
    }

    const std::wstring output_name =
        L"DSRRL_THREAD_RIP_SAMPLE_" +
        std::to_wstring(pid) +
        L".log";

    std::wofstream log(output_name);
    if (!log) {
        std::wcerr
            << L"Could not create "
            << output_name
            << L"\n";
        return 3;
    }

    log
        << L"DSRRL external thread/RIP sampler R49\n"
        << L"target=" << k_target_process
        << L" pid=" << pid
        << L" duration_s=" << k_duration_seconds
        << L" interval_ms=" << k_sample_interval_ms
        << L" rva_bucket=" << hex_u64(k_rva_bucket_size)
        << L"\n";

    auto modules =
        enumerate_modules(pid);
    log << L"[MODULES] count="
        << modules.size()
        << L"\n";
    for (const auto &mod : modules) {
        log << L"[MODULE] name="
            << mod.name
            << L" base="
            << hex_u64(
                   static_cast<std::uint64_t>(
                       mod.base))
            << L" size="
            << hex_u64(
                   static_cast<std::uint64_t>(
                       mod.size))
            << L"\n";
    }
    log.flush();

    std::wcout
        << L"Attached to DarkSoulsRemastered.exe PID "
        << pid
        << L".\n"
        << L"Sampling for "
        << k_duration_seconds
        << L" seconds. Keep reproducing the slow-FPS scene.\n";

    auto cpu_before =
        snapshot_thread_cpu(pid);
    auto wall_before =
        std::chrono::steady_clock::now();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(1000));

    auto cpu_after =
        snapshot_thread_cpu(pid);
    auto wall_after =
        std::chrono::steady_clock::now();

    const auto warmup_wall =
        std::chrono::duration_cast<
            std::chrono::nanoseconds>(
                wall_after - wall_before)
            .count();
    const std::uint64_t warmup_100ns =
        warmup_wall > 0
            ? static_cast<std::uint64_t>(
                  warmup_wall / 100)
            : 0u;

    auto deltas =
        compute_cpu_deltas(
            cpu_before,
            cpu_after,
            warmup_100ns);
    write_cpu_rows(
        log,
        0u,
        deltas);

    DWORD hot_tid =
        !deltas.empty()
            ? deltas.front().tid
            : 0u;

    std::map<rip_key, std::uint64_t>
        rip_histogram;
    std::map<std::wstring, std::uint64_t>
        module_histogram;
    std::uint64_t rip_samples_ok = 0u;
    std::uint64_t rip_samples_fail = 0u;

    for (unsigned second = 1u;
         second <= k_duration_seconds;
         ++second) {
        cpu_before =
            snapshot_thread_cpu(pid);
        wall_before =
            std::chrono::steady_clock::now();

        const unsigned samples_per_second =
            std::max(
                1u,
                1000u / k_sample_interval_ms);

        for (unsigned sample = 0u;
             sample < samples_per_second;
             ++sample) {
            if (hot_tid != 0u) {
                std::uintptr_t rip = 0u;
                if (sample_thread_rip(
                        hot_tid,
                        rip)) {
                    ++rip_samples_ok;

                    const auto *mod =
                        find_module(
                            modules,
                            rip);
                    std::wstring module_name =
                        L"<unknown>";
                    std::uint64_t rva =
                        static_cast<std::uint64_t>(
                            rip);

                    if (mod != nullptr) {
                        module_name = mod->name;
                        rva =
                            static_cast<std::uint64_t>(
                                rip - mod->base);
                    }

                    const auto bucket =
                        rva &
                        ~(k_rva_bucket_size - 1u);

                    rip_key key{};
                    key.tid = hot_tid;
                    key.module = module_name;
                    key.rva_bucket = bucket;
                    ++rip_histogram[key];
                    ++module_histogram[module_name];
                } else {
                    ++rip_samples_fail;
                }
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    k_sample_interval_ms));
        }

        cpu_after =
            snapshot_thread_cpu(pid);
        wall_after =
            std::chrono::steady_clock::now();

        const auto wall_ns =
            std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    wall_after - wall_before)
                .count();
        const std::uint64_t wall_100ns =
            wall_ns > 0
                ? static_cast<std::uint64_t>(
                      wall_ns / 100)
                : 0u;

        deltas =
            compute_cpu_deltas(
                cpu_before,
                cpu_after,
                wall_100ns);
        write_cpu_rows(
            log,
            second,
            deltas);

        if (!deltas.empty())
            hot_tid = deltas.front().tid;

        if ((second % 5u) == 0u)
            modules = enumerate_modules(pid);

        std::wcout
            << L"second "
            << second
            << L"/"
            << k_duration_seconds;

        if (!deltas.empty()) {
            std::wcout
                << L" hot_tid="
                << deltas.front().tid
                << L" core="
                << std::fixed
                << std::setprecision(1)
                << deltas.front().one_core_percent
                << L"%";
        }
        std::wcout << L"\n";
    }

    struct histogram_row {
        rip_key key;
        std::uint64_t samples = 0u;
    };

    std::vector<histogram_row> rows;
    rows.reserve(rip_histogram.size());
    for (const auto &[key, count] : rip_histogram)
        rows.push_back({key, count});

    std::sort(
        rows.begin(),
        rows.end(),
        [](const histogram_row &a,
           const histogram_row &b) {
            if (a.samples != b.samples)
                return a.samples > b.samples;
            return a.key < b.key;
        });

    log
        << L"[SUMMARY] rip_ok="
        << rip_samples_ok
        << L" rip_fail="
        << rip_samples_fail
        << L"\n";

    std::vector<
        std::pair<std::wstring, std::uint64_t>>
        module_rows(
            module_histogram.begin(),
            module_histogram.end());
    std::sort(
        module_rows.begin(),
        module_rows.end(),
        [](const auto &a, const auto &b) {
            return a.second > b.second;
        });

    for (const auto &[module, count] :
         module_rows) {
        const double pct =
            rip_samples_ok != 0u
                ? (static_cast<double>(count) *
                   100.0) /
                      static_cast<double>(
                          rip_samples_ok)
                : 0.0;
        log
            << L"[MODULE_HOT] name="
            << module
            << L" samples="
            << count
            << L" pct="
            << std::fixed
            << std::setprecision(2)
            << pct
            << L"\n";
    }

    const auto top_count =
        std::min<std::size_t>(
            rows.size(),
            100u);
    for (std::size_t i = 0u;
         i < top_count;
         ++i) {
        const auto &row = rows[i];
        const double pct =
            rip_samples_ok != 0u
                ? (static_cast<double>(
                       row.samples) *
                   100.0) /
                      static_cast<double>(
                          rip_samples_ok)
                : 0.0;

        log
            << L"[RIP_HOT] rank="
            << (i + 1u)
            << L" tid="
            << row.key.tid
            << L" module="
            << row.key.module
            << L" rva_bucket="
            << hex_u64(
                   row.key.rva_bucket)
            << L" samples="
            << row.samples
            << L" pct="
            << std::fixed
            << std::setprecision(2)
            << pct
            << L"\n";
    }

    log.flush();
    log.close();

    std::wcout
        << L"Done. Send me "
        << output_name
        << L"\n";
    return 0;
}
