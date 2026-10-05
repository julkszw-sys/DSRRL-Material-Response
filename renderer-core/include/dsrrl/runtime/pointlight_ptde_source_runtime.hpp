#pragma once
#include "dsrrl/runtime/pointlight_ptde_source.hpp"
#include "dsrrl/runtime/pmetal_selector_policy.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::pointlight_ptde_source {

struct draw_bank_authority_cache_entry {
    std::uintptr_t param = 0u;
    std::uint16_t count = 0u;
    std::uint32_t first = 0u;
    const pointlight_donors::bank *bank = nullptr;
};

struct draw_bank_authority_cache {
    std::array<draw_bank_authority_cache_entry,4> entries{};
    std::uint8_t victim = 0u;
};

inline constexpr std::size_t
    k_bank_structure_snapshot_max = 0x10000u;
inline constexpr std::size_t
    k_bank_structure_entry_count = 64u;
inline constexpr std::size_t
    k_bank_structure_table_bytes =
        k_bank_structure_entry_count * 12u;
inline constexpr std::size_t
    k_bank_structure_name_max = 256u;

struct persistent_bank_name_guard {
    std::uint32_t offset = 0u;
    std::uint16_t length = 0u;
    std::array<std::uint8_t,
               k_bank_structure_name_max> bytes{};
};

struct persistent_bank_structure_cache_entry {
    std::uintptr_t param = 0u;
    std::uintptr_t allocation_base = 0u;
    std::uint16_t count = 0u;
    std::uint32_t first = 0u;
    std::uint32_t span = 0u;
    const pointlight_donors::bank *bank = nullptr;
    std::array<std::uint8_t,
               k_bank_structure_table_bytes> table_snapshot{};
    std::array<persistent_bank_name_guard,
               k_bank_structure_entry_count> name_guards{};
};

struct persistent_bank_structure_cache {
    std::array<persistent_bank_structure_cache_entry,4>
        entries{};
    std::uint8_t victim = 0u;
};

inline persistent_bank_structure_cache &
persistent_structure_cache() noexcept {
    thread_local persistent_bank_structure_cache cache{};
    return cache;
}

inline void clear_persistent_structure_cache() noexcept {
    persistent_structure_cache() = {};
}

inline bool readable_single_region(
    std::uintptr_t address,
    std::size_t size,
    std::uintptr_t &allocation_base) noexcept {
    allocation_base = 0u;
    if (!address ||
        size == 0u ||
        address + size < address)
        return false;

    MEMORY_BASIC_INFORMATION m{};
    if (VirtualQuery(
            reinterpret_cast<void *>(address),
            &m,
            sizeof(m)) != sizeof(m) ||
        m.State != MEM_COMMIT ||
        (m.Protect & PAGE_GUARD))
        return false;

    const auto p = m.Protect & 255u;
    if (p != PAGE_READONLY &&
        p != PAGE_READWRITE &&
        p != PAGE_WRITECOPY &&
        p != PAGE_EXECUTE_READ &&
        p != PAGE_EXECUTE_READWRITE &&
        p != PAGE_EXECUTE_WRITECOPY)
        return false;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            m.BaseAddress);
    const auto end = begin + m.RegionSize;
    if (end <= address ||
        end < begin ||
        address + size > end)
        return false;

    allocation_base =
        reinterpret_cast<std::uintptr_t>(
            m.AllocationBase);
    return allocation_base != 0u;
}

inline const pointlight_donors::bank *
lookup_persistent_structure_cache(
    std::uintptr_t param,
    std::uint16_t count,
    std::uint32_t first) noexcept {
    auto &cache = persistent_structure_cache();

    for (auto &entry : cache.entries) {
        if (entry.param != param ||
            entry.count != count ||
            entry.first != first ||
            entry.bank == nullptr ||
            entry.span == 0u ||
            entry.span >
                k_bank_structure_snapshot_max)
            continue;

        std::uintptr_t allocation_base = 0u;
        if (!readable_single_region(
                param + 0x30u,
                entry.span,
                allocation_base) ||
            allocation_base != entry.allocation_base) {
            entry = {};
            continue;
        }

        const auto table_bytes =
            static_cast<std::size_t>(count) * 12u;
        if (table_bytes !=
                k_bank_structure_table_bytes ||
            std::memcmp(
                reinterpret_cast<const void *>(
                    param + 0x30u),
                entry.table_snapshot.data(),
                table_bytes) != 0) {
            entry = {};
            continue;
        }

        bool names_match = true;
        for (std::size_t i = 0u;
             i < k_bank_structure_entry_count;
             ++i) {
            const auto &guard =
                entry.name_guards[i];
            if (guard.length == 0u ||
                guard.length >
                    k_bank_structure_name_max ||
                guard.offset < 0x30u ||
                static_cast<std::uint64_t>(
                    guard.offset) +
                    static_cast<std::uint64_t>(
                        guard.length) >
                    static_cast<std::uint64_t>(
                        0x30u + entry.span) ||
                std::memcmp(
                    reinterpret_cast<const void *>(
                        param + guard.offset),
                    guard.bytes.data(),
                    guard.length) != 0) {
                names_match = false;
                break;
            }
        }

        if (!names_match) {
            entry = {};
            continue;
        }

        return entry.bank;
    }

    return nullptr;
}

inline void store_persistent_structure_cache(
    std::uintptr_t param,
    std::uint16_t count,
    std::uint32_t first,
    std::uint32_t structure_end,
    const pointlight_donors::bank *bank) noexcept {
    if (bank == nullptr ||
        structure_end <= 0x30u)
        return;

    const auto span =
        static_cast<std::size_t>(
            structure_end - 0x30u);
    if (span == 0u ||
        span > k_bank_structure_snapshot_max)
        return;

    std::uintptr_t allocation_base = 0u;
    if (!readable_single_region(
            param + 0x30u,
            span,
            allocation_base))
        return;

    auto &cache = persistent_structure_cache();
    auto &entry =
        cache.entries[
            static_cast<std::size_t>(
                cache.victim++) %
            cache.entries.size()];

    entry = {};
    entry.param = param;
    entry.allocation_base = allocation_base;
    entry.count = count;
    entry.first = first;
    entry.span =
        static_cast<std::uint32_t>(span);
    entry.bank = bank;

    const auto table_bytes =
        static_cast<std::size_t>(count) * 12u;
    if (count !=
            k_bank_structure_entry_count ||
        table_bytes !=
            k_bank_structure_table_bytes) {
        entry = {};
        return;
    }

    std::memcpy(
        entry.table_snapshot.data(),
        reinterpret_cast<const void *>(
            param + 0x30u),
        table_bytes);

    const auto structure_limit =
        static_cast<std::uint64_t>(0x30u) +
        static_cast<std::uint64_t>(span);

    for (std::size_t i = 0u;
         i < k_bank_structure_entry_count;
         ++i) {
        std::uint32_t name_offset = 0u;
        std::memcpy(
            &name_offset,
            entry.table_snapshot.data() +
                i * 12u + 8u,
            sizeof(name_offset));

        if (name_offset < 0x30u ||
            static_cast<std::uint64_t>(
                name_offset) >=
                structure_limit) {
            entry = {};
            return;
        }

        const auto max_available =
            std::min<std::size_t>(
                k_bank_structure_name_max,
                static_cast<std::size_t>(
                    structure_limit -
                    static_cast<std::uint64_t>(
                        name_offset)));
        const auto *name =
            reinterpret_cast<const std::uint8_t *>(
                param + name_offset);

        std::size_t length = 0u;
        while (length < max_available &&
               name[length] != 0u)
            ++length;
        if (length >= max_available) {
            entry = {};
            return;
        }
        ++length; // include the NUL terminator in exact revalidation

        auto &guard =
            entry.name_guards[i];
        guard.offset = name_offset;
        guard.length =
            static_cast<std::uint16_t>(length);
        std::memcpy(
            guard.bytes.data(),
            name,
            length);
    }
}

inline bool readable(std::uintptr_t address,std::size_t size) noexcept {
    if(!address || address+size<address) return false;
    const auto end=address+size;
    while(address<end) {
        MEMORY_BASIC_INFORMATION m{};
        if(VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m))!=sizeof(m)||
           m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)) return false;
        const auto p=m.Protect&255u;
        if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
           p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto next=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=address) return false;
        address=std::min(next,end);
    }
    return true;
}
struct region_cache_entry { std::uintptr_t begin=0,end=0; };
struct access_cache {
    std::array<region_cache_entry,4> regions{};
    std::uint8_t victim=0;
};
inline bool readable_cached(std::uintptr_t address,std::size_t size,access_cache &cache) noexcept {
    if(!address || address+size<address) return false;
    const auto end=address+size;
    for(const auto &r:cache.regions)
        if(r.begin<=address && end<=r.end) return true;

    MEMORY_BASIC_INFORMATION m{};
    if(VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m))!=sizeof(m)||
       m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)) return false;
    const auto p=m.Protect&255u;
    if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
       p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY) return false;
    const auto begin=reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    const auto region_end=begin+m.RegionSize;
    if(region_end<=address || region_end<begin) return false;
    if(end>region_end) return readable(address,size);

    auto &slot=cache.regions[static_cast<std::size_t>(cache.victim++)%cache.regions.size()];
    slot={begin,region_end};
    return true;
}
template<class T> inline bool read_cached(std::uintptr_t address,T &out,access_cache &cache) noexcept {
    if(!readable_cached(address,sizeof(T),cache)) return false;
    std::memcpy(&out,reinterpret_cast<void*>(address),sizeof(T)); return true;
}

inline std::size_t readable_cached_prefix(
    std::uintptr_t address,
    std::size_t max_size,
    access_cache &cache) noexcept {
    if(!address || max_size==0u) return 0u;

    for(const auto &r:cache.regions) {
        if(r.begin<=address && address<r.end)
            return std::min<std::size_t>(
                max_size,
                static_cast<std::size_t>(
                    r.end-address));
    }

    MEMORY_BASIC_INFORMATION m{};
    if(VirtualQuery(
           reinterpret_cast<void*>(address),
           &m,
           sizeof(m))!=sizeof(m) ||
       m.State!=MEM_COMMIT ||
       (m.Protect&PAGE_GUARD))
        return 0u;

    const auto p=m.Protect&255u;
    if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
       p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY)
        return 0u;

    const auto begin=
        reinterpret_cast<std::uintptr_t>(
            m.BaseAddress);
    const auto region_end=
        begin+m.RegionSize;
    if(region_end<=address || region_end<begin)
        return 0u;

    auto &slot=
        cache.regions[
            static_cast<std::size_t>(
                cache.victim++)%
            cache.regions.size()];
    slot={begin,region_end};

    return std::min<std::size_t>(
        max_size,
        static_cast<std::size_t>(
            region_end-address));
}

inline void structure_hash_byte(
    std::uint64_t &hash,
    std::uint8_t value) noexcept {
    hash ^= value;
    hash *= 0x100000001b3ULL;
}

inline bool bank_structure_signature(
    std::uintptr_t param,
    std::uint16_t count,
    std::uint32_t first,
    access_cache &cache,
    std::uint64_t &signature,
    std::uint32_t *structure_end_out = nullptr) noexcept {
    if (count != 64u ||
        first < 0x330u ||
        first > 0x10000u ||
        !readable_cached(
            param,
            0x30u + static_cast<std::size_t>(count) * 12u,
            cache))
        return false;

    std::uint64_t hash = 0xcbf29ce484222325ULL;
    structure_hash_byte(hash, static_cast<std::uint8_t>(count & 0xffu));
    structure_hash_byte(hash, static_cast<std::uint8_t>((count >> 8u) & 0xffu));

    const auto table_end =
        0x30u + static_cast<std::uint32_t>(count) * 12u;
    std::uint32_t structure_end = table_end;

    for (std::uint32_t i = 0u; i < count; ++i) {
        const auto entry = param + 0x30u + static_cast<std::uintptr_t>(i) * 12u;
        std::uint32_t id = 0u;
        std::uint32_t row_offset = 0u;
        std::uint32_t name_offset = 0u;
        std::memcpy(&id, reinterpret_cast<const void *>(entry), 4u);
        std::memcpy(&row_offset, reinterpret_cast<const void *>(entry + 4u), 4u);
        std::memcpy(&name_offset, reinterpret_cast<const void *>(entry + 8u), 4u);

        if (id != i ||
            row_offset != first + 16u * i ||
            name_offset < table_end ||
            name_offset > 0x100000u)
            return false;

        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            structure_hash_byte(
                hash,
                static_cast<std::uint8_t>((id >> shift) & 0xffu));

        bool terminated = false;
        const auto name_address =
            param + name_offset;
        const auto direct_bytes =
            readable_cached_prefix(
                name_address,
                256u,
                cache);
        if (direct_bytes == 0u)
            return false;

        const auto *name =
            reinterpret_cast<const std::uint8_t *>(
                name_address);
        std::uint32_t consumed = 0u;
        for (; consumed < direct_bytes; ++consumed) {
            const auto value = name[consumed];
            structure_hash_byte(hash, value);
            if (value == 0u) {
                terminated = true;
                break;
            }
        }

        // A valid name almost always terminates inside the same committed
        // region. Preserve the exact old fail-open semantics across a region
        // boundary rather than assuming contiguous readability.
        if (!terminated) {
            for (std::uint32_t j = consumed;
                 j < 256u;
                 ++j) {
                std::uint8_t value = 0u;
                if (!read_cached(
                        name_address + j,
                        value,
                        cache))
                    return false;
                structure_hash_byte(hash, value);
                if (value == 0u) {
                    consumed = j;
                    terminated = true;
                    break;
                }
            }
        }
        if (!terminated)
            return false;

        const auto name_end64 =
            static_cast<std::uint64_t>(
                name_offset) +
            static_cast<std::uint64_t>(
                consumed) +
            1u;
        if (name_end64 >
            static_cast<std::uint64_t>(
                0xffffffffu))
            return false;
        structure_end =
            std::max(
                structure_end,
                static_cast<std::uint32_t>(
                    name_end64));
    }

    signature = hash;
    if (structure_end_out != nullptr)
        *structure_end_out = structure_end;
    return true;
}
inline bool donor(
    std::uintptr_t source,
    std::int32_t selector,
    signal &out,
    access_cache &cache,
    draw_bank_authority_cache &authority_cache) noexcept {
    if(selector<0) return false;
    std::uintptr_t param=0;
    if(!read_cached(source+0x18u,param,cache)||!readable_cached(param,0x330u,cache)) return false;
    const auto *p=reinterpret_cast<const std::uint8_t*>(param);
    std::uint16_t count=0; std::uint32_t first=0;
    std::memcpy(&count,p+0xau,2); std::memcpy(&first,p+0x34u,4);

    const pointlight_donors::bank *bank=nullptr;
    for(const auto &entry:authority_cache.entries) {
        if(entry.param==param &&
           entry.count==count &&
           entry.first==first &&
           entry.bank!=nullptr) {
            bank=entry.bank;
            break;
        }
    }

    if(bank==nullptr)
        bank =
            lookup_persistent_structure_cache(
                param,
                count,
                first);

    if(bank==nullptr) {
        std::uint64_t structure_signature=0u;
        std::uint32_t structure_end=0u;
        if(!bank_structure_signature(
                param,
                count,
                first,
                cache,
                structure_signature,
                &structure_end))
            return false;

        bank=identify_structure(structure_signature);
        if(!bank) return false;

        store_persistent_structure_cache(
            param,
            count,
            first,
            structure_end,
            bank);
    }

    auto &entry=
        authority_cache.entries[
            static_cast<std::size_t>(
                authority_cache.victim++)%
            authority_cache.entries.size()];
    entry={param,count,first,bank};

    const auto index=static_cast<std::uint32_t>(selector)&255u;
    if(index>=64u||!readable_cached(param+first,1024u,cache)) return false;

    out=decode(bank->ptde[index]); return true;
}

inline bool donor(
    std::uintptr_t source,
    std::int32_t selector,
    signal &out,
    access_cache &cache) noexcept {
    draw_bank_authority_cache authority_cache{};
    return donor(
        source,
        selector,
        out,
        cache,
        authority_cache);
}
inline bool selected_source(std::uintptr_t manager,std::int16_t selector,std::uintptr_t &source,access_cache &cache) noexcept {
    const auto area=selector<0?0xffffffffu:(static_cast<unsigned>(selector)>>8u)&127u;
    auto lookup=[&](unsigned a) noexcept {
        std::uintptr_t table=0;
        return read_cached(manager+0x20u+a*0x1b0u,table,cache)&&table&&
               read_cached(table+9u*0x10u+8u,source,cache);
    };
    source=0;
    if(area<=11u&&!lookup(area)) return false;
    if(!source&&!lookup(11u)) return false;
    return source!=0;
}
// Caller must have attested the retail EXE. No hook, host write, retained pointer,
// source-category guess, or global latest-source publication is used here.
inline bool capture(
    void *node,
    std::uintptr_t base,
    std::array<float,8> &raw,
    draw_bank_authority_cache &authority_cache) noexcept {
    const auto n=reinterpret_cast<std::uintptr_t>(node);
    access_cache cache{};
    std::uintptr_t vt=0,fn=0,owner=0;
    if(!base||!read_cached(n,vt,cache)||!read_cached(vt+0x60u,fn,cache)||
       !read_cached(n+0x50u,owner,cache)||!owner) return false;
    signal a,b,result;
    if(fn==base+0x55bc00u) {
        std::int32_t selector=-1;
        if(!read_cached(n+0x58u,selector,cache)||!donor(owner,selector,a,cache,authority_cache)||!mix(a,a,0,result)) return false;
    } else if(fn==base+0x55d0b0u) {
        std::int16_t sa=-1,sb=-1; float beta=0;
        if(!read_cached(n+0x58u,sa,cache)||!read_cached(n+0x5au,sb,cache)||
           !read_cached(n+0x5cu,beta,cache)) return false;
        const auto pair=pmetal_selector_policy::select(sa,sb,beta);
        if(!pair.valid) return false;
        std::uintptr_t source_a=0,source_b=0;
        if(!selected_source(owner,pair.a,source_a,cache)||!donor(source_a,pair.a,a,cache,authority_cache)) return false;
        b=a;
        if(pair.beta!=0&&(!selected_source(owner,pair.b,source_b,cache)||!donor(source_b,pair.b,b,cache,authority_cache))) return false;
        if(!mix(a,b,pair.beta,result)) return false;
    } else return false;
    // Preserve host world position. Only the operator-local payload changes.
    raw[3]=1.0f/(result.end-result.begin);
    raw[4]=result.q[0];raw[5]=result.q[1];raw[6]=result.q[2];raw[7]=result.end;
    return std::isfinite(raw[3]);
}

inline bool capture(
    void *node,
    std::uintptr_t base,
    std::array<float,8> &raw) noexcept {
    draw_bank_authority_cache authority_cache{};
    return capture(
        node,
        base,
        raw,
        authority_cache);
}
} // namespace dsrrl::runtime::pointlight_ptde_source
