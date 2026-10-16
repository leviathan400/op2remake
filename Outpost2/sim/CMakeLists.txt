# =============================================================================
# op2sim — the deterministic simulation core.
#
# FIREWALL: this library MUST NOT depend on Qt, floating-point math, or any
# wall-clock / OS time source. It is integer-only, headless, and fully
# reproducible. Keeping it a standalone target is what enforces that boundary:
# it does not (and must never) link Qt.
# =============================================================================

add_library(op2sim STATIC
    src/op2sim.cpp
    src/checksum.cpp
    src/cmdlog.cpp
    src/command.cpp
    src/replay.cpp
    src/techtree.cpp
    src/sheet.cpp
    src/morale.cpp
    src/world.cpp
)

target_include_directories(op2sim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(op2sim PUBLIC cxx_std_20)

# --- Self-test / regression backbone -----------------------------------------
add_executable(op2sim_selftest tests/sim_selftest.cpp)
target_link_libraries(op2sim_selftest PRIVATE op2sim)
add_test(NAME op2sim_selftest COMMAND op2sim_selftest)
