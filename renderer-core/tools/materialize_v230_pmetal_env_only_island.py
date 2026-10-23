#!/usr/bin/env python3
"""Operator-local Source1 EnvSpec shader without PTDE t10. V230 experimental bridge.

Do not patch V359 release or silently alias texture names. Apply at end of
V230 native build overlays. Full PTDE shader keeps its existing contract.
"""
from pathlib import Path
R=Path("renderer-core")
def patch(path,old,new):
    f=R/path
    s=f.read_text("utf-8")
    assert s.count(old)==1,(path,old[:80],s.count(old))
    f.write_text(s.replace(old,new,1),"utf-8")
def inject(path,needle,insert):patch(path,needle,insert+needle)

patch("include/dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp",
    "    std::vector<std::uint8_t> &output) noexcept;",
    "    std::vector<std::uint8_t> &output,\n    bool envspec_only = false) noexcept;")
m="src/operators/env_spec/pmetal_rgba_materializer.cpp"
patch(m,
    "#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)",
    "#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_NO_TAIL_DIAG) || defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG) || defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)")
patch(m,
    "    std::vector<std::uint8_t> &output) noexcept\n{",
    "    std::vector<std::uint8_t> &output,\n    bool envspec_only) noexcept\n{")
inject(m,
    "#if defined(DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG)\n#if defined(DSRRL_PMETAL_FORCE_PTDE_PACKEDGI)",
    '''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    if (envspec_only) {
        // V13 exact certified shader family, no t10 declaration/sample.
        // PTDE A/B/beta, native DSR cubes, stock material SpecRGB.
        if (compose_upper_lower ||
            !apply_v13_native_dsr_no_tail(base,*authority) ||
            !v13_native_dsr_no_tail_postcondition(base,*authority)) {
            outcome.result = pmetal_rgba_materialize_result::
                fail_build131_precondition;
            return outcome;
        }
        outcome.spec_rgb_consumer=false;
        outcome.envdiffuse_linear_consumer_diag=false;
        output=std::move(base);
        outcome.result=pmetal_rgba_materialize_result::applied;
        return outcome;
    }
#else
    (void)envspec_only;
#endif
''')
h="include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
inject(h,"    bool register_lerp_replacement(",
    '''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    bool register_envspec_only_replacement(
        const operators::env_spec::pmetal_rgba_materialize_outcome &outcome,
        const void *dxbc, std::size_t size) noexcept;
#endif
''')
patch(h,"        ID3D11PixelShader *shader = nullptr;\n        core::operator_mask composed_owners = 0;",
      '''        ID3D11PixelShader *shader = nullptr;
#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
        ID3D11PixelShader *env_only_shader = nullptr;
        core::sha256_digest env_only_sha{};
        std::size_t env_only_size = 0;
#endif
        core::operator_mask composed_owners = 0;''')
c="src/runtime/pmetal_envspec_draw_runtime.cpp"
patch(c,'''        if (record.shader != nullptr)
            record.shader->Release();
    }
    replacements_.clear();''',
'''        if (record.shader != nullptr)
            record.shader->Release();
#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
        if (record.env_only_shader != nullptr)
            record.env_only_shader->Release();
#endif
    }
    replacements_.clear();''')
inject(c,"bool pmetal_envspec_draw_runtime::\nregister_lerp_replacement(",
'''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
bool pmetal_envspec_draw_runtime::register_envspec_only_replacement(
    const operators::env_spec::pmetal_rgba_materialize_outcome &outcome,
    const void *dxbc, std::size_t size) noexcept
{
    using result=operators::env_spec::pmetal_rgba_materialize_result;
    if(outcome.result!=result::applied ||
       outcome.receiver_id<33u || outcome.receiver_id>35u ||
       outcome.upper_lower_composed || outcome.spec_rgb_consumer ||
       outcome.envdiffuse_linear_consumer_diag ||
       dxbc==nullptr || size==0u || quarantined_.load())
        return false;
    const auto sha=hashing::sha256(
        static_cast<const std::uint8_t *>(dxbc),size);
    std::lock_guard<std::mutex> lock(mutex_);
    if(device_==nullptr) return false;
    auto &rec=replacements_[outcome.receiver_id];
    if(rec.env_only_shader!=nullptr)
        return rec.env_only_size==size &&
               rec.env_only_sha==sha &&
               rec.composed_owners==outcome.composed_owners;
    ID3D11PixelShader *ps=nullptr;
    if(FAILED(device_->CreatePixelShader(dxbc,size,nullptr,&ps)) ||
       ps==nullptr) return false;
    rec.env_only_shader=ps;
    rec.env_only_sha=sha;
    rec.env_only_size=size;
    return true;
}
#endif

''')
inject(c,"    effect_latch(effect_replacement_ready_);",
'''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    // Selected after shader registration but before any D3D state changes.
    // "no sidecar" cannot excuse a shader that still reads t10.
#endif
''')
patch(c,
'''        log_prepare_stage_once(
            1u << 6u,
            "context_reject",
            material,
            decision,
            family);
        return false;
    }''',
'''        log_prepare_stage_once(
            1u << 6u,
            "context_reject",
            material,
            decision,
            family);
        return false;
    }
#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    const auto spec_probe = material_resources_.probe_exact_specular_companion(context);
    const bool env_only =
        family == pmetal_envspec_receiver_family::stable_hemenv &&
        exact_pmetal_material(material) &&
        !spec_probe.companion_ready;
    if(env_only) {
        static std::atomic<std::uint32_t> seen{0u};
        const auto n=seen.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=12u) {
            char line[216]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL PMETAL ENV ONLY] n=%u rx=%u route=%u hash=%016llx t10=unused",
                n,decision.receiver_id,material.route_index,
                (unsigned long long)spec_probe.logical_hash);
            reshade::log::message(reshade::log::level::warning,line);
        }
    }
#else
    constexpr bool env_only=false;
#endif''')
patch(c,
'''            shader = found->second.shader;
            composed_owners =
                found->second.composed_owners;
            shader->AddRef();''',
'''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
            shader = env_only ? found->second.env_only_shader :
                found->second.shader;
            if(shader==nullptr) return false;
#else
            shader = found->second.shader;
#endif
            composed_owners =
                found->second.composed_owners;
            shader->AddRef();''')
patch(c,"    const bool shadow_env_ready =\n        !k_native_dsr_cubemap_feed &&",
      "    const bool shadow_env_ready =\n        !(k_native_dsr_cubemap_feed || env_only) &&")
patch(c,"    const bool env_resource_ready =\n        k_native_dsr_cubemap_feed",
      "    const bool env_resource_ready =\n        (k_native_dsr_cubemap_feed || env_only)")
inject(c,'''    ID3D11ShaderResourceView *shadow_material[3]{};''',
'''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    if (!env_only) {
#endif
''')
patch(c,"    effect_latch(effect_spec_rgb_ready_);\n#endif",
'''    effect_latch(effect_spec_rgb_ready_);
#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
    }
#endif
#endif''')
patch(c,"    if (!env_resources_.prepare_envdiffuse(\n",
      "    if (!env_only && !env_resources_.prepare_envdiffuse(\n")
patch(c,"    if (!g_native_envdiffuse_logged.exchange(\n",
      "    if (!env_only && !g_native_envdiffuse_logged.exchange(\n")
patch(c,
'''    prepared.request.additional_owners =
        mr_owner |
        domain_owner |
        spec_owner |
        envdiff_owner |
        composed_owners;''',
'''    prepared.request.additional_owners =
        mr_owner | domain_owner | composed_owners |
        (env_only ? 0u : (spec_owner | envdiff_owner));''')
patch(c,
'''    prepared.request.additional_constant_buffer_owners =
        mr_owner |
        envdiff_owner;
    prepared.request.additional_resource_owners =
        envdiff_owner;''',
'''    prepared.request.additional_constant_buffer_owners =
        mr_owner | (env_only ? 0u : envdiff_owner);
    prepared.request.additional_resource_owners =
        env_only ? 0u : envdiff_owner;''')
patch(c,"        env_owner | mr_owner | envdiff_owner\n#endif",
        "        env_owner | mr_owner | (env_only ? 0u : envdiff_owner)\n#endif")
patch(c,
'''    prepared.request.srvs[
        request_srv_count++] = {
            11u,
            prepared.envdiffuse_resources.ptde_a
        };''',
'''    if(!env_only) {
        prepared.request.srvs[request_srv_count++] = {
            11u,prepared.envdiffuse_resources.ptde_a
        };
    }''')
patch(c,
'''        prepared.request.srvs[
            request_srv_count++] = {
                13u,
                prepared.envdiffuse_resources.ptde_b
            };''',
'''        if(!env_only) {
            prepared.request.srvs[request_srv_count++] = {
                13u,prepared.envdiffuse_resources.ptde_b
            };
        }''')
patch(c,
'''    const bool shadow_r7_draw =
        family ==''',
'''    const bool shadow_r7_draw =
        !env_only &&
        family ==''')
patch(c,
'''    prepared.request.samplers[
        request_sampler_count++] = {
            11u,
            prepared.envdiffuse_resources.sampler
        };''',
'''    if(!env_only) {
        prepared.request.samplers[request_sampler_count++] = {
            11u,prepared.envdiffuse_resources.sampler
        };
    }''')
patch(c,
'''        prepared.request.samplers[
            request_sampler_count++] = {
                13u,
                prepared.envdiffuse_resources.sampler
            };''',
'''        if(!env_only) {
            prepared.request.samplers[request_sampler_count++] = {
                13u,prepared.envdiffuse_resources.sampler
            };
        }''')
i="integrated/integrated_addon.cpp"
inject(i,"                    ++g_envspec_payload_materialize_ok;",
'''#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)
                    // Create a second PS only from the SAME exact stock receiver.
                    std::vector<std::uint8_t> alt_bytes;
                    const auto alt=dsrrl::operators::env_spec::
                        materialize_pmetal_rgba_receiver(
                            g_core.features(),source,pixel_shader->code_size,
                            false,alt_bytes,true);
                    if(alt.result == dsrrl::operators::env_spec::
                           pmetal_rgba_materialize_result::applied &&
                       !alt.spec_rgb_consumer)
                        g_pmetal_envspec.register_envspec_only_replacement(
                            alt,alt_bytes.data(),alt_bytes.size());
#endif
''')
print("V230_PMETAL_ENV_ONLY_ISLAND_PATCH_PASS")
