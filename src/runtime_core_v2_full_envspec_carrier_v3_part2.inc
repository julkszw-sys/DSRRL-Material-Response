    std::array<std::uint8_t,32> digest{};
    if (!sha256_bytes(out.data(), out.size(), digest) || hex_string(digest.data(), digest.size()) != expected_sha) {
        out.clear(); return false;
    }
    return true;
}

template <typename T>
bool read_scalar(const std::uint8_t *&p, const std::uint8_t *end, T &out)
{
    if (static_cast<std::size_t>(end - p) < sizeof(T)) return false;
    std::memcpy(&out, p, sizeof(T));
    p += sizeof(T);
    return true;
}

bool load_router_sidecars(HMODULE addon_module)
{
    std::filesystem::path root = module_path(addon_module);
    if (root.empty()) root = std::filesystem::current_path();
    else root = root.parent_path();
    root /= L"DSRRL"; root /= L"EnvSpec"; root /= L"FullBridge";

    std::vector<std::uint8_t> material_bytes;
    std::vector<std::uint8_t> native_bytes;
    if (!read_verified_file(root / L"MATERIAL_ENVSPEC_ROUTER_V1.bin",
                            k_expected_material_router_sha256, material_bytes)) return false;
    if (!read_verified_file(root / L"DSR_NATIVE_ENVSPEC_PROBE_HASH_V1.bin",
                            k_expected_native_probe_sha256, native_bytes)) return false;

    auto parse_header = [](const std::vector<std::uint8_t> &bytes, const char magic[8],
                           std::uint32_t expected_count, std::uint32_t expected_stride,
                           const std::uint8_t *&records) -> bool {
        if (bytes.size() < 24) return false;
        if (std::memcmp(bytes.data(), magic, 8) != 0) return false;
        const std::uint8_t *p = bytes.data() + 8;
        const std::uint8_t *end = bytes.data() + bytes.size();
        std::uint32_t version=0,count=0,stride=0,reserved=0;
        if (!read_scalar(p,end,version) || !read_scalar(p,end,count) ||
            !read_scalar(p,end,stride) || !read_scalar(p,end,reserved)) return false;
        if (version != 1 || count != expected_count || stride != expected_stride || reserved != 0) return false;
        if (bytes.size() != 24ull + static_cast<std::uint64_t>(count) * stride) return false;
        records = p;
        return true;
    };

    const std::uint8_t *mr = nullptr;
    const std::uint8_t *nr = nullptr;
    if (!parse_header(material_bytes, "DSREMR01", 325, 72, mr)) return false;
    if (!parse_header(native_bytes, "DSRENP01", 342, 36, nr)) return false;

    g_material_registry.clear();
    g_material_registry.reserve(325);
    for (std::uint32_t i = 0; i < 325; ++i) {
        const std::uint8_t *p = mr + static_cast<std::size_t>(i) * 72;
        const std::uint8_t *end = p + 72;
        material_record r{};
        std::uint8_t state=0,slot=0,safe=0,reserved=0;
        if (!read_scalar(p,end,r.name_hash)) return false;
        std::memcpy(r.raw_sha.data(), p, 32); p += 32;
        if (!read_scalar(p,end,state) || !read_scalar(p,end,slot) ||
            !read_scalar(p,end,safe) || !read_scalar(p,end,reserved)) return false;
        if (state > 3 || slot > 3 || safe > 1 || reserved != 0) return false;
        r.state = static_cast<material_envspec_state>(state); r.slot = slot; r.explicit_none_safe = safe != 0;
        for (float &v : r.c101) if (!read_scalar(p,end,v)) return false;
        if (!read_scalar(p,end,r.c102)) return false;
        for (float &v : r.c100) if (!read_scalar(p,end,v)) return false;
        if (p != end) return false;
        g_material_registry.push_back(r);
    }

    g_native_probe_registry.clear();
    g_native_probe_registry.reserve(342);
    for (std::uint32_t i = 0; i < 342; ++i) {
        const std::uint8_t *p = nr + static_cast<std::size_t>(i) * 36;
        native_probe_hash_record r{};
        std::memcpy(r.sha.data(), p, 32); p += 32;
        std::uint16_t reserved = 0;
        std::memcpy(&r.probe_index, p, 2); p += 2;
        std::memcpy(&reserved, p, 2);
        if (r.probe_index >= k_probe_count || reserved != 0) return false;
        g_native_probe_registry.push_back(r);
    }
    return true;
}


std::uint64_t fnv1a_utf16_basename_lower_ascii(const wchar_t *s) noexcept
{
    if (s == nullptr) return 0;
    const wchar_t *base = s;
    std::size_t length = 0;
    for (; length < 512; ++length) {
        const wchar_t c = s[length];
        if (c == 0) break;
        if (c == L'\\' || c == L'/') base = s + length + 1;
    }
    if (length == 512) return 0;

    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    for (const wchar_t *p = base; *p != 0; ++p) {
        wchar_t c = *p;
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c + (L'a' - L'A'));
        const std::uint16_t u = static_cast<std::uint16_t>(c);
        hash ^= static_cast<std::uint8_t>(u & 0xFFu); hash *= prime;
        hash ^= static_cast<std::uint8_t>((u >> 8) & 0xFFu); hash *= prime;
    }
    return hash;
}

bool material_response_equivalent(const material_record &a, const material_record &b) noexcept
{
    return a.state == b.state && a.slot == b.slot &&
           a.explicit_none_safe == b.explicit_none_safe &&
           std::memcmp(a.c101, b.c101, sizeof(a.c101)) == 0 &&
           std::memcmp(&a.c102, &b.c102, sizeof(a.c102)) == 0 &&
           std::memcmp(a.c100, b.c100, sizeof(a.c100)) == 0;
}

const material_record *find_material(std::uint64_t name_hash, const std::array<std::uint8_t,32> &sha) noexcept
{
    const material_record *first = nullptr;
    std::uint32_t matches = 0;
    bool response_equivalent = true;

    for (const auto &r : g_material_registry) {
        if (r.raw_sha != sha) continue;
        if (r.name_hash == name_hash && name_hash != 0) return &r;
        if (first == nullptr) first = &r;
        else if (!material_response_equivalent(*first, r)) response_equivalent = false;
        ++matches;
    }

    // Raw MTD SHA is the primary identity. Semantic key is only required when
    // one raw SHA aliases materially different PTDE responses.
    if (matches == 1 || (matches > 1 && response_equivalent)) return first;
    return nullptr;
}

std::optional<std::uint16_t> find_native_probe(const std::array<std::uint8_t,32> &sha) noexcept
{
    for (const auto &r : g_native_probe_registry)
        if (r.sha == sha)
            return r.probe_index;
    return std::nullopt;
}

bool classify_receiver(const void *code, std::size_t code_size, std::uint64_t,
                       std::uint32_t *receiver_id, std::uint64_t *consumer_family_hash)
{
    if (code == nullptr || code_size < 20) return false;
    const auto *b = static_cast<const std::uint8_t *>(code);
    if (std::memcmp(b, "DXBC", 4) != 0) return false;
    for (const auto &r : k_receivers) {
        if (r.code_size != code_size) continue;
        if (std::memcmp(b + 4, r.dxbc_checksum.data(), r.dxbc_checksum.size()) != 0) continue;
        if (receiver_id != nullptr) *receiver_id = r.shader_index;
        if (consumer_family_hash != nullptr) *consumer_family_hash = r.lerp ? 0x48454D4C45525001ull : 0x48454D454E560001ull;
        return true;
    }
    return false;
}

void write_abs_jump(std::uint8_t *dst, const void *target)
{
    dst[0] = 0xFF; dst[1] = 0x25;
    dst[2] = dst[3] = dst[4] = dst[5] = 0;
    const std::uint64_t address = reinterpret_cast<std::uint64_t>(target);
    std::memcpy(dst + 6, &address, sizeof(address));
}

bool install_detour(detour_state &state, void *target, const void *hook,
                    const std::array<std::uint8_t,k_detour_stolen> &expected)
{
    if (target == nullptr || hook == nullptr || std::memcmp(target, expected.data(), expected.size()) != 0)
        return false;
    state.target = target;
    std::memcpy(state.original.data(), target, state.original.size());
    auto *trampoline = static_cast<std::uint8_t *>(VirtualAlloc(nullptr, k_detour_stolen + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr) return false;
    std::memcpy(trampoline, state.original.data(), k_detour_stolen);
    write_abs_jump(trampoline + k_detour_stolen,
        static_cast<std::uint8_t *>(target) + k_detour_stolen);
    DWORD old_protect = 0;
    if (!VirtualProtect(target, k_detour_stolen, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }
    std::array<std::uint8_t,k_detour_stolen> patch{};
    patch.fill(0x90);
    write_abs_jump(patch.data(), hook);
    std::memcpy(target, patch.data(), patch.size());
    FlushInstructionCache(GetCurrentProcess(), target, patch.size());
    DWORD ignored = 0;
    VirtualProtect(target, k_detour_stolen, old_protect, &ignored);
    state.trampoline = trampoline;
    state.installed = true;
    return true;
}

void uninstall_detour(detour_state &state)
{
    if (!state.installed || state.target == nullptr) return;
    DWORD old_protect = 0;
    if (VirtualProtect(state.target, k_detour_stolen, PAGE_EXECUTE_READWRITE, &old_protect)) {
        std::memcpy(state.target, state.original.data(), state.original.size());
        FlushInstructionCache(GetCurrentProcess(), state.target, state.original.size());
        DWORD ignored = 0;
        VirtualProtect(state.target, k_detour_stolen, old_protect, &ignored);
    }
    if (state.trampoline != nullptr) VirtualFree(state.trampoline, 0, MEM_RELEASE);
    state = {};
}

void __fastcall parser_hook(void *material, const void *raw, std::uint32_t size, const wchar_t *semantic_key)
{
    ++g_parser_seen;
    material_binding binding{};
    if (material != nullptr && raw != nullptr && size != 0 && size <= (1u << 20)) {
        std::array<std::uint8_t,32> sha{};
        const std::uint64_t name_hash = fnv1a_utf16_basename_lower_ascii(semantic_key);
        if (sha256_bytes(raw, size, sha)) {
            binding.record = find_material(name_hash, sha);
            if (binding.record != nullptr && binding.record->state == material_envspec_state::explicit_none)
                binding.explicit_none_safe = binding.record->explicit_none_safe;
        }
    }
    g_parser_original(material, raw, size, semantic_key);
    if (material != nullptr) {
        std::lock_guard lock(g_mutex);
        if (binding.record != nullptr) {
            g_materials[reinterpret_cast<std::uintptr_t>(material)] = binding;
            ++g_material_mapped;
        } else {
            g_materials.erase(reinterpret_cast<std::uintptr_t>(material));
            ++g_material_unmapped;
        }
    }
}

std::uintptr_t __fastcall selector_hook(
    void *container, void *arg2, std::int32_t material_index, std::uint32_t arg4,
    std::uint64_t arg5, std::uint64_t arg6, std::uint64_t arg7,
    std::uint64_t arg8, std::uint64_t arg9, std::uint64_t arg10)
{
    ++g_selector_seen;
    std::uintptr_t actual = 0;
    if (container != nullptr && material_index >= 0) {
        __try {
            const auto base = *reinterpret_cast<const std::uintptr_t *>(
                static_cast<const std::uint8_t *>(container) + 0x10);
            if (base != 0) actual = *reinterpret_cast<const std::uintptr_t *>(
                base + static_cast<std::uintptr_t>(material_index) * 24u);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            actual = 0;
        }
    }
    if (actual != 0) ++g_selector_resolved;
    // The selector submits PS state and returns before the caller issues the draw.
    // Keep one exact material token pending until the next draw callback consumes it.
    g_active_material = actual;
    return g_selector_original(
        container, arg2, material_index, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
}

bool verify_and_install_hooks()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) return false;
    std::array<std::uint8_t,32> digest{};
    const auto path = module_path(exe);
    if (path.empty() || !sha256_file(path, digest) ||
        hex_string(digest.data(), digest.size()) != k_expected_exe_sha256) {
        ++g_exe_guard_fail;
        return false;
    }
    ++g_exe_guard_pass;
    auto *base = reinterpret_cast<std::uint8_t *>(exe);
    if (!install_detour(g_parser_detour, base + k_parser_rva,
                        reinterpret_cast<const void *>(&parser_hook), k_parser_prolog)) {
        ++g_hook_install_fail;
        return false;
    }
    g_parser_original = reinterpret_cast<parser_fn>(g_parser_detour.trampoline);
    if (!install_detour(g_selector_detour, base + k_selector_rva,
                        reinterpret_cast<const void *>(&selector_hook), k_selector_prolog)) {
        uninstall_detour(g_parser_detour);
        g_parser_original = nullptr;
        ++g_hook_install_fail;
        return false;
    }
    g_selector_original = reinterpret_cast<selector_fn>(g_selector_detour.trampoline);
    ++g_hook_install_pass;
    return true;
}

bool is_native_envspec_desc(const reshade::api::resource_desc &desc) noexcept
{
    if (desc.type != reshade::api::resource_type::texture_2d ||
        desc.texture.width != k_dsr_size || desc.texture.height != k_dsr_size ||
        desc.texture.depth_or_layers != k_faces || desc.texture.levels != k_dsr_levels ||
        desc.texture.samples != 1) return false;
    return desc.texture.format == reshade::api::format::bc6h_typeless ||
           desc.texture.format == reshade::api::format::bc6h_ufloat ||
           desc.texture.format == reshade::api::format::bc6h_sfloat;
}

bool extract_tight_bc6h(const reshade::api::resource_desc &desc,
                        const reshade::api::subresource_data *initial_data,
                        std::vector<std::uint8_t> &out)
{
    if (!is_native_envspec_desc(desc) || initial_data == nullptr) return false;
    out.clear(); out.reserve(k_dsr_tight_bc6h_bytes);
    for (std::uint32_t face = 0; face < k_faces; ++face) {
        for (std::uint32_t level = 0; level < k_dsr_levels; ++level) {
            const std::uint32_t sub = face * k_dsr_levels + level;
