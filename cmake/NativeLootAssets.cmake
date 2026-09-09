# Precompute the accepted artwork while building, never in a gameplay frame.
add_executable(native_loot_bake tools/bake_native_loot.cpp)
target_include_directories(native_loot_bake PRIVATE src/dx12 d2gl/d2gl/vendor/include)
target_compile_definitions(native_loot_bake PRIVATE _CRT_SECURE_NO_WARNINGS)
# Match the verifier's precise arithmetic so pixel quantization is stable
# across translation units. This tool runs at build time, never during play.
target_compile_options(native_loot_bake PRIVATE /O2 /fp:precise /arch:SSE2)
set(MXL_LOOT_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-loot-assets/$<CONFIG>")
set(MXL_LOOT_ASSET_RC "${MXL_LOOT_ASSET_DIR}/native_loot_assets.rc")
add_custom_command(OUTPUT "${MXL_LOOT_ASSET_RC}"
  COMMAND $<TARGET_FILE:native_loot_bake> "${MXL_LOOT_ASSET_DIR}"
  DEPENDS native_loot_bake
  COMMENT "Baking native loot artwork into renderer resources"
  VERBATIM)
add_custom_target(native_loot_assets DEPENDS "${MXL_LOOT_ASSET_RC}")
set_source_files_properties("${MXL_LOOT_ASSET_RC}" PROPERTIES GENERATED TRUE)
