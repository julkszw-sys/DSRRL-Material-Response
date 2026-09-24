#include "dsrrl/runtime/subsurface_dispatch.hpp"
#include "ptde_material_donor_registry.hpp"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<#x<<"\n";return 1;}}while(false)
int main(){
    using namespace dsrrl::runtime;
    namespace s=dsrrl::operators::resource_bridges;
    for(const auto &r:s::k_subsurface_receiver_routes){
        CHECK(subsurface_target(r.dsr_receiver_sha256)==static_cast<int>(r.target_plain_receiver_id));
        // Ordinary receiver identity must never authorize the Subsurf path.
        CHECK(subsurface_target(r.target_plain_receiver_sha256)==-1);
        for(unsigned bits=0;bits<16;++bits)
            CHECK(subsurface_dispatch_ready(static_cast<int>(r.target_plain_receiver_id),
                bits&1,bits&2,bits&4,bits&8)==(bits==15));
    }
    CHECK(subsurface_target("unknown")==-1);
    CHECK(!subsurface_dispatch_ready(32,true,true,true,true));
    CHECK(!subsurface_dispatch_ready(36,true,true,true,true));
    const int donor=dsrrl::materialdonor::find_sha256(s::k_ptde_body_plain_material_sha256);
    CHECK(donor>=0);
    const auto &d=dsrrl::materialdonor::k_donors[static_cast<unsigned>(donor)];
    CHECK(d.has_c101 && d.c101_ptde[0]==1.f && d.c101_ptde[1]==1.f && d.c101_ptde[2]==1.f);
    std::cout<<"subsurface_dispatch_tests: PASS\n";
}
