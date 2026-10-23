#!/usr/bin/env python3
"""Exact P_Metal native-EnvSpec + PTDE EnvDiffuse (no PTDE SpecRGB t10).

Apply after V230 PMetal EnvOnly and generated-physical-cut create hooks.
Compile only in diagnostic V230; exact RX33-35. PTDE t11 + c86 operator
is independently prepared and may fall back to the existing no-t10 variant.
The full PTDE SpecRGB composite and all nonmetal material gates are untouched.
"""
from pathlib import Path
R=Path("renderer-core")
def patch(path, old, new, n=1):
    f=Path(path) if str(path).startswith("build-") else R/path
    s=f.read_text("utf-8")
    count=s.count(old)
    assert count==n,("ENV_DIFFUSE_SOURCE_DRIFT",str(path),count,n,old[:92])
    f.write_text(s.replace(old,new,n),"utf-8")
M="src/operators/env_spec/pmetal_rgba_materializer.cpp"
H="include/dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
RTH="include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
RTC="src/runtime/pmetal_envspec_draw_runtime.cpp"
patch(H,
'''    std::vector<std::uint8_t> &output,
    bool envspec_only = false) noexcept;''',
'''    std::vector<std::uint8_t> &output,
    bool envspec_only = false,
    bool with_ptde_envdiffuse = false) noexcept;''')
patch(M,
'''    std::vector<std::uint8_t> &output,
    bool envspec_only) noexcept
{''',
'''    std::vector<std::uint8_t> &output,
    bool envspec_only,
    bool with_ptde_envdiffuse) noexcept
{''')
patch(M,
'''        outcome.spec_rgb_consumer=false;
        outcome.envdiffuse_linear_consumer_diag=false;
        output=std::move(base);''',
'''#if defined(DSRRL_V230_ENV_DIFFUSE_SPLIT) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
        if(with_ptde_envdiffuse &&
           !apply_exact_ptde_envdiffuse_consumer(base)) {
            outcome.result=pmetal_rgba_materialize_result::fail_postcondition;
            return outcome;
        }
        // Exact PTDE t11 RGBA/A consumer + c86 endpoint in b12[3].
        outcome.envdiffuse_linear_consumer_diag=with_ptde_envdiffuse;
#else
        if(with_ptde_envdiffuse) {
            outcome.result=pmetal_rgba_materialize_result::fail_postcondition;
            return outcome;
        }
        outcome.envdiffuse_linear_consumer_diag=false;
#endif
        outcome.spec_rgb_consumer=false;
        output=std::move(base);''')
patch(RTH,
'''    bool register_envspec_only_replacement(''',
'''    bool register_envdiffuse_only_replacement(
        const operators::env_spec::pmetal_rgba_materialize_outcome &outcome,
        const void *dxbc, std::size_t size) noexcept;
    bool register_envspec_only_replacement(''')
patch(RTH,
'''        ID3D11PixelShader *env_only_shader = nullptr;
        core::sha256_digest env_only_sha{};''',
'''        ID3D11PixelShader *env_only_shader = nullptr;
        ID3D11PixelShader *envdiffuse_only_shader = nullptr;
        core::sha256_digest envdiffuse_only_sha{};
        std::size_t envdiffuse_only_size = 0;
        core::sha256_digest env_only_sha{};''')
patch(RTC,
'''        if (record.env_only_shader != nullptr)
            record.env_only_shader->Release();''',
'''        if (record.env_only_shader != nullptr)
            record.env_only_shader->Release();
        if (record.envdiffuse_only_shader != nullptr)
            record.envdiffuse_only_shader->Release();''',2)
patch(RTC,
'''bool pmetal_envspec_draw_runtime::register_envspec_only_replacement(''',
'''#if defined(DSRRL_V230_ENV_DIFFUSE_SPLIT)
bool pmetal_envspec_draw_runtime::register_envdiffuse_only_replacement(
    const operators::env_spec::pmetal_rgba_materialize_outcome &outcome,
    const void *dxbc, std::size_t size) noexcept
{
    using result=operators::env_spec::pmetal_rgba_materialize_result;
    if (outcome.result!=result::applied ||
        outcome.receiver_id<33u || outcome.receiver_id>35u ||
        outcome.spec_rgb_consumer ||
        !outcome.envdiffuse_linear_consumer_diag ||
        outcome.upper_lower_composed ||
        dxbc==nullptr || size==0u || quarantined_.load())
        return false;
    const auto sha=hashing::sha256(
        static_cast<const std::uint8_t *>(dxbc),size);
    std::lock_guard<std::mutex> lock(mutex_);
    if(device_==nullptr) return false;
    auto &rec=replacements_[outcome.receiver_id];
    if(rec.envdiffuse_only_shader!=nullptr)
        return rec.envdiffuse_only_size==size &&
               rec.envdiffuse_only_sha==sha;
    ID3D11PixelShader *ps=nullptr;
    if(FAILED(device_->CreatePixelShader(dxbc,size,nullptr,&ps)) ||
       ps==nullptr) return false;
    rec.envdiffuse_only_shader=ps;
    rec.envdiffuse_only_sha=sha;
    rec.envdiffuse_only_size=size;
    return true;
}
#endif

bool pmetal_envspec_draw_runtime::register_envspec_only_replacement(''')
# The prior no-tail registration code exists only under the original flag.
# This one is nested within it, to avoid changing global runtime registrations.
old='''    if (!env_only && !env_resources_.prepare_envdiffuse(
'''
new='''#if defined(DSRRL_V230_ENV_DIFFUSE_SPLIT) && defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    bool use_ptde_envdiffuse_island=false;
    if(env_only) {
        // PTDE packed t11 is independent of character material SpecRGB t10.
        // If unavailable, preserve the already-certified no-t10 shader and
        // stock EnvDiffuse; never block the EnvSpec draw.
        const bool valid_ptde_t11=env_resources_.prepare_envdiffuse(
            prepared.env_resources.probe_a,
            prepared.env_resources.probe_b,
            false,prepared.envdiffuse_resources);
        if(valid_ptde_t11 &&
           prepared.envdiffuse_resources.ready &&
           prepared.envdiffuse_resources.ptde_a!=nullptr &&
           prepared.envdiffuse_resources.sampler!=nullptr) {
            ID3D11PixelShader *ptde_diffuse_ps=nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it=replacements_.find(decision.receiver_id);
                if(it!=replacements_.end() &&
                   it->second.envdiffuse_only_shader!=nullptr) {
                    ptde_diffuse_ps=it->second.envdiffuse_only_shader;
                    ptde_diffuse_ps->AddRef();
                }
            }
            if(ptde_diffuse_ps!=nullptr) {
                if(shader!=nullptr) shader->Release();
                shader=ptde_diffuse_ps;
                use_ptde_envdiffuse_island=true;
            }
        }
        if(!use_ptde_envdiffuse_island)
            env_resources_.release(prepared.envdiffuse_resources);
        static std::atomic<std::uint32_t> count{0u};
        const auto n=count.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=20u) {
            char msg[212]{};
            std::snprintf(msg,sizeof(msg),
                "[DSRRL SOURCE1 ENVDIFFUSE] n=%u rx=%u route=%u probe=%u ptde_t11=%u shader_ptde=%u no_t10=1",
                n,decision.receiver_id,material.route_index,
                prepared.env_resources.probe_a,
                valid_ptde_t11?1u:0u,use_ptde_envdiffuse_island?1u:0u);
            reshade::log::message(reshade::log::level::warning,msg);
        }
    }
#else
    constexpr bool use_ptde_envdiffuse_island=false;
#endif
    if (!env_only && !env_resources_.prepare_envdiffuse(
'''
patch(RTC,old,new)
patch(RTC,
'''(env_only ? 0u : (spec_owner | envdiff_owner))''',
'''(env_only ? (use_ptde_envdiffuse_island ? envdiff_owner : 0u) :
                    (spec_owner | envdiff_owner))''')
patch(RTC,
'''prepared.request.additional_constant_buffer_owners =
        mr_owner | (env_only ? 0u : envdiff_owner);''',
'''prepared.request.additional_constant_buffer_owners =
        mr_owner | ((env_only && !use_ptde_envdiffuse_island) ? 0u : envdiff_owner);''')
patch(RTC,
'''env_only ? 0u : envdiff_owner;''',
'''(env_only && !use_ptde_envdiffuse_island) ? 0u : envdiff_owner;''')
patch(RTC,
'''env_owner | mr_owner | (env_only ? 0u : envdiff_owner)''',
'''env_owner | mr_owner | ((env_only && !use_ptde_envdiffuse_island) ? 0u : envdiff_owner)''')
patch(RTC,'''if(!env_only) {
        prepared.request.srvs''',
          '''if(!env_only || use_ptde_envdiffuse_island) {
        prepared.request.srvs''')
patch(RTC,'''if (!env_only) {
            prepared.request.srvs''',
          '''if (!env_only || use_ptde_envdiffuse_island) {
            prepared.request.srvs''')
patch(RTC,'''if(!env_only) {
        prepared.request.samplers''',
          '''if(!env_only || use_ptde_envdiffuse_island) {
        prepared.request.samplers''')
patch(RTC,'''if (!env_only) {
            prepared.request.samplers''',
          '''if (!env_only || use_ptde_envdiffuse_island) {
            prepared.request.samplers''')
# Stable HemEnv construct: two variants are registered from the *same*
# exact PS SHA. Patch only the first stable registration in the compiled unit,
# and do not regenerate the entire TU (which would delete V13 overlays).
PHYS="build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp"
create='''#if defined(DSRRL_V230_ENV_DIFFUSE_SPLIT)
                    std::vector<std::uint8_t> diffuse_no_t10_bytes;
                    const auto diffuse_no_t10=dsrrl::operators::env_spec::
                        materialize_pmetal_rgba_receiver(
                            g_core.features(),source,pixel_shader->code_size,
                            false,diffuse_no_t10_bytes,true,true);
                    const bool diffuse_compiled =
                        diffuse_no_t10.result ==
                            dsrrl::operators::env_spec::
                                pmetal_rgba_materialize_result::applied &&
                        !diffuse_no_t10.spec_rgb_consumer &&
                        diffuse_no_t10.envdiffuse_linear_consumer_diag;
                    const bool diffuse_registered =
                        diffuse_compiled &&
                        g_pmetal_envspec.register_envdiffuse_only_replacement(
                            diffuse_no_t10,diffuse_no_t10_bytes.data(),
                            diffuse_no_t10_bytes.size());
                    static std::atomic<std::uint32_t> once{0u};
                    const auto number=once.fetch_add(
                        1u,std::memory_order_relaxed)+1u;
                    if(number<=6u) {
                        char msg[220]{};
                        std::snprintf(msg,sizeof(msg),
                            "[DSRRL SOURCE1 ENVDIFFUSE CREATE] n=%u rx=%u result=%u compiled=%u registered=%u bytes=%llu",
                            number,diffuse_no_t10.receiver_id,
                            static_cast<unsigned>(diffuse_no_t10.result),
                            diffuse_compiled?1u:0u,
                            diffuse_registered?1u:0u,
                            (unsigned long long)diffuse_no_t10_bytes.size());
                        reshade::log::message(
                            reshade::log::level::warning,msg);
                    }
#endif
'''
for path in ["integrated/integrated_addon.cpp",PHYS]:
    compiled_path=Path(path) if path==PHYS else R/path
    body=compiled_path.read_text("utf-8")
    anchor="                    ++g_envspec_payload_materialize_ok;"
    assert body.count(anchor)==2,("CREATE_ANCHOR_DRIFT",path,body.count(anchor))
    assert "[DSRRL ENV ONLY CREATE]" in body,("NO_PRIOR_SHADER_CREATE",path)
    assert "[DSRRL SOURCE1 ENVDIFFUSE CREATE]" not in body
    compiled_path.write_text(body.replace(anchor,create+anchor,1),"utf-8")
print("V230_SOURCE1_ENVDIFFUSE_SPLIT_PASS independent exact PTDE t11/c86, optional per draw, no t10")
