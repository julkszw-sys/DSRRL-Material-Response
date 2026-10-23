#!/usr/bin/env python3
"""V230 resource-view identity and COM lock-safety repair.

Preserve existing owner/sidecar/receiver authorization. Reject a native view
whose logical texture identity differs even if its 64-bit name hash agrees.
Drop speculative duplicate sidecar SRVs AFTER unlocking g_mutex, because COM
Release can synchronously trigger ReShade resource-view callbacks.
"""
from pathlib import Path
p=Path("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
s=p.read_text("utf-8")
def one(old,new):
    global s
    if s.count(old)!=1:
        raise RuntimeError(f"SIDECAR_CACHE_SAFETY_DRIFT expected=1 found={s.count(old)}")
    s=s.replace(old,new,1)

one("""            std::lock_guard<std::mutex> lock(g_mutex);
            const auto it = g_sidecar_shared.find(key);
            if (it != g_sidecar_shared.end() &&
                it->second.view != nullptr) {
                it->second.view->AddRef();
                ++it->second.live_stock_users;
                ID3D11ShaderResourceView *winner = it->second.view;
                loaded.view->Release();""",
"""            std::unique_lock<std::mutex> lock(g_mutex);
            const auto it = g_sidecar_shared.find(key);
            if (it != g_sidecar_shared.end() &&
                it->second.view != nullptr) {
                it->second.view->AddRef();
                ++it->second.live_stock_users;
                ID3D11ShaderResourceView *winner = it->second.view;
                // Never call COM Release under g_mutex: callback reentrancy.
                lock.unlock();
                loaded.view->Release();""")

one("found->second.logical_hash != logical_hash) {",
"""(found->second.logical_hash != logical_hash ||
                 _wcsicmp(found->second.logical_name.c_str(),
                          set.logical_name.c_str()) != 0)) {""")

p.write_text(s,"utf-8")
print("V230_SIDECAR_CACHE_SAFETY_PASS native_view_exact_name_and_com_release_unlock")
