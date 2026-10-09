#include "dsrrl/runtime/pmetal_asylum_cmdlist_trace.hpp"
#include <cassert>
#include <iostream>
using namespace dsrrl::runtime::pmetal_asylum_cmdlist_trace;
int main() {
    observer o;
    o.observe_deferred_update(0x10,0x1000,0,2064);
    o.observe_receiver_candidate(0x10,0x1000,34,m10_bank,0,0x67538dba,1);
    o.observe_receiver_candidate(0x10,0x1000,34,m18_bank,32,0x67538dba,1);
    o.close(0x10,0x1000);
    auto finished=o.secondary(0x20,0x10);
    assert(finished.phase==stage::after_finish);
    assert(finished.bank_mask==3 && finished.rx34_candidates==2);
    assert(finished.size_matched_b0_updates==1 && finished.recorded_native==0x1000);
    o.reset_command(0x10);
    auto submitted=o.secondary(0x30,0x20);
    assert(submitted.phase==stage::before_execute);
    assert(submitted.generation==finished.generation);
    assert(submitted.finished_api==0x20 && submitted.execution_api==0x30);
    o.observe_receiver_candidate(0x10,0x1001,33,m18_bank,33,0x01020304,4);
    o.close(0x10,0x1001);
    auto second=o.secondary(0x21,0x10);
    assert(second.phase==stage::after_finish && second.bank_mask==2);
    assert(second.generation!=finished.generation);
    assert(o.secondary(0x30,0x20).bank_mask==3);
    o.destroy_command(0x20);
    assert(o.secondary(0x30,0x20).phase==stage::none);
    o.reset_command(0x10);
    o.close(0x10,0x1001);
    assert(o.secondary(0x22,0x10).phase==stage::none);
    assert(o.secondary(0x30,0x22).phase==stage::none);
    o.observe_receiver_candidate(0x40,0x4000,34,0xDEAD,0,0,0);
    o.close(0x40,0x4000);
    assert(o.secondary(0x41,0x40).phase==stage::none);
    o.observe_deferred_update(0x50,0x5000,0,96);
    o.close(0x50,0x5000);
    assert(o.secondary(0x51,0x50).phase==stage::none);
    o.observe_receiver_candidate(0x60,0x6000,33,m10_bank,0,0xAAAAAAAA,2);
    o.observe_receiver_candidate(0x70,0x7000,34,m18_bank,32,0xBBBBBBBB,3);
    o.close(0x70,0x7000); o.close(0x60,0x6000);
    const auto a=o.secondary(0x61,0x60);
    const auto b=o.secondary(0x71,0x70);
    assert(a.phase==stage::after_finish && a.bank_mask==1 && a.rx33_candidates==1);
    assert(b.phase==stage::after_finish && b.bank_mask==2 && b.rx34_candidates==1);
    assert(o.secondary(0x90,0x71).bank_mask==2);
    assert(o.secondary(0x90,0x61).bank_mask==1);
    o.clear();
    assert(o.secondary(0x90,0x61).phase==stage::none);
    std::cout << "PASS: lifecycle, independent deferred contexts, resource lifetime, fail-open\n";
}
