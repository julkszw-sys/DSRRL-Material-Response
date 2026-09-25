#pragma once
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
namespace dsrrl::runtime {
using spec_rgb_resource_handle=std::uint64_t;
enum class spec_rgb_resource_result:std::uint8_t{registered=0,already_registered,fail_open_invalid_identity,fail_open_invalid_resource,fail_open_ambiguous_identity,fail_open_conflicting_stock_binding,fail_open_conflicting_companion};
struct spec_rgb_resource_record{std::u16string logical_name;spec_rgb_resource_handle stock_t1_srv=0;spec_rgb_resource_handle ptde_t10_srv=0;};
// The live carrier is a draw-local transaction. The request snapshots both the
// stock DSR t1 owner and the pre-existing host t10. A successful plan replaces
// only t10 and carries the exact host t10 handle back to the caller for
// unconditional post-draw restore/verification. Construction success is not
// pixel equivalence; an incomplete identity/resource route remains fail-open.
struct spec_rgb_bind_request{operators::resource_bridges::spec_rgb_context route{};std::u16string logical_name;spec_rgb_resource_handle currently_bound_t1=0;spec_rgb_resource_handle currently_bound_t10=0;};
struct spec_rgb_bind_plan{operators::resource_bridges::spec_rgb_decision route{};bool activate=false;spec_rgb_resource_handle preserved_t1_srv=0;spec_rgb_resource_handle preserved_t10_srv=0;spec_rgb_resource_handle ptde_t10_srv=0;};
class spec_rgb_resource_cache{public:spec_rgb_resource_result register_exact(const std::u16string&,spec_rgb_resource_handle,spec_rgb_resource_handle);spec_rgb_bind_plan plan_bind(const spec_rgb_bind_request&) const;void erase_stock(spec_rgb_resource_handle);void erase_companion(spec_rgb_resource_handle);void clear() noexcept;std::size_t size() const noexcept;private:struct entry{std::u16string logical_name;spec_rgb_resource_handle stock_t1_srv=0;spec_rgb_resource_handle ptde_t10_srv=0;bool ambiguous=false;};mutable std::mutex mutex_;std::unordered_map<std::u16string,entry> by_name_;std::unordered_map<spec_rgb_resource_handle,std::u16string> name_by_stock_;std::unordered_map<spec_rgb_resource_handle,std::u16string> name_by_companion_;std::unordered_set<spec_rgb_resource_handle> ambiguous_stock_;};
} // namespace dsrrl::runtime
