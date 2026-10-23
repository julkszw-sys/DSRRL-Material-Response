#!/usr/bin/env python3
"""Quelaag one-shot A/B EnvDiffuse-only TEMPORARY diagnostic overlay.

Applies after materialize_v230_cutscene_trace.py, before MSVC compile.
EXPERIMENTAL: not a canonical PTDE operator or final pixel fix.
The verified base is v2.3.0 build235. Do not put this in production.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "src/runtime/pmetal_envspec_draw_runtime.cpp"
src = path.read_text(encoding="utf-8")
anchor = "    const bool stable_envdiffuse_consumer_diag ="
assert src.count(anchor) == 1, "Source drift; refusing blind instrumentation"

injection = r"""
#if defined(DSRRL_V230_REMO_ONE_SHOT_AB)
    // One-scene falsification experiment, NOT source correction. Only a
    // fully admitted Stable HemEnv receiver with the exact observed m14 bank
    // signature, a single source row 5/0 and the exact GI ordinal 188 enters.
    // No global LightBank change; EnvSpec source, beta, cube SRVs, shaders,
    // material response, original source cache and native hook are untouched.
    if (family == pmetal_envspec_receiver_family::stable_hemenv &&
        material.valid &&
        source.bank_signature_a == 0xc9b879a8c5e624d0ull &&
        source.bank_signature_b == source.bank_signature_a &&
        source.beta == 0.0f &&
        prepared.env_resources.probe_a == 188u &&
        (source.row_id_a == 0u || source.row_id_a == 5u) &&
        source.row_id_b == source.row_id_a) {

        struct row5_snapshot {
            std::array<std::uint8_t,32> flver{};
            std::array<std::uint8_t,32> mtd{};
            std::uint32_t slot = 0u;
            std::array<float,3> envdiffuse{};
            std::uint64_t last_ms = 0u;
            bool valid = false;
        };
        static std::array<row5_snapshot,64u> snapshots{};
        static std::mutex snapshots_mutex{};
        static std::atomic<int> last_logged_ab{-1};
        const auto now_ms = static_cast<std::uint64_t>(GetTickCount64());
        auto same_material = [&](const row5_snapshot &r) {
            return r.valid &&
                r.flver == material.flver_sha256 &&
                r.mtd == material.raw_mtd_sha256 &&
                r.slot == static_cast<std::uint32_t>(material.material_slot);
        };

        bool matched_row5 = false;
        bool override_diffuse = false;
        {
            std::lock_guard<std::mutex> guard(snapshots_mutex);
            row5_snapshot *previous = nullptr;
            for (auto &entry : snapshots) {
                if (same_material(entry)) {
                    previous = &entry;
                    break;
                }
            }

            if (source.row_id_a == 5u) {
                if (previous == nullptr) {
                    for (auto &entry : snapshots) {
                        if (!entry.valid ||
                            now_ms < entry.last_ms ||
                            now_ms - entry.last_ms > 30000u) {
                            previous = &entry;
                            break;
                        }
                    }
                }
                if (previous != nullptr) {
                    previous->flver = material.flver_sha256;
                    previous->mtd = material.raw_mtd_sha256;
                    previous->slot =
                        static_cast<std::uint32_t>(material.material_slot);
                    previous->envdiffuse = source.envdiffuse_a;
                    previous->last_ms = now_ms;
                    previous->valid = true;
                }
            } else if (previous != nullptr &&
                       now_ms >= previous->last_ms &&
                       now_ms - previous->last_ms <= 30000u) {
                matched_row5 = true;
                // A (0): original row0 PTDE EnvDiffuse.
                // B (1): same material's last *observed* row5 EnvDiffuse.
                // Alternate every 1200 ms during the one-shot REMO.
                override_diffuse = ((now_ms / 1200u) & 1u) != 0u;
                if (override_diffuse) {
                    source.envdiffuse_a = previous->envdiffuse;
                    source.envdiffuse_b = previous->envdiffuse;
                }
            }
        }

        if (source.row_id_a == 0u && matched_row5) {
            const int mode = override_diffuse ? 1 : 0;
            if (last_logged_ab.exchange(
                    mode, std::memory_order_relaxed) != mode) {
                char line[416]{};
                std::snprintf(
                    line,sizeof(line),
                    "[DSRRL REMO AB] mode=%s row_source=0 "
                    "envspec_unchanged=1 probe=188 rx=%u route=%u "
                    "envdiffuse=%.7g,%.7g,%.7g tick=%llu",
                    override_diffuse ? "B_LAST_ROW5" : "A_NATIVE_ROW0",
                    static_cast<unsigned>(decision.receiver_id),
                    static_cast<unsigned>(decision.route_index),
                    static_cast<double>(source.envdiffuse_a[0]),
                    static_cast<double>(source.envdiffuse_a[1]),
                    static_cast<double>(source.envdiffuse_a[2]),
                    static_cast<unsigned long long>(now_ms));
                reshade::log::message(
                    reshade::log::level::info,line);
            }
        }

#if defined(DSRRL_V230_CUTSCENE_TRACE)
        // Make observer report the EFFECTIVE prepared CPU source, not merely
        // the pre-override request. source_kind remains original hook=2.
        prepared.v230_cutscene_source = source;
#endif
    }
#endif // DSRRL_V230_REMO_ONE_SHOT_AB

"""
path.write_text(src.replace(anchor,injection+anchor,1),encoding="utf-8")
print("V230_REMO_ONE_SHOT_AB_OVERLAY_PASS stable RX33/34 exact m14 row0/5 & GI188; EnvSpec untouched")
