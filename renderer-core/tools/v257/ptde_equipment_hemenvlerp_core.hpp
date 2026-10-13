#pragma once
// V257 clean PTDE HemEnvLerp equipment reference kernel.
// Construction-only; not an activated D3D11 bridge or a pixel validation.
// PTDE DifSpcBmp HemEnvLerp ps_3_0 verified subcuts: 0511..0542,
// 0611..0679, 0782..0794. Subsurface/other PHN families remain separate.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dsrrl::v257 {
struct rgb { float r,g,b; };
struct rgba { float r,g,b,a; };
struct endpoint { rgba filtered; rgb bank; };
using sha256 = std::array<std::uint8_t,32>;

inline bool nonzero(const sha256& d) noexcept {
    std::uint8_t v=0; for(const auto b:d) v|=b; return v!=0;
}
inline bool exact_digest(const sha256& a,const sha256& b) noexcept {
    if(!nonzero(a)||!nonzero(b)) return false;
    std::uint8_t diff=0;
    for(std::size_t i=0;i<a.size();++i) diff|=a[i]^b[i];
    return diff==0;
}
struct identity {
    // Truncated hashes are fast rejects only. Full hashes MUST match.
    std::uint64_t flver_fingerprint=0,mtd_fingerprint=0;
    std::uint64_t shader_fingerprint=0,logical_tex_hash=0;
    sha256 flver{},mtd{},shader{},texture{},sidecar{};
    std::uint32_t receiver=0,slot=0;
};
struct admission {
    identity expected{},observed{};
    bool equipment_owner_verified=false;
    bool slot_verified=false;
    bool sidecar_loaded_and_hashed=false;
    bool consumer_family_proven=false;
    bool full_material_port_proven=false;
    bool exact_post_material_inputs_proven=false;
};
inline bool accepts(const admission& g) noexcept {
    const identity &a=g.expected,&b=g.observed;
    return g.equipment_owner_verified && g.slot_verified &&
      g.sidecar_loaded_and_hashed && g.consumer_family_proven &&
      g.full_material_port_proven &&
      g.exact_post_material_inputs_proven &&
      (a.receiver==33u || a.receiver==34u || a.receiver==35u) &&
      a.receiver==b.receiver && a.slot==b.slot &&
      a.flver_fingerprint!=0 && a.flver_fingerprint==b.flver_fingerprint &&
      a.mtd_fingerprint!=0 && a.mtd_fingerprint==b.mtd_fingerprint &&
      a.shader_fingerprint!=0 && a.shader_fingerprint==b.shader_fingerprint &&
      a.logical_tex_hash!=0 && a.logical_tex_hash==b.logical_tex_hash &&
      exact_digest(a.flver,b.flver) && exact_digest(a.mtd,b.mtd) &&
      exact_digest(a.shader,b.shader) &&
      exact_digest(a.texture,b.texture) &&
      exact_digest(a.sidecar,b.sidecar);
}
inline bool finite(rgb v) noexcept {
    return std::isfinite(v.r)&&std::isfinite(v.g)&&std::isfinite(v.b);
}
inline bool finite(rgba v) noexcept {
    return std::isfinite(v.r)&&std::isfinite(v.g)&&
           std::isfinite(v.b)&&std::isfinite(v.a);
}
inline rgb add(rgb a,rgb b) noexcept {
    return {a.r+b.r,a.g+b.g,a.b+b.b};
}
inline rgb sub(rgb a,rgb b) noexcept {
    return {a.r-b.r,a.g-b.g,a.b-b.b};
}
inline rgb mul(rgb a,rgb b) noexcept {
    return {a.r*b.r,a.g*b.g,a.b*b.b};
}
inline rgb mul(rgb a,float b) noexcept {
    return {a.r*b,a.g*b,a.b*b};
}
inline rgb lerp(rgb a,rgb b,float beta) noexcept {
    return add(a,mul(sub(b,a),beta));
}
// PTDE filters RGBA first; only THEN divides sample RGB by sample alpha.
// Alpha zero is an unverified physical input => fail-open, not clamp/gain.
inline bool decode(const endpoint& e,rgb& out) noexcept {
    if(!finite(e.filtered)||!finite(e.bank)||e.filtered.a==0.f)
        return false;
    const float reciprocal=1.f/e.filtered.a;
    rgb v={e.filtered.r*reciprocal,e.filtered.g*reciprocal,
           e.filtered.b*reciprocal};
    v=mul(v,e.bank);
    if(!finite(v)) return false;
    out=v;return true;
}
struct pair { endpoint a{},b{}; float beta=0.f; };
struct pair_output { rgb decoded_a{},decoded_b{},blended{}; };
inline bool env_blend(const pair& p,pair_output& out) noexcept {
    if(!std::isfinite(p.beta)||p.beta<0.f||p.beta>1.f) return false;
    pair_output r{};
    if(!decode(p.a,r.decoded_a)||!decode(p.b,r.decoded_b)) return false;
    r.blended=lerp(r.decoded_a,r.decoded_b,p.beta);
    if(!finite(r.blended))return false;
    out=r;return true;
}
// Separate envdiffuse c84.w and envspec c85.w: no shared-beta assumption.
struct environment {
    pair diffuse, specular;
};
struct environment_output {pair_output diffuse{},specular{};};
inline bool env_blend(const environment& e,environment_output& out) noexcept {
    environment_output r{};
    if(!env_blend(e.diffuse,r.diffuse) ||
       !env_blend(e.specular,r.specular)) return false;
    out=r;return true;
}
// Original PTDE ps_3_0 local material composition 0651..0679.
// PTDE SpecTex alpha is NOT consumed in this cut.
struct surface_input {
    rgb envdiffuse{},upper_lower{},envspec{};
    rgb diffuse_tex_rgb{},spec_tex_rgb{};
    rgb c100{},c101{},c156{},vertex_rgb{};
};
inline bool compose_local(const surface_input& p,rgb& out) noexcept {
    if(!finite(p.envdiffuse)||!finite(p.upper_lower)||!finite(p.envspec)||
       !finite(p.diffuse_tex_rgb)||!finite(p.spec_tex_rgb)||
       !finite(p.c100)||!finite(p.c101)||!finite(p.c156)||
       !finite(p.vertex_rgb))return false;
    const rgb spec=mul(p.envspec,
        mul(mul(p.spec_tex_rgb,p.c101),p.vertex_rgb));
    const rgb diff=mul(add(p.envdiffuse,p.upper_lower),
        mul(mul(add(p.diffuse_tex_rgb,p.c156),p.c100),p.vertex_rgb));
    const rgb r=add(diff,spec);
    if(!finite(r))return false;
    out=r;return true;
}
// PTDE exact late LRP and ToneMapEntity scale 0782..0794.
// Caller MUST supply genuine PTDE post-Fog & post-LightScattering signals,
// c110.w and c135; this kernel does not substitute stock DSR atmosphere.
struct terminal_input {
    rgb post_fog_rgb{},post_lightscattering_rgb{};
    float c110_w=0.f,c135_x=0.f,c135_y=0.f;
};
inline bool compose_terminal(const terminal_input& p,rgb& out) noexcept {
    if(!finite(p.post_fog_rgb)||!finite(p.post_lightscattering_rgb)||
       !std::isfinite(p.c110_w)||!std::isfinite(p.c135_x)||
       !std::isfinite(p.c135_y)||p.c135_y==0.f) return false;
    rgb r=mul(lerp(p.post_fog_rgb,p.post_lightscattering_rgb,
                   p.c110_w),p.c135_x/p.c135_y);
    if(!finite(r))return false;
    out=r;return true;
}
} // namespace dsrrl::v257
