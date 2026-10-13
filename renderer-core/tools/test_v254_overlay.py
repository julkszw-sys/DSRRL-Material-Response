"""Offline source-patcher and C++ diagnostic format smoke checks (not Win64 runtime)."""
import importlib.util
import pathlib
import subprocess
import tempfile

ROOT=pathlib.Path(__file__).resolve().parent
SCRIPT=ROOT/'apply_v254_lerp_multievent_diag.py'
spec=importlib.util.spec_from_file_location('v254', SCRIPT)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

synthetic='''#include <atomic>
#include <cstdint>
#include <cstdio>
#include <reshade.hpp>
'''+ '\n'.join((
    '[DSRRL V250 PMETAL LERP DRAW]',
    '[DSRRL V252 NONMETAL LERP]',
    m.DECL_ANCHOR+'    const unsigned cls = route == 345u ? 0u : 1u;\n}\n',
    m.GLOBAL_ANCHOR+'    const unsigned index =\n        (static_cast<unsigned>(route_number) * 2u +\n}\n',
    m.NATIVE_ANCHOR+'\n',
    m.NATIVE_ANCHOR+'\n',
    m.PRESENT_ANCHOR,
))
assert synthetic.count(m.NATIVE_ANCHOR)==2
patched=m.patch_integrated(synthetic)
assert m.patch_integrated(patched)==patched, 'integrated patch must be idempotent'
assert patched.count('[DSRRL V254 LERP EVENTS]')==1
assert patched.count('[DSRRL V254 LERP SUMMARY]')==1
assert patched.count('sampled[2]')==2
source='[DSRRL V253 PTDE ENVSPEC RGBM]\n'+m.SOURCE_ANCHOR+'\n};'
source_patched=m.patch_pmetal(source)
assert m.patch_pmetal(source_patched)==source_patched, 'source patch must be idempotent'
assert '[DSRRL V254 LERP SOURCE]' in source_patched

# C++ unit: compile exact injected helper and the exact source log body.
# Stub native structures only; this cannot prove Windows D3D runtime behavior.
helpers=m.GLOBAL_REPLACEMENT[:-len(m.GLOBAL_ANCHOR)]
source_code=m.SOURCE_REPLACEMENT[:-len(m.SOURCE_ANCHOR)]
code=r'''#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <array>
namespace reshade { namespace log { enum class level { info }; void message(level,const char*) {} } }
namespace dsrrl::runtime {
'''+helpers+r'''
struct Decision { std::uint32_t receiver_id{33},route_index{345}; float c100[3]{}; float c101{2.5f}; };
struct Source {
float beta{.5f},a[3]{},b[3]{},envdiffuse_a[3]{},envdiffuse_b[3]{},phn_k135{1};
bool phn_k135_valid{false};
std::uint64_t bank_signature_a{},bank_signature_b{};
std::uint32_t row_id_a{},row_id_b{};
};
enum class pmetal_envspec_receiver_family { stable_hemenv, hemenvlerp };
void test_source(){
 Source source{};
 Decision decision{};
 auto family=pmetal_envspec_receiver_family::hemenvlerp;
 std::array<float,3> ptde_spec_a{},ptde_spec_b{};
'''+source_code+r'''
}
}
int main(){dsrrl::runtime::v254_lerp_event(0,33,1,345,0,0,0,0,0,0,0,0,0,0);dsrrl::runtime::v254_lerp_summary(600);dsrrl::runtime::test_source();}
'''
with tempfile.TemporaryDirectory() as tmp:
    file=pathlib.Path(tmp)/'v254_diag.cpp'
    file.write_text(code,encoding='utf8')
    result=subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Wformat=2','-fsyntax-only',str(file)],capture_output=True,text=True)
    if result.returncode:
        print(result.stdout,result.stderr)
        raise AssertionError('C++ diagnostic emission syntax failed')
print('PASS: idempotent generated-source overlay + repeat native sample + C++17 syntax/printf types')
