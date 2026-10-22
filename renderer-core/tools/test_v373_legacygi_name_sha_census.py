#!/usr/bin/env python3
"""V373 legacy census-only source proof; never a runtime or pixel PASS."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
head=(root/"include/dsrrl/runtime/v373_legacygi_name_sha_census.hpp").read_text()
src=(root/"src/runtime/envspec_resource_runtime.cpp").read_text()
candidates=re.findall(r'\{"(EnvDif_m\d\d_\d\d\d)", "([a-f0-9]{64})", (true|false)\}',head)
assert len(candidates)==17, len(candidates)
assert len(set(x[0] for x in candidates))==17
assert len(set(x[1] for x in candidates))==17
assert sum(x[2]=='true' for x in candidates)==2
assert sorted(x[0] for x in candidates if x[2]=='true')==[
    "EnvDif_m18_500","EnvDif_m18_510"]
assert sum(x[0].startswith("EnvDif_m10_") for x in candidates)==9
assert sum(x[0].startswith("EnvDif_m18_") for x in candidates)==8
assert "56a6702b61815120e8e1d82ef3b6823f4691b16e77b24c33380767db695761d5" in head
assert "previously_authorized" in head
assert 'v373_legacygi::find_exact' in src
assert 'v371_exact_resource_locked' in src
assert 'g_v373_census_by_view' in src
assert 'g_v373_census_by_view.erase(view.handle)' in src
assert 'it=g_v373_census_by_view.erase(it)' in src
assert '[DSRRL V373 LEGACY CREATE]' in src
assert '[DSRRL V373 LEGACY T11 CENSUS]' in src
assert 'action=%s pixel=OPEN' in src
assert '"FAIL_OPEN_STOCK_DSR"' in src
assert 'stock_t11->GetResource(&backing);' in src
assert 'stock_t11->GetDevice(&native_device);' in src
assert 'name.stage>it->second.stage' in src
assert 'v373_legacygi::k_candidates[ix].previously_authorized' in src
assert 'g_v371_legacy_by_view[view.handle]' in src
assert 'const auto *legacy=v371_exact_resource_locked(resource_handle.handle);' in src
assert 'reshade::log::message(reshade::log::level::warning,line);' in src
assert '#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)' in src
print("V373 39 PTDE sources / 17 SHA candidates / 2 authorized SOURCE CONTRACT PASS; PTDE pixel OPEN")
