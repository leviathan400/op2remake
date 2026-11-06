# =============================================================================
# op2inspect — headless CLI over the sim (load a scenario, step, inspect).
# No Qt: op2sim + op2maps only.
# =============================================================================

add_executable(op2inspect main.cpp json.h)
target_link_libraries(op2inspect PRIVATE op2maps)
target_compile_features(op2inspect PRIVATE cxx_std_20)

# OP2Utility.dll is implicitly linked → must sit next to the exe (as op2maps_selftest).
add_custom_command(TARGET op2inspect POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/third_party/OP2UtilityC/OP2Utility.dll $<TARGET_FILE_DIR:op2inspect>
    VERBATIM)
