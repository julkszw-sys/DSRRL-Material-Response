#include "dsrrl/runtime/pmetal_producer_state.hpp"
#include <cassert>
#include <cstdint>
using namespace dsrrl;
using namespace dsrrl::runtime;
int main() {
    operators::material_response::material_identity actor{};
    actor.valid=true;
    actor.owner_tuple_exact=true;
    actor.model_owner_instance=0x219a0d00u;
    pmetal_envspec_source endpoints{};
    endpoints.row_id_a=31u;
    endpoints.row_id_b=30u;
    endpoints.a={0.4f,0.4f,0.4f};
    endpoints.b={1.2f,0.8f,0.65882355f};
    endpoints.envdiffuse_a=endpoints.a;
    endpoints.envdiffuse_b=endpoints.b;
    endpoints.envdiffuse_linear_valid=true;
    endpoints.beta=0.5f;
    endpoints.native_producer_sequence=123u;
    endpoints.bank_signature_a=6u;
    endpoints.bank_signature_b=6u;
    constexpr auto epoch=77u;
    auto *source_a=reinterpret_cast<void*>(0x22334000u);
    auto *source_b=reinterpret_cast<void*>(0x22334000u);
    pmetal_producer_state_begin(actor,epoch,source_a,1311,source_b,1310,0.5f);
    pmetal_producer_state_publish(actor,endpoints,epoch);
    pmetal_envspec_source out{};
    assert(pmetal_producer_state_latest(actor,epoch,out));
    assert(out.beta==0.5f && out.a[0]==0.4f && out.b[0]==1.2f);
    auto gen=out.generation;
    // No new retail packer event yet. Exact endpoint witnesses remain
    // valid; beta is a separate, actor-authenticated semantic signal.
    pmetal_producer_state_begin(actor,epoch,source_a,1311,source_b,1310,0.75f);
    assert(pmetal_producer_state_latest(actor,epoch,out));
    assert(out.beta==0.75f && out.generation>gen);
    // Scalar-only beta continuity cannot change which native event supplied
    // the actor-certified endpoint payload.
    assert(out.native_producer_sequence==123u);
    assert(out.row_id_a==31u && out.row_id_b==30u);
    assert(out.a[0]==0.4f && out.b[0]==1.2f);
    auto other=actor;
    other.model_owner_instance=0x219a4d80u;
    assert(!pmetal_producer_state_latest(other,epoch,out));
    // Same exact actor across MTD and FLVER slot.
    other=actor;other.material_slot=12u;
    other.raw_mtd_sha256[0]=35u;
    assert(pmetal_producer_state_latest(other,epoch,out));
    assert(out.beta==0.75f);
    // A true endpoint selector change MUST still invalidate.
    pmetal_producer_state_begin(actor,epoch,source_a,1311,source_b,1309,0.76f);
    assert(!pmetal_producer_state_latest(actor,epoch,out));
    // Re-certify the new pair and do not mix row30 payload with row29.
    endpoints.row_id_b=29u;
    endpoints.beta=0.76f;
    pmetal_producer_state_publish(actor,endpoints,epoch);
    assert(pmetal_producer_state_latest(actor,epoch,out));
    assert(out.row_id_b==29u);
    // New native bank pointer must not reuse previous A/B.
    pmetal_producer_state_begin(actor,epoch,source_a,1311,
        reinterpret_cast<void*>(0x998877u),1309,0.77f);
    assert(!pmetal_producer_state_latest(actor,epoch,out));
    // Genuine zero endpoints and row0 remain legal.
    pmetal_producer_state_begin(actor,epoch,source_a,1280,source_b,1281,0.4f);
    endpoints.row_id_a=0u;endpoints.row_id_b=1u;
    endpoints.a={0,0,0};endpoints.b={0,0,0};endpoints.beta=0.4f;
    pmetal_producer_state_publish(actor,endpoints,epoch);
    pmetal_producer_state_begin(actor,epoch,source_a,1280,source_b,1281,0.5f);
    assert(pmetal_producer_state_latest(actor,epoch,out));
    assert(out.beta==0.5f && out.a[0]==0.0f);
    return 0;
}
