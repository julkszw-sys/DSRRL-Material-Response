# Renderer Core v1 — Equipment microfacet boundary

DSR microfacet/PBL specular is not homologous to PTDE equipment local specular. A PTDE island must replace the complete owned local-specular window; feeding PTDE state into a surviving DSR GGX/Schlick/NdotL tail is forbidden.

PTDE target on exact supported Spc PointLight receivers:

    R = 2 * dot(N,V) * N - V
    S_i = q_i * A_i * pow(max(dot(R,L_i),0), c102)
    C_spec = M_spec * sum(S_i)

The PTDE specular branch is not multiplied by NdotL. Replacing only the DSR GGX/Schlick kernel inside its common diffuse/specular NdotL bracket is therefore forbidden.

Replacement entry is after exact selected-light/source/geometry/material inputs and before DSR local microfacet material response. PTDE owns attenuation/source, reflection vector, max(RdotL,0), c102 POW, SpecRGB*c101*COLOR0 and accumulation. The window suppresses DSR local GGX/Schlick, SpecTex-alpha roughness in the replaced branch, common NdotL on specular, and—when the enclosing PointLight island owns them—DSR category gain, q^2.2, cubic attenuation and Lit factor. Replacement exit is the additive local-light contribution immediately before the first proven homologous downstream composition.

Microfacet suppression is route/receiver scoped, never global. PTDE-noSpc -> DSR-Spc routes require suppression of DSR-added specular only under exact material/receiver routing. NoSpc strata are not patched. EnvSpec owns its separate PBL/roughness tail. Unknown/nonhomologous routes fail open.

Activation requires exact material+receiver identity, selected-light provenance/order, PTDE source/attenuation, c101/c102, required SpecRGB/vertex-spec inputs and verified entry/exit/downstream homology. Existing cb0[11].x may carry the exponent for a narrowly verified bridge but does not authorize activation by itself. Roughness must not be constantized as a PTDE substitute.

Anti-hybrid rule: PTDE exponent, SpecRGB or PointLight source must never feed an unverified stock DSR microfacet/NdotL tail. Fail open unless the complete replacement window is owned.

This is a capture-free implementation boundary from confirmed cross-render operator evidence. Runtime activation and pixel equivalence remain OPEN.
