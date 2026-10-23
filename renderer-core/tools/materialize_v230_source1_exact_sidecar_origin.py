#!/usr/bin/env python3
"""Read-only exact Source1 RX34 native-name PTDE sidecar-origin audit.

Apply after V230 stock-identity transport and V13 materializer. No new DDS
authorization, runtime view binding, source routing or render-state mutation.
"""
from pathlib import Path

p=Path("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
s=p.read_text(encoding="utf-8")
anchor="""    } catch (...) {
        return;
    }

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());"""
replacement="""    } catch (...) {
        return;
    }

#if defined(DSRRL_V230_SOURCE1_SIDECAR_ORIGIN)
    // Exact owner-log SRV hashes, Quelaag RX34 route345 material slot2.
    // Presence of a hash alone DOES NOT authorize PTDE replacement.
    constexpr std::uint64_t h_diff=0x07897dbf4ed2fd72ULL;
    constexpr std::uint64_t h_spec=0x236adf2eb0a2afccULL;
    constexpr std::uint64_t h_norm=0x236af42eb0a2d37bULL;
    const unsigned cls_idx=
        logical_hash==h_diff?0u:
        logical_hash==h_spec?1u:
        logical_hash==h_norm?2u:255u;
    if(cls_idx!=255u) {
        static std::atomic<std::uint32_t> count{0u};
        const auto n=count.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=24u) {
            const auto cls=cls_idx==0u?asset_class::diffuse:
                cls_idx==1u?asset_class::specular:asset_class::normal;
            const bool allowed=cls_idx==0u?diffuse_member:
                cls_idx==1u?spec_member:normal_member;
            std::error_code io_error;
            const auto dds=sidecar_path(cls,logical_name);
            const bool exists=std::filesystem::is_regular_file(dds,io_error);
            std::uintmax_t size=0u;
            if(exists&&!io_error)
                size=std::filesystem::file_size(dds,io_error);
            char ascii_name[100]{};
            const auto len=std::min<std::size_t>(
                logical_name.size(),sizeof(ascii_name)-1u);
            for(std::size_t j=0;j<len;++j) {
                const auto c=static_cast<unsigned>(logical_name[j]);
                ascii_name[j]=(c>=32u&&c<=126u)?static_cast<char>(c):'?';
            }
            char log[384]{};
            std::snprintf(log,sizeof(log),
                "[DSRRL SOURCE1 SIDECAR ORIGIN] n=%u cls=%u hash=%016llx "
                "name=%s target_gate=%u exact_dds_exists=%u dds_bytes=%llu "
                "io_error=%u native_view=%016llx",
                n,cls_idx,static_cast<unsigned long long>(logical_hash),
                ascii_name,allowed?1u:0u,exists?1u:0u,
                static_cast<unsigned long long>(size),io_error?1u:0u,
                static_cast<unsigned long long>(view.handle));
            reshade::log::message(reshade::log::level::warning,log);
        }
    }
#endif

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());"""
assert s.count(anchor)==1,f"SOURCE1_ORIGIN_SOURCE_DRIFT {s.count(anchor)}"
p.write_text(s.replace(anchor,replacement,1),encoding="utf-8")
print("V230_SOURCE1_SIDECAR_ORIGIN_PASS")
