from pathlib import Path
import argparse
I=Path("renderer-core/integrated/integrated_addon.cpp")
S=Path("renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp")
def transform():
 a=I.read_text(encoding="utf8");b=S.read_text(encoding="utf8")
 marker='#include "../tools/v277/v277_exact_stable_lerp_dxbc_capture.hpp"'
 assert a.count(marker)==1
 a=a.replace(marker,marker+'\n#include "../tools/v278/v278_r5_lerp_experiment.hpp"',1)
 needle='''                if (g_pmetal_envspec.
                        register_lerp_replacement('''
 assert a.count(needle)==1
 replacement='''                // V278 experimental Lerp-only, exact R5 source island.
                // Keep V277 payload intact if R5 strict preconditions fail.
                if (envspec_lerp.semantic_receiver_id >= 33u &&
                    envspec_lerp.semantic_receiver_id <= 35u) {
                    auto r5_candidate = envspec_lerp_payload;
                    const bool r5_ok = dsrrl::operators::env_spec::
                        v278_apply_existing_stable_r5_on_exact_lerp(r5_candidate);
                    if (r5_ok) envspec_lerp_payload = std::move(r5_candidate);
                    char msg[240]{};
                    std::snprintf(msg,sizeof(msg),
                        "[DSRRL V278 R5 LERP AB] rx=%u result=%s bytes=%zu "
                        "pixel=UNOBSERVED",
                        envspec_lerp.semantic_receiver_id,
                        r5_ok?"R5_APPLIED":"FALLBACK_V272",
                        envspec_lerp_payload.size());
                    reshade::log::message(reshade::log::level::info,msg);
                }
'''
 a=a.replace(needle,replacement+needle,1)
 base='''} // namespace

pmetal_rgba_materialize_outcome
materialize_pmetal_rgba_receiver('''
 assert b.count(base)==1
 insert='''} // namespace

bool v278_apply_existing_stable_r5_on_exact_lerp(
    std::vector<std::uint8_t> &payload) noexcept {
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
    return apply_exact_ptde_normal_basis_r5(payload);
#else
    (void)payload;
    return false;
#endif
}

pmetal_rgba_materialize_outcome
materialize_pmetal_rgba_receiver('''
 b=b.replace(base,insert,1)
 I.write_text(a,encoding="utf8",newline="\n")
 S.write_text(b,encoding="utf8",newline="\n")
def main():
 p=argparse.ArgumentParser();p.add_argument("--apply",action="store_true")
 args=p.parse_args()
 if args.apply:transform()
 else:
    assert I.read_text().count("[DSRRL V278 R5 LERP AB]")==1
    assert S.read_text().count("bool v278_apply_existing_stable_r5_on_exact_lerp(")==1
 print("V278_R5_LERP_SOURCE_ORACLE_PASS")
if __name__=="__main__":main()
