# =============================================================================
# op2maps — map-format loaders (pluggable, one loader per format).
#
# NOT part of the deterministic sim firewall: this library does file I/O and
# will pull in external dependencies (OP2Utility for .map, a JSON parser for
# .opm/.json). It produces an op2::sim::Map, which IS pure sim data.
#
# To add a format: add its loader source here + register it in
# MapLoaderRegistry::withDefaults(). To drop one: remove both.
# =============================================================================

add_library(op2maps STATIC
    src/maploader.cpp
    src/op2maploader.cpp
    src/opmloader.cpp
    src/op2mapperjsonloader.cpp
)

# Prebuilt OP2UtilityC DLL: import lib + C header (the .map backend).
set(OP2UTILITYC_DIR ${CMAKE_SOURCE_DIR}/third_party/OP2UtilityC)

target_include_directories(op2maps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OP2UTILITYC_DIR}
)

target_compile_features(op2maps PUBLIC cxx_std_20)

# op2sim for the Map type; the OP2Utility import lib for .map loading.
target_link_libraries(op2maps PUBLIC
    op2sim
    ${OP2UTILITYC_DIR}/libOP2Utility.dll.a
)

# --- Self-test ---------------------------------------------------------------
add_executable(op2maps_selftest tests/maps_selftest.cpp)
target_link_libraries(op2maps_selftest PRIVATE op2maps)
add_test(NAME op2maps_selftest COMMAND op2maps_selftest)

# OP2Utility.dll is implicitly linked → must sit next to any exe using op2maps.
add_custom_command(TARGET op2maps_selftest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OP2UTILITYC_DIR}/OP2Utility.dll $<TARGET_FILE_DIR:op2maps_selftest>
    VERBATIM)
