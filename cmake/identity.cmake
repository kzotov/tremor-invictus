set(PROJECT_NAME ioq3)
set(PROJECT_VERSION 1.36)

set(SERVER_NAME ioq3ded)
set(CLIENT_NAME ioquake3)

set(BASEGAME baseq3)

set(CGAME_MODULE cgame)
set(GAME_MODULE qagame)
set(UI_MODULE ui)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/windows/quake3.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/misc/macos/quake3_flat.icns)

# the app icon is made from the logo emblem at configure time (macOS tools only)
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
endif()
set(MACOS_BUNDLE_ID org.ioquake.${CLIENT_NAME})

set(COPYRIGHT "QUAKE III ARENA Copyright © 1999-2000 id Software, Inc. All rights reserved.")

set(CONTACT_EMAIL "info@ioquake.org")
set(PROTOCOL_HANDLER_SCHEME quake3)
