if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_METAL OR NOT APPLE OR NOT USE_RENDERER_DLOPEN OR NOT RENDERER_GL1_SOURCES)
    return()
endif()

include(utils/set_output_dirs)
include(renderer_common)

# The classic renderer, drawing through Metal instead of OpenGL
set(RENDERER_METAL_BINARY renderer_metal)

set(RENDERER_METAL_BINARY_SOURCES
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_GL1_SOURCES}
    ${SDL_RENDERER_SOURCES}
    ${RENDERER_LIBRARY_SOURCES}
    ${DYNAMIC_RENDERER_SOURCES}
    ${SOURCE_DIR}/renderermetal/mtl_gl.m)

set_source_files_properties(${SOURCE_DIR}/renderermetal/mtl_gl.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

add_library(${RENDERER_METAL_BINARY} SHARED ${RENDERER_METAL_BINARY_SOURCES})

target_link_libraries(      ${RENDERER_METAL_BINARY} PRIVATE ${RENDERER_LIBRARIES}
    "-framework Metal" "-framework QuartzCore" "-framework Foundation")
target_include_directories( ${RENDERER_METAL_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
target_compile_definitions( ${RENDERER_METAL_BINARY} PRIVATE ${RENDERER_DEFINITIONS} USE_METAL)
target_compile_options(     ${RENDERER_METAL_BINARY} PRIVATE ${RENDERER_COMPILE_OPTIONS})
target_link_options(        ${RENDERER_METAL_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

set_output_dirs(${RENDERER_METAL_BINARY})
