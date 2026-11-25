configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/nagus/version.hpp" @ONLY)

add_library(ngs_core STATIC
    src/util/inflate.cpp
    src/data/zip_archive.cpp
    src/data/cd_stream.cpp
    src/data/resources.cpp
    src/data/image.cpp
    src/data/layout.cpp
    src/data/font.cpp
    src/data/lexicon.cpp
    src/data/tables.cpp
    src/data/animation.cpp
    src/game/economy.cpp
    src/game/fleet.cpp
    src/game/galaxy.cpp
    src/game/research.cpp
    src/game/save.cpp
    src/game/ai.cpp
    src/game/combat.cpp
    src/game/game.cpp
    src/data/cursor.cpp
    src/game_files.cpp
    src/settings.cpp
    src/render/canvas.cpp
    src/ui/assets.cpp
    src/ui/screen.cpp
    src/ui/menus.cpp
    src/ui/load_save_screen.cpp
    src/ui/galaxy_screen.cpp
    src/ui/research_screen.cpp
    src/ui/shell.cpp
    src/ui/system_screen.cpp
)
add_library(nagus::core ALIAS ngs_core)

target_include_directories(ngs_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_features(ngs_core PUBLIC cxx_std_20)
set_target_properties(ngs_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
ngs_set_warnings(ngs_core)
