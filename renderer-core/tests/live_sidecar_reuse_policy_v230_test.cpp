#include "dsrrl/runtime/live_sidecar_reuse_policy_v230.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
using dsrrl::runtime::live_sidecar_reuse_candidate_v230;
using dsrrl::runtime::live_sidecar_reuse_need_v230;
using dsrrl::runtime::may_reuse_live_sidecar_v230;
constexpr live_sidecar_reuse_candidate_v230 k_full{
    0x1010u, L"HD_A_9550_s", true, true, true};
constexpr live_sidecar_reuse_need_v230 k_request{
    0x1010u, L"HD_A_9550_s", true, true, true};
static_assert(may_reuse_live_sidecar_v230(k_full,k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9550_s",false,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9550_s",true,false,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9550_s",true,true,false}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x2020u,L"HD_A_9550_s",true,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9551_s",true,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"hd_a_9550_s",true,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"",true,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    {0u,L"HD_A_9550_s",true,true,true}, k_request));
static_assert(!may_reuse_live_sidecar_v230(
    k_full, {0x1010u,L"HD_A_9550_s",false,false,false}));
static_assert(may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9550_s",true,false,false},
    {0x1010u,L"HD_A_9550_s",true,false,false}));
static_assert(!may_reuse_live_sidecar_v230(
    {0x1010u,L"HD_A_9550_s",false,false,false},
    {0x1010u,L"HD_A_9550_s",true,false,false}));
int main(){
    assert(may_reuse_live_sidecar_v230(k_full,k_request));
    std::cout<<"V230_LIVE_SIDECAR_EXACT_DEVICE_NAME_CLASS_POLICY_PASS\n";
}
