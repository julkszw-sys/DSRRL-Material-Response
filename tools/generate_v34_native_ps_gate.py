#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path
import re


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 exact match, got {count}")
    return text.replace(old, new, 1)


def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    out, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 regex match, got {count}")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    args = ap.parse_args()
    src = Path(args.input).read_text(encoding="utf-8")

    src = replace_once(
        src,
        "#include <windows.h>\n#include <bcrypt.h>",
        "#include <windows.h>\n#include <d3d11.h>\n#include <bcrypt.h>",
        "d3d11 include",
    )

    counter_anchor = """std::atomic<std::uint64_t> g_v33_dedicated_match{0};
std::atomic<std::uint64_t> g_v33_present_consumer_reject{0};
std::atomic<std::uint64_t> g_v33_raw_rgba_created{0};
std::atomic<std::uint64_t> g_v33_nonpmetal_present_failopen{0};
"""
    helper_block = counter_anchor + r'''
constexpr char k_expected_mr_build131_sha256[] =
    "db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966";
constexpr std::array<std::uintptr_t,3> k_mr_dedicated_ps_rvas = {
    0x112308, 0x112328, 0x112348
};
HMODULE g_v34_mr_host = nullptr;
bool g_v34_mr_guard = false;
std::atomic<std::uint64_t> g_v34_mr_guard_pass{0};
std::atomic<std::uint64_t> g_v34_mr_guard_fail{0};
std::atomic<std::uint64_t> g_v34_ps_ready_max{0};
std::atomic<std::uint64_t> g_v34_native_ps_match{0};
std::atomic<std::uint64_t> g_v34_native_ps_reject{0};

bool verify_mr_build131_host_v34()
{
    HMODULE module = GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");
    if (module == nullptr) { ++g_v34_mr_guard_fail; return false; }
    std::array<std::uint8_t,32> digest{};
    const auto path = module_path(module);
    if (path.empty() || !sha256_file(path, digest) ||
        hex_string(digest.data(), digest.size()) != k_expected_mr_build131_sha256) {
        ++g_v34_mr_guard_fail;
        return false;
    }
    g_v34_mr_host = module;
    g_v34_mr_guard = true;
    ++g_v34_mr_guard_pass;
    return true;
}

std::array<ID3D11PixelShader *,3> mr_dedicated_ps_v34(std::uint64_t &ready)
{
    std::array<ID3D11PixelShader *,3> out{};
    ready = 0;
    if (!g_v34_mr_guard || g_v34_mr_host == nullptr) return out;
    auto *base = reinterpret_cast<std::uint8_t *>(g_v34_mr_host);
    __try {
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = *reinterpret_cast<ID3D11PixelShader **>(base + k_mr_dedicated_ps_rvas[i]);
            if (out[i] != nullptr) ++ready;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        ready = 0;
    }
    auto prior = g_v34_ps_ready_max.load();
    while (ready > prior && !g_v34_ps_ready_max.compare_exchange_weak(prior, ready)) {}
    return out;
}

bool native_dedicated_pmetal_bound_v34(reshade::api::command_list *cmd)
{
    if (cmd == nullptr || !g_v34_mr_guard) { ++g_v34_native_ps_reject; return false; }
    auto *context = reinterpret_cast<ID3D11DeviceContext *>(cmd->get_native());
    if (context == nullptr) { ++g_v34_native_ps_reject; return false; }
    std::uint64_t ready = 0;
    const auto expected = mr_dedicated_ps_v34(ready);
    if (ready == 0) { ++g_v34_native_ps_reject; return false; }
    ID3D11PixelShader *current = nullptr;
    context->PSGetShader(&current, nullptr, nullptr);
    bool match = false;
    if (current != nullptr) {
        for (auto *candidate : expected) {
            if (candidate != nullptr && current == candidate) { match = true; break; }
        }
        current->Release();
    }
    if (match) ++g_v34_native_ps_match;
    else ++g_v34_native_ps_reject;
    return match;
}
'''
    src = replace_once(src, counter_anchor, helper_block, "native PS helper insertion")

    new_build_plan = r'''std::optional<replacement_plan_v33> build_plan_v33(reshade::api::command_list *cmd)
{
    const auto binding = active_binding();
    if (!binding.has_value() || binding->record == nullptr) {
        ++g_material_unknown; ++g_fail_open; return std::nullopt;
    }
    const material_record &m = *binding->record;

    command_bindings current{};
    {
        std::lock_guard lock(g_mutex);
        const auto it = g_command_bindings.find(command_id(cmd));
        if (it == g_command_bindings.end()) { ++g_fail_open; return std::nullopt; }
        current = it->second;
    }

    replacement_plan_v33 plan{}; plan.original = current;
    if (m.state == material_envspec_state::present) {
        if (!is_exact_pmetal(m)) {
            ++g_v33_nonpmetal_present_failopen; ++g_material_unknown; ++g_fail_open;
            return std::nullopt;
        }
        if (!native_dedicated_pmetal_bound_v34(cmd)) {
            ++g_v33_present_consumer_reject; ++g_fail_open;
            return std::nullopt;
        }
        ++g_v33_dedicated_match;
        ++g_receiver_match;
        if (!current.srv[0].valid || !current.srv[1].valid) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        const auto probe_a = probe_for_view(current.srv[0].view);
        const auto probe_b = probe_for_view(current.srv[1].view);
        if (!probe_a.has_value() || !probe_b.has_value()) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        const auto raw_a = get_raw_rgba_cube_v33(*probe_a, m.slot);
        const auto raw_b = get_raw_rgba_cube_v33(*probe_b, m.slot);
        if (!raw_a.has_value() || !raw_b.has_value()) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        plan.replacement[0] = *raw_a;
        plan.replacement[1] = *raw_b;
        plan.replace[0] = true;
        plan.replace[1] = true;
        plan.use_exact_sampler = true;
        ++g_resource_route_hit; ++g_resource_route_hit;
        ++g_material_present;
        return plan;
    }

    if (m.state == material_envspec_state::explicit_none && binding->explicit_none_safe) {
        const auto pipeline = global_tracker().resolve_bound_pipeline(command_id(cmd));
        if (!pipeline.has_value() || !pipeline->confirmed) {
            ++g_receiver_reject; ++g_fail_open; return std::nullopt;
        }
        if (is_dedicated_pmetal_receiver(pipeline->receiver_id)) {
            ++g_receiver_reject; ++g_fail_open; return std::nullopt;
        }
        ++g_receiver_match;
        if (g_black_view.handle == 0 && !create_black_cube()) { ++g_fail_open; return std::nullopt; }
        bool any = false;
        for (int i = 0; i < 2; ++i) {
            if (!current.srv[i].valid) continue;
            plan.replacement[i] = g_black_view;
            plan.replace[i] = true;
            any = true;
        }
        if (!any) { ++g_resource_route_miss; ++g_fail_open; return std::nullopt; }
        ++g_material_explicit_none;
        return plan;
    }

    ++g_material_unknown; ++g_fail_open;
    return std::nullopt;
}

'''
    src = regex_once(
        src,
        r"std::optional<replacement_plan_v33> build_plan_v33\(reshade::api::command_list \*cmd\)\n\{.*?\n\}\n\nbool apply_plan_v33",
        new_build_plan + "bool apply_plan_v33",
        "build_plan_v33 replacement",
    )

    new_log = r'''void log_snapshot_v33(const runtime_snapshot &snapshot)
{
    const std::uint64_t p = ++g_present_count;
    if (p != 1 && (p % 300) != 0) return;
    char line[2100] = {};
    std::snprintf(line, sizeof(line),
        "[DSRRL FULL ENVSPEC V3.4] P=%llu EXE=%llu/%llu HOOK=%llu/%llu MR=%llu/%llu ps_ready=%llu "
        "PARSER=%llu mapped=%llu unmapped=%llu SELECTOR=%llu resolved=%llu "
        "native=%llu/%llu miss=%llu raw=%llu recv=%llu reject=%llu "
        "dedicated=%llu native_ps=%llu/%llu present_reject=%llu nonpmetal_present=%llu "
        "mat_present=%llu explicit_none=%llu unknown=%llu route=%llu miss=%llu "
        "t12=%llu t14=%llu sampler=%llu replay=%llu restore=%llu/%llu failopen=%llu "
        "LIVE res=%llu view=%llu pipe=%llu cmd=%llu",
        static_cast<unsigned long long>(p),
        static_cast<unsigned long long>(g_exe_guard_pass.load()),
        static_cast<unsigned long long>(g_exe_guard_fail.load()),
        static_cast<unsigned long long>(g_hook_install_pass.load()),
        static_cast<unsigned long long>(g_hook_install_fail.load()),
        static_cast<unsigned long long>(g_v34_mr_guard_pass.load()),
        static_cast<unsigned long long>(g_v34_mr_guard_fail.load()),
        static_cast<unsigned long long>(g_v34_ps_ready_max.load()),
        static_cast<unsigned long long>(g_parser_seen.load()),
        static_cast<unsigned long long>(g_material_mapped.load()),
        static_cast<unsigned long long>(g_material_unmapped.load()),
        static_cast<unsigned long long>(g_selector_seen.load()),
        static_cast<unsigned long long>(g_selector_resolved.load()),
        static_cast<unsigned long long>(g_native_matches.load()),
        static_cast<unsigned long long>(g_native_candidates.load()),
        static_cast<unsigned long long>(g_native_hash_miss.load()),
        static_cast<unsigned long long>(g_v33_raw_rgba_created.load()),
        static_cast<unsigned long long>(g_receiver_match.load()),
        static_cast<unsigned long long>(g_receiver_reject.load()),
        static_cast<unsigned long long>(g_v33_dedicated_match.load()),
        static_cast<unsigned long long>(g_v34_native_ps_match.load()),
        static_cast<unsigned long long>(g_v34_native_ps_reject.load()),
        static_cast<unsigned long long>(g_v33_present_consumer_reject.load()),
        static_cast<unsigned long long>(g_v33_nonpmetal_present_failopen.load()),
        static_cast<unsigned long long>(g_material_present.load()),
        static_cast<unsigned long long>(g_material_explicit_none.load()),
        static_cast<unsigned long long>(g_material_unknown.load()),
        static_cast<unsigned long long>(g_resource_route_hit.load()),
        static_cast<unsigned long long>(g_resource_route_miss.load()),
        static_cast<unsigned long long>(g_t12_rebind.load()),
        static_cast<unsigned long long>(g_t14_rebind.load()),
        static_cast<unsigned long long>(g_sampler_rebind.load()),
        static_cast<unsigned long long>(g_replay_pass.load()),
        static_cast<unsigned long long>(g_restore_pass.load()),
        static_cast<unsigned long long>(g_restore_fail.load()),
        static_cast<unsigned long long>(g_fail_open.load()),
        static_cast<unsigned long long>(snapshot.live_resources),
        static_cast<unsigned long long>(snapshot.live_views),
        static_cast<unsigned long long>(snapshot.live_pipelines),
        static_cast<unsigned long long>(snapshot.live_commands));
    reshade::log::message(reshade::log::level::info, line);
}

'''
    src = regex_once(
        src,
        r"void log_snapshot_v33\(const runtime_snapshot &snapshot\)\n\{.*?\n\}\n\nvoid register_v33_events",
        new_log + "void register_v33_events",
        "telemetry replacement",
    )

    src = replace_once(
        src,
        '"DSRRL PTDE Full EnvSpec Carrier V3.3 Raw RGBA Dedicated P_Metal"',
        '"DSRRL PTDE Full EnvSpec Carrier V3.4 Native PS Gate"',
        "NAME",
    )
    src = replace_once(
        src,
        '"Build131 coexistence; raw PTDE RGBA filtered before shader decode; exact P_Metal dedicated-consumer gate; explicit-none safe subset; fail-open elsewhere."',
        '"Build131 coexistence; raw PTDE RGBA; exact P_Metal actual-material + native bound ID3D11PixelShader gate; explicit-none safe subset; fail-open elsewhere."',
        "DESCRIPTION",
    )

    init_anchor = '''    if (!load_router_sidecars(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC V3.3] material/probe sidecar load/SHA failed; bridge stays fail-open");
    }
'''
    init_insert = init_anchor + '''    if (!verify_mr_build131_host_v34()) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC V3.4] exact Material Response build131 SHA guard failed; PRESENT stays fail-open");
    }
'''
    src = replace_once(src, init_anchor, init_insert, "MR host guard init")
    src = src.replace("[DSRRL FULL ENVSPEC V3.3]", "[DSRRL FULL ENVSPEC V3.4]")
    src = src.replace("build131 callsites PASS; raw RGBA + dedicated P_Metal consumer gate armed",
                      "build131 callsites PASS; raw RGBA + native PS P_Metal consumer gate armed")

    uninit_anchor = '''    unpatch_callsites();
    unregister_v33_events();
'''
    uninit_replace = '''    unpatch_callsites();
    g_v34_mr_host = nullptr;
    g_v34_mr_guard = false;
    unregister_v33_events();
'''
    src = replace_once(src, uninit_anchor, uninit_replace, "MR host clear")

    if "native_dedicated_pmetal_bound_v34(cmd)" not in src:
        raise RuntimeError("native PS gate missing from generated source")
    if "context->PSGetShader" not in src:
        raise RuntimeError("PSGetShader missing from generated source")
    if "get_raw_rgba_cube_v33" not in src:
        raise RuntimeError("raw RGBA path missing from generated source")

    Path(args.output).write_text(src, encoding="utf-8")
    print(f"generated {args.output}: {len(src)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
