# RmlUi (MIT, code/thirdparty/RmlUi-6.3) for the client's menus: built as a
# static library with its default FreeType font engine. Without FreeType the
# client builds without it and keeps the classic menus.

option(USE_RMLUI "RmlUi menus in the client" ON)
if(NOT USE_RMLUI OR NOT BUILD_CLIENT)
    return()
endif()

find_package(Freetype QUIET)
if(NOT FREETYPE_FOUND)
    message(STATUS "FreeType not found: building without RmlUi")
    set(USE_RMLUI OFF)
    return()
endif()

enable_language(CXX)

set(RMLUI_DIR ${SOURCE_DIR}/thirdparty/RmlUi-6.3)
file(GLOB_RECURSE RMLUI_SOURCES CONFIGURE_DEPENDS ${RMLUI_DIR}/Source/Core/*.cpp)

add_library(rmlui_core STATIC ${RMLUI_SOURCES})
set_target_properties(rmlui_core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(rmlui_core PUBLIC ${RMLUI_DIR}/Include)
target_compile_definitions(rmlui_core PUBLIC RMLUI_STATIC_LIB RMLUI_NO_THIRDPARTY_CONTAINERS
    PRIVATE RMLUI_FONT_ENGINE_FREETYPE)
target_link_libraries(rmlui_core PRIVATE Freetype::Freetype)
# third-party code: its own warnings are not ours to fix
target_compile_options(rmlui_core PRIVATE -w)
