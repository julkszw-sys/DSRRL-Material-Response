// V275: the production policy already exists; this test proves the
// *exact* alternate-builder predicate rather than expanding it by area.
// This file compiles against the real production source header.
#include "dsrrl/runtime/pmetal_selector_policy.hpp"
#include <cstdio>
using dsrrl::runtime::pmetal_selector_policy::is_synthetic_zero_descriptor;
using dsrrl::runtime::pmetal_selector_policy::select;
static int err(bool ok,const char* why){
 if(!ok){std::fprintf(stderr,"V275_FAIL %s\n",why);return 1;}
 return 0;
}
int main(){
 int n=0;
 n+=err(is_synthetic_zero_descriptor(0x20EB7Fu,0u,0u,0.0f),
        "alternate zero tuple must not publish m10");
 n+=err(is_synthetic_zero_descriptor(0x20EB7Fu,0u,0u,-0.0f),
        "negative zero shares numerical zero semantics");
 n+=err(!is_synthetic_zero_descriptor(0x20E019u,0u,0u,0.0f),
        "authentic full-builder area0 row0 must survive");
 n+=err(!is_synthetic_zero_descriptor(0x20FB9Eu,0u,0u,0.0f),
        "unproven different return site must not be excluded");
 n+=err(!is_synthetic_zero_descriptor(0x20EB7Fu,1u,0u,0.0f),
        "nonzero A prevents this exact synthetic signature");
 n+=err(!is_synthetic_zero_descriptor(0x20EB7Fu,0u,1u,0.0f),
        "nonzero B prevents this exact synthetic signature");
 n+=err(!is_synthetic_zero_descriptor(0x20EB7Fu,0u,0u,0.125f),
        "real nonzero interior beta prevents synthetic match");
 const auto real=select(0,0,0.0f);
 n+=err(real.valid && real.a==0 && real.b==0,
        "legitimate 0 tuple remains a valid selector elsewhere");
 if(!n)std::puts("V275_SYNTHETIC_ZERO_POLICY_8_POSITIVE_NEGATIVE_CASES_PASS");
 return n?2:0;
}
