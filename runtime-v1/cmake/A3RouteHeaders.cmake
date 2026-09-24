set(A3_GEN_DIR "${GEN_DIR}/dsrrl/runtime")
file(MAKE_DIRECTORY "${A3_GEN_DIR}")

add_custom_command(
  OUTPUT "${A3_GEN_DIR}/generated_spec_routes_v12.hpp"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_spec_routes_v12.py"
          --input "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/spec_routes_v12.tsv"
          --output "${A3_GEN_DIR}/generated_spec_routes_v12.hpp"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_spec_routes_v12.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/spec_routes_v12.tsv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${A3_GEN_DIR}/generated_spec_material_routes.hpp"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_spec_material_routes.py"
          --input "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/spec_material_routes_v1.tsv"
          --output "${A3_GEN_DIR}/generated_spec_material_routes.hpp"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_spec_material_routes.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/spec_material_routes_v1.tsv"
  VERBATIM
)

add_custom_command(
  OUTPUT "${A3_GEN_DIR}/generated_ul_stable_hashes.hpp"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_ul_stable_hashes.py"
          --input "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/ul_stable_v29_hashes_v1.tsv"
          --output "${A3_GEN_DIR}/generated_ul_stable_hashes.hpp"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/tools/generate_ul_stable_hashes.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/../renderer-core/data/provenance/ul_stable_v29_hashes_v1.tsv"
  VERBATIM
)

add_custom_target(dsrrl_a3_route_headers DEPENDS
  "${A3_GEN_DIR}/generated_spec_routes_v12.hpp"
  "${A3_GEN_DIR}/generated_spec_material_routes.hpp"
  "${A3_GEN_DIR}/generated_ul_stable_hashes.hpp"
)
