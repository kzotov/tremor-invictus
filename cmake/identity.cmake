set(PROJECT_NAME ioq3)
set(PROJECT_VERSION 1.36)

set(SERVER_NAME ioq3ded)
set(CLIENT_NAME tremor-invictus)

set(BASEGAME baseq3)

set(CGAME_MODULE cgame)
set(GAME_MODULE qagame)
set(UI_MODULE ui)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/windows/quake3.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/macos/quake3_flat.icns)

# a placeholder app icon is made from the working logo's emblem at configure
# time (macOS tools only); the branding pack's icons replace it (below)
if(CMAKE_HOST_APPLE)
    set(TREMOR_ICON_SVG ${CMAKE_SOURCE_DIR}/misc/logo/tremor-invictus-emblem-on-dark.svg)
    set(TREMOR_ICON_ICNS ${CMAKE_BINARY_DIR}/TremorInvictus.icns)
    if(NOT EXISTS ${TREMOR_ICON_ICNS} OR ${TREMOR_ICON_SVG} IS_NEWER_THAN ${TREMOR_ICON_ICNS})
        execute_process(COMMAND ${CMAKE_SOURCE_DIR}/scripts/make-icns.sh ${TREMOR_ICON_SVG} ${TREMOR_ICON_ICNS}
            RESULT_VARIABLE TREMOR_ICON_RESULT)
    endif()
    if(EXISTS ${TREMOR_ICON_ICNS})
        set(MACOS_ICON_PATH ${TREMOR_ICON_ICNS})
    endif()

    # the menu logo, linked into the game's home folder by the run scripts
    set(TREMOR_LOGO_PNG ${CMAKE_BINARY_DIR}/generated/gfx/tremor/logo.png)
    if(NOT EXISTS ${TREMOR_LOGO_PNG} OR ${TREMOR_ICON_SVG} IS_NEWER_THAN ${TREMOR_LOGO_PNG})
        execute_process(COMMAND ${CMAKE_SOURCE_DIR}/scripts/make-logo-images.sh
            ${CMAKE_SOURCE_DIR}/misc/logo ${CMAKE_BINARY_DIR}/generated)
    endif()
endif()

# the lineup and podium pillar textures, drawn by a script at configure time
# and linked into the game's home folder with the menu logo
find_program(TREMOR_PYTHON3 python3)
if(TREMOR_PYTHON3)
    set(TREMOR_PILLAR_SCRIPT ${CMAKE_SOURCE_DIR}/misc/textures/make-pillar-textures.py)
    set(TREMOR_PILLAR_TGA ${CMAKE_BINARY_DIR}/generated/gfx/tremor/pillar/side.tga)
    if(NOT EXISTS ${TREMOR_PILLAR_TGA} OR ${TREMOR_PILLAR_SCRIPT} IS_NEWER_THAN ${TREMOR_PILLAR_TGA})
        execute_process(COMMAND ${TREMOR_PYTHON3} -I ${TREMOR_PILLAR_SCRIPT} ${CMAKE_BINARY_DIR}/generated)
    endif()

    # the rest of the art (splash pictures, medals, ability icons and effects,
    # the HUD icon set and the HUD images):
    # every misc/art/make-*.py, run again when any script there changes
    file(GLOB TREMOR_ART_SCRIPTS ${CMAKE_SOURCE_DIR}/misc/art/make-*.py)
    file(GLOB TREMOR_ART_SOURCES ${CMAKE_SOURCE_DIR}/misc/art/*.py ${CMAKE_SOURCE_DIR}/misc/art/looks/*.py)
    # the Roboto fonts packed into the art (make-models.py): new ones redraw it
    if(DEFINED ENV{TREMOR_FONTS})
        file(GLOB TREMOR_FONT_FILES $ENV{TREMOR_FONTS}/*.ttf)
    else()
        file(GLOB TREMOR_FONT_FILES $ENV{HOME}/projects/quake/fonts/*.ttf)
    endif()
    list(APPEND TREMOR_ART_SOURCES ${TREMOR_FONT_FILES})
    set(TREMOR_ART_STAMP ${CMAKE_BINARY_DIR}/generated/gfx/tremor/art.stamp)
    set(TREMOR_ART_STALE FALSE)
    foreach(src ${TREMOR_ART_SOURCES})
        if(${src} IS_NEWER_THAN ${TREMOR_ART_STAMP})
            set(TREMOR_ART_STALE TRUE)
        endif()
    endforeach()
    if(TREMOR_ART_STALE)
        message(STATUS "Drawing the art in misc/art")
        foreach(script ${TREMOR_ART_SCRIPTS})
            execute_process(COMMAND ${TREMOR_PYTHON3} -I ${script} ${CMAKE_BINARY_DIR}/generated
                RESULT_VARIABLE TREMOR_ART_RESULT)
            if(NOT TREMOR_ART_RESULT EQUAL 0)
                message(WARNING "${script} failed")
            endif()
        endforeach()
        file(TOUCH ${TREMOR_ART_STAMP})
    endif()
endif()

# the approved UI branding: a pack of finished art kept outside the repository
# (misc/art/import-brand-art.py), copied over the drawn placeholders above and
# again on every build (target tremor-brand), with the app icons staged in
# build/brand. Without the pack the placeholders stay and configure says so.
# Container builds have no Python: scripts/build-windows.sh stages build/brand
# on the host first.
if(DEFINED ENV{TREMOR_BRAND})
    set(TREMOR_BRAND_DEFAULT $ENV{TREMOR_BRAND})
elseif(DEFINED ENV{TREMOR_LOCAL})
    set(TREMOR_BRAND_DEFAULT $ENV{TREMOR_LOCAL}/branding/tremor-invictus-v1)
else()
    set(TREMOR_BRAND_DEFAULT $ENV{HOME}/projects/quake/local/branding/tremor-invictus-v1)
endif()
set(TREMOR_BRAND_DIR ${TREMOR_BRAND_DEFAULT} CACHE PATH "The approved UI branding pack (outside the repository)")
set(TREMOR_BRAND_ICONS ${CMAKE_BINARY_DIR}/brand)
if(TREMOR_PYTHON3)
    execute_process(COMMAND ${TREMOR_PYTHON3} -I ${CMAKE_SOURCE_DIR}/misc/art/import-brand-art.py sync
            ${CMAKE_BINARY_DIR}/generated --pack ${TREMOR_BRAND_DIR} --icons ${TREMOR_BRAND_ICONS}
        RESULT_VARIABLE TREMOR_BRAND_RESULT
        ERROR_VARIABLE TREMOR_BRAND_ERRORS)
    if(TREMOR_BRAND_ERRORS)
        message(WARNING "UI branding:\n${TREMOR_BRAND_ERRORS}")
    endif()
    # the icons too, before the client: a changed .icns reaches the app bundle
    function(tremor_brand_target)
        add_custom_target(tremor-brand ALL
            COMMAND ${TREMOR_PYTHON3} -I ${CMAKE_SOURCE_DIR}/misc/art/import-brand-art.py sync
                ${CMAKE_BINARY_DIR}/generated --pack ${TREMOR_BRAND_DIR} --icons ${TREMOR_BRAND_ICONS}
            COMMENT "UI branding" VERBATIM)
        if(TARGET ${CLIENT_BINARY})
            add_dependencies(${CLIENT_BINARY} tremor-brand)
        endif()
    endfunction()
    list(APPEND POST_CONFIGURE_FUNCTIONS tremor_brand_target)
endif()
if(EXISTS ${TREMOR_BRAND_ICONS}/TremorInvictus.icns)
    set(MACOS_ICON_PATH ${TREMOR_BRAND_ICONS}/TremorInvictus.icns)
endif()
if(EXISTS ${TREMOR_BRAND_ICONS}/tremor-invictus.ico)
    set(WINDOWS_ICON_PATH ${TREMOR_BRAND_ICONS}/tremor-invictus.ico)
endif()
# the window icon SDL sets on Windows and Linux (code/sdl/sdl_glimp.c)
if(EXISTS ${TREMOR_BRAND_ICONS}/tremor_window_icon.h)
    list(APPEND CLIENT_DEFINITIONS TREMOR_WINDOW_ICON)
    list(APPEND RENDERER_DEFINITIONS TREMOR_WINDOW_ICON)
    list(APPEND CLIENT_INCLUDE_DIRS ${TREMOR_BRAND_ICONS})
    list(APPEND RENDERER_INCLUDE_DIRS ${TREMOR_BRAND_ICONS})
endif()
set(MACOS_BUNDLE_ID org.tremor-invictus.game)
set(MACOS_APP_DISPLAY_NAME "Tremor Invictus")

set(COPYRIGHT "QUAKE III ARENA Copyright © 1999-2000 id Software, Inc. All rights reserved.")

set(CONTACT_EMAIL "info@ioquake.org")
set(PROTOCOL_HANDLER_SCHEME quake3)
