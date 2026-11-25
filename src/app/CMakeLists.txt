set(NGS_APP_SOURCES
    main.cpp
)

find_package(Threads REQUIRED)

if(ANDROID)
    # SDLActivity loads the game as libmain.so.
    add_library(ngs_app SHARED ${NGS_APP_SOURCES})
    set_target_properties(ngs_app PROPERTIES OUTPUT_NAME main)
else()
    add_executable(ngs_app WIN32 MACOSX_BUNDLE ${NGS_APP_SOURCES})
    set_target_properties(ngs_app PROPERTIES
        OUTPUT_NAME Nagus
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.niliodev.nagus
        MACOSX_BUNDLE_BUNDLE_NAME Nagus
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()

target_link_libraries(ngs_app PRIVATE nagus::core SDL3::SDL3 Threads::Threads)
ngs_set_warnings(ngs_app)
