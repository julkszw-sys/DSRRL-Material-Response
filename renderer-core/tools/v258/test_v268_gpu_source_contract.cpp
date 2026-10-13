// V268 native C++17 producer CB fail-open + independent beta test.
#include "v268_gpu_source_contract.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace dsrrl::v268;

static void require(bool cond,const char *message) {
    if(!cond) throw std::runtime_error(message);
}
static dsrrl::v257::admission admitted() {
    using namespace dsrrl::v257;
    admission a{};
    a.expected.receiver=33;
    a.expected.slot=2;
    a.expected.flver_fingerprint=1;
    a.expected.mtd_fingerprint=2;
    a.expected.shader_fingerprint=3;
    a.expected.logical_tex_hash=4;
    a.expected.flver.fill(0x11);
    a.expected.mtd.fill(0x22);
    a.expected.shader.fill(0x33);
    a.expected.texture.fill(0x44);
    a.expected.sidecar.fill(0x55);
    a.observed=a.expected;
    a.equipment_owner_verified=true;
    a.slot_verified=true;
    a.sidecar_loaded_and_hashed=true;
    a.consumer_family_proven=true;
    a.full_material_port_proven=true;
    a.exact_post_material_inputs_proven=true;
    return a;
}
static source proven() {
    source s{};
    s.exact_owner=admitted();
    s.ptde_c84_producer_verified=true;
    s.ptde_c85_producer_verified=true;
    s.ptde_upper_lower_producer_verified=true;
    s.ptde_material_producer_verified=true;
    s.native_rx_signature_verified=true;
    s.ptde_env_probe_a_b_verified=true;
    s.dsr_t2_normal_resource_verified=true;
    s.dsr_s2_sampler_verified=true;
    s.single_normal_material_operator_verified=true;
    s.dsr_normal_scale_producer_verified=true;
    s.c84={0.1f,0.2f,0.3f,0.25f};
    s.c85={0.4f,0.5f,0.6f,0.75f};
    s.c86={1.f,1.f,1.f,1.f};
    s.c87={2.f,2.f,2.f,2.f};
    s.c100={.5f,.5f,.5f,0.f};
    s.c101={1.f,2.f,3.f,0.f};
    s.c156={0.f,0.f,0.f,0.f};
    s.upper_lower={.12f,.34f,.56f,0.f};
    return s;
}
int main() {
  try {
    source s=proven();
    gpu_cb12 cb{};
    require(build_cb12(s,cb),"authorized independent-source positive");
    require(cb.reg[0][3]==.25f && cb.reg[1][3]==.75f,
            "independent PTDE c84.w and c85.w were collapsed");
    require(cb.reg[4][0]==.5f&&cb.reg[5][2]==3.f&&
            cb.reg[7][1]==.34f,"CB semantic order mismatch");

    // All negative gates must retain the *entire* prior 128-byte payload.
    gpu_cb12 known=cb;
    auto denied=s;denied.ptde_c85_producer_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,
            "unverified c85 accepted or partial CB update");
    denied=s;denied.ptde_c84_producer_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"unverified c84 accepted");
    denied=s;denied.ptde_upper_lower_producer_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"unverified UL accepted");
    denied=s;denied.ptde_env_probe_a_b_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"unverified probes accepted");
    denied=s;denied.dsr_t2_normal_resource_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,
            "unverified stock t2 normal accepted");
    denied=s;denied.dsr_s2_sampler_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,
            "unverified stock s2 normal sampler accepted");
    denied=s;denied.single_normal_material_operator_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,
            "dual/unknown normal operator accepted");
    denied=s;denied.dsr_normal_scale_producer_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,
            "missing host normal-scale source accepted");
    denied=s;denied.native_rx_signature_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"unverified RX accepted");
    denied=s;denied.ptde_material_producer_verified=false;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"unverified material accepted");
    denied=s;denied.exact_owner.observed.sidecar[0]^=1u;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"mismatched sidecar SHA accepted");
    denied=s;denied.exact_owner.observed.slot=1;
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"wrong material slot accepted");
    denied=s;denied.c85[3]=std::numeric_limits<float>::quiet_NaN();
    require(!build_cb12(denied,cb) && cb.reg==known.reg,"NaN c85 accepted");

    // Equal beta values are legal only when independently sourced.
    denied=s;denied.c85[3]=denied.c84[3];
    require(build_cb12(denied,cb) && cb.reg[0][3]==cb.reg[1][3],
            "independently verified coincident controls incorrectly rejected");
    std::puts("V268_INDEPENDENT_PTDE_C84_C85_B12_128B_NATIVE_PASS");
    std::puts("V268_OWNER_SOURCE_FAILOPEN_AND_FULL_BUFFER_ATOMICITY_PASS");
    return 0;
  }catch(const std::exception &e){
    std::fprintf(stderr,"V268_GPU_CB_ERROR %s\n",e.what());
    return 2;
  }
}
