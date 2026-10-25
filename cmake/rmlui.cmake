# RmlUi (MIT, code/thirdparty/RmlUi-6.3) for the client's menus: built as a
# static library with its default FreeType font engine. FreeType (FTL,
# code/thirdparty/freetype-2.13.3) is built here too, with none of its
# optional dependencies, so the result has every architecture we build.

option(USE_RMLUI "RmlUi menus in the client" ON)
if(NOT USE_RMLUI OR NOT BUILD_CLIENT)
    return()
endif()

enable_language(CXX)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
set(TREMOR_SAVED_SHARED ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${SOURCE_DIR}/thirdparty/freetype-2.13.3 ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)
set(BUILD_SHARED_LIBS ${TREMOR_SAVED_SHARED})
target_compile_options(freetype PRIVATE -w)
# linked into the renderer libraries as well (their TrueType HUD text)
set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(RMLUI_DIR ${SOURCE_DIR}/thirdparty/RmlUi-6.3)
file(GLOB_RECURSE RMLUI_SOURCES CONFIGURE_DEPENDS ${RMLUI_DIR}/Source/Core/*.cpp)

add_library(rmlui_core STATIC ${RMLUI_SOURCES})
set_target_properties(rmlui_core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_include_directories(rmlui_core PUBLIC ${RMLUI_DIR}/Include)
target_compile_definitions(rmlui_core PUBLIC RMLUI_STATIC_LIB RMLUI_NO_THIRDPARTY_CONTAINERS
    PRIVATE RMLUI_FONT_ENGINE_FREETYPE)
target_link_libraries(rmlui_core PRIVATE freetype)
# third-party code: its own warnings are not ours to fix
target_compile_options(rmlui_core PRIVATE -w)
