add_executable(ngs_tests
    test_main.cpp
    test_util.cpp
    test_formats.cpp
    test_economy.cpp
    test_ai.cpp
    test_combat.cpp
    test_diplomacy.cpp
    test_intel.cpp
    test_fleets.cpp
    test_save.cpp
    test_galaxy.cpp
    test_import.cpp
    test_menus.cpp
)
target_link_libraries(ngs_tests PRIVATE nagus::core)
target_compile_definitions(ngs_tests PRIVATE
    NAGUS_GAMEDATA_DIR="${PROJECT_SOURCE_DIR}/gamedata"
    NAGUS_TEST_LIBRARY="${CMAKE_BINARY_DIR}/test-library")
ngs_set_warnings(ngs_tests)

add_test(NAME ngs_tests COMMAND ngs_tests)

# Draws screens to picture files, for looking at the engine's work without
# a window. Needs the original files; not installed.
add_executable(ngs_snapshot snapshot.cpp)
target_link_libraries(ngs_snapshot PRIVATE nagus::core)
ngs_set_warnings(ngs_snapshot)
