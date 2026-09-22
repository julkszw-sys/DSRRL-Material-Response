}

bool push_srv_binding(reshade::api::command_list *cmd, std::uint32_t slot,
                      const srv_binding &binding, reshade::api::resource_view view,
                      bool exact_sampler)
{
    if (!binding.valid) return false;
    reshade::api::descriptor_table_update u{};
    u.binding = slot; u.array_offset = 0; u.count = 1; u.type = binding.type;
    g_internal_descriptor_push = true;
    if (binding.type == reshade::api::descriptor_type::shader_resource_view) {
        u.descriptors = &view;
        cmd->push_descriptors(reshade::api::shader_stage::pixel, binding.layout, binding.param_index, u);
    } else if (binding.type == reshade::api::descriptor_type::sampler_with_resource_view) {
        reshade::api::sampler_with_resource_view d{};
        d.sampler = exact_sampler && g_ptde_sampler.handle != 0 ? g_ptde_sampler : binding.sampler;
        d.view = view;
        u.descriptors = &d;
        cmd->push_descriptors(reshade::api::shader_stage::pixel, binding.layout, binding.param_index, u);
        if (exact_sampler && g_ptde_sampler.handle != 0) ++g_sampler_rebind;
    } else {
        g_internal_descriptor_push = false;
        return false;
    }
    g_internal_descriptor_push = false;
    return true;
}

bool push_sampler_binding(reshade::api::command_list *cmd, std::uint32_t slot,
                          const sampler_binding &binding, reshade::api::sampler sampler)
{
    if (!binding.valid || sampler.handle == 0) return false;
    reshade::api::descriptor_table_update u{};
    u.binding = slot; u.array_offset = 0; u.count = 1;
    u.type = reshade::api::descriptor_type::sampler; u.descriptors = &sampler;
    g_internal_descriptor_push = true;
    cmd->push_descriptors(reshade::api::shader_stage::pixel, binding.layout, binding.param_index, u);
    g_internal_descriptor_push = false;
    return true;
}

struct replacement_plan {
    command_bindings original{};
    std::array<reshade::api::resource_view,2> replacement{};
    std::array<bool,2> replace{};
    bool use_exact_sampler = false;
};

std::optional<replacement_plan> build_plan(reshade::api::command_list *cmd)
{
    const auto pipeline = global_tracker().resolve_bound_pipeline(command_id(cmd));
    if (!pipeline.has_value() || !pipeline->confirmed) { ++g_receiver_reject; return std::nullopt; }
    ++g_receiver_match;
    const auto binding = active_binding();
    if (!binding.has_value() || binding->record == nullptr) { ++g_material_unknown; ++g_fail_open; return std::nullopt; }
    const material_record &m = *binding->record;
    enum class action { present, explicit_none, none } a = action::none;
    if (m.state == material_envspec_state::present) a = action::present;
    else if (m.state == material_envspec_state::explicit_none && binding->explicit_none_safe) a = action::explicit_none;
    else { ++g_material_unknown; ++g_fail_open; return std::nullopt; }
    command_bindings current{};
    {
        std::lock_guard lock(g_mutex);
        const auto it = g_command_bindings.find(command_id(cmd));
        if (it == g_command_bindings.end()) { ++g_fail_open; return std::nullopt; }
        current = it->second;
    }
    replacement_plan plan{}; plan.original = current;
    for (int i = 0; i < 2; ++i) {
        if (!current.srv[i].valid) continue;
        if (a == action::explicit_none) {
            if (g_black_view.handle == 0 && !create_black_cube()) { ++g_fail_open; return std::nullopt; }
            plan.replacement[i] = g_black_view;
            plan.replace[i] = true;
            continue;
        }
        const auto probe = probe_for_view(current.srv[i].view);
        if (!probe.has_value()) continue;
        const auto virtual_view = get_virtual_cube(*probe, m.slot);
        if (!virtual_view.has_value()) { ++g_resource_route_miss; continue; }
        plan.replacement[i] = *virtual_view;
        plan.replace[i] = true;
        ++g_resource_route_hit;
    }
    if (!plan.replace[0] && !plan.replace[1]) { ++g_resource_route_miss; ++g_fail_open; return std::nullopt; }
    if (a == action::present) { ++g_material_present; plan.use_exact_sampler = true; }
    else ++g_material_explicit_none;
    return plan;
}

bool apply_plan(reshade::api::command_list *cmd, const replacement_plan &plan)
{
    bool any = false;
    for (int i = 0; i < 2; ++i) {
        if (!plan.replace[i]) continue;
        const std::uint32_t slot = i == 0 ? 12u : 14u;
        if (push_srv_binding(cmd, slot, plan.original.srv[i], plan.replacement[i], plan.use_exact_sampler)) {
            any = true;
            if (slot == 12) ++g_t12_rebind; else ++g_t14_rebind;
        }
        if (plan.use_exact_sampler && plan.original.sampler[i].valid && g_ptde_sampler.handle != 0) {
            if (push_sampler_binding(cmd, slot, plan.original.sampler[i], g_ptde_sampler)) ++g_sampler_rebind;
        }
    }
    return any;
}

bool restore_plan(reshade::api::command_list *cmd, const replacement_plan &plan)
{
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        if (!plan.replace[i]) continue;
        const std::uint32_t slot = i == 0 ? 12u : 14u;
        ok = push_srv_binding(cmd, slot, plan.original.srv[i], plan.original.srv[i].view, false) && ok;
        if (plan.original.sampler[i].valid)
            ok = push_sampler_binding(cmd, slot, plan.original.sampler[i], plan.original.sampler[i].sampler) && ok;
    }
    if (ok) ++g_restore_pass; else ++g_restore_fail;
    return ok;
}

bool on_draw(reshade::api::command_list *cmd, std::uint32_t vertex_count,
             std::uint32_t instance_count, std::uint32_t first_vertex,
             std::uint32_t first_instance)
{
    if (g_internal_replay) return false;
    const auto plan = build_plan(cmd);
    if (!plan.has_value() || !apply_plan(cmd, *plan)) return false;
    g_internal_replay = true;
    cmd->draw(vertex_count, instance_count, first_vertex, first_instance);
    g_internal_replay = false;
    restore_plan(cmd, *plan);
    ++g_replay_pass;
    return true;
}

bool on_draw_indexed(reshade::api::command_list *cmd, std::uint32_t index_count,
                     std::uint32_t instance_count, std::uint32_t first_index,
                     std::int32_t vertex_offset, std::uint32_t first_instance)
{
    if (g_internal_replay) return false;
    const auto plan = build_plan(cmd);
    if (!plan.has_value() || !apply_plan(cmd, *plan)) return false;
    g_internal_replay = true;
    cmd->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
    g_internal_replay = false;
    restore_plan(cmd, *plan);
    ++g_replay_pass;
    return true;
}

void log_snapshot(const runtime_snapshot &snapshot)
{
    const std::uint64_t p = ++g_present_count;
    if (p != 1 && (p % 300) != 0) return;
    char line[1800] = {};
    std::snprintf(line, sizeof(line),
        "[DSRRL FULL ENVSPEC CARRIER V3] P=%llu EXE=%llu/%llu HOOK=%llu/%llu "
        "PARSER=%llu mapped=%llu unmapped=%llu SELECTOR=%llu resolved=%llu "
        "native=%llu/%llu miss=%llu virtual=%llu/%llu recv=%llu reject=%llu "
        "mat_present=%llu explicit_none=%llu unknown=%llu route=%llu miss=%llu "
        "t12=%llu t14=%llu sampler=%llu replay=%llu restore=%llu/%llu failopen=%llu "
        "LIVE res=%llu view=%llu pipe=%llu cmd=%llu",
        static_cast<unsigned long long>(p),
        static_cast<unsigned long long>(g_exe_guard_pass.load()),
        static_cast<unsigned long long>(g_exe_guard_fail.load()),
        static_cast<unsigned long long>(g_hook_install_pass.load()),
        static_cast<unsigned long long>(g_hook_install_fail.load()),
        static_cast<unsigned long long>(g_parser_seen.load()),
        static_cast<unsigned long long>(g_material_mapped.load()),
        static_cast<unsigned long long>(g_material_unmapped.load()),
        static_cast<unsigned long long>(g_selector_seen.load()),
        static_cast<unsigned long long>(g_selector_resolved.load()),
        static_cast<unsigned long long>(g_native_matches.load()),
        static_cast<unsigned long long>(g_native_candidates.load()),
        static_cast<unsigned long long>(g_native_hash_miss.load()),
        static_cast<unsigned long long>(g_virtual_created.load()),
        static_cast<unsigned long long>(g_virtual_create_fail.load()),
        static_cast<unsigned long long>(g_receiver_match.load()),
        static_cast<unsigned long long>(g_receiver_reject.load()),
        static_cast<unsigned long long>(g_material_present.load()),
        static_cast<unsigned long long>(g_material_explicit_none.load()),
        static_cast<unsigned long long>(g_material_unknown.load()),
        static_cast<unsigned long long>(g_resource_route_hit.load()),
        static_cast<unsigned long long>(g_resource_route_miss.load()),
        static_cast<unsigned long long>(g_t12_rebind.load()),
        static_cast<unsigned long long>(g_t14_rebind.load()),
        static_cast<unsigned long long>(g_sampler_rebind.load()),
        static_cast<unsigned long long>(g_replay_pass.load()),
        static_cast<unsigned long long>(g_restore_pass.load()),
        static_cast<unsigned long long>(g_restore_fail.load()),
        static_cast<unsigned long long>(g_fail_open.load()),
        static_cast<unsigned long long>(snapshot.live_resources),
        static_cast<unsigned long long>(snapshot.live_views),
        static_cast<unsigned long long>(snapshot.live_pipelines),
        static_cast<unsigned long long>(snapshot.live_commands));
    reshade::log::message(reshade::log::level::info, line);
}

void register_v3_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::register_event<reshade::addon_event::draw>(on_draw);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
}

void unregister_v3_events()
{
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::unregister_event<reshade::addon_event::draw>(on_draw);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::unregister_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
}

} // namespace
} // namespace dsrrl::runtime_v2::full_envspec_v3

extern "C" __declspec(dllexport) const char *NAME = "DSRRL PTDE Full EnvSpec Carrier V3";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Exact material + native-probe routed PTDE EnvSpec carrier diagnostic; 48 no-PointLight HemEnv/HemEnvLerp receivers; fail-open.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module)) return false;
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    set_receiver_classifier(classify_receiver);
    set_snapshot_sink(log_snapshot);
    register_reshade_events();
    register_v3_events();
    if (!load_pack(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3] canonical PTDE pack load/SHA failed; PRESENT route stays fail-open");
    }
    if (!load_router_sidecars(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3] material/probe sidecar load/SHA failed; bridge stays fail-open");
    }
    if (!verify_and_install_hooks()) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3] retail EXE/hook guard failed; material bridge stays fail-open");
    } else {
        reshade::log::message(reshade::log::level::info,
            "[DSRRL FULL ENVSPEC CARRIER V3] retail hooks PASS; exact material routing armed");
    }
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    uninstall_detour(g_selector_detour); g_selector_original = nullptr;
    uninstall_detour(g_parser_detour); g_parser_original = nullptr;
    unregister_v3_events();
    unregister_reshade_events();
    set_receiver_classifier(nullptr);
    set_snapshot_sink(nullptr);
    {
        std::lock_guard lock(g_mutex);
        g_materials.clear(); g_native_resources.clear(); g_source_resource_by_view.clear();
        g_command_bindings.clear();
    }
    g_ptde_pack.clear(); g_material_registry.clear(); g_native_probe_registry.clear();
    reshade::unregister_addon(addon_module, reshade_module);
}
