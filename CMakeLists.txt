cmake_minimum_required(VERSION 3.21)
project(cshot)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-protocols)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(XKB REQUIRED xkbcommon)
#pkg_check_modules(GIO REQUIRED gio-2.0)

execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
    OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

find_program(WAYLAND_SCANNER wayland-scanner)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/protocols")
function(generate_wayland_protocol PROTOCOL_PATH OUTPUT_LIST)
    get_filename_component(PROTOCOL_NAME ${PROTOCOL_PATH} NAME_WE)

    set(GEN_CLIENT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/protocols/${PROTOCOL_NAME}-client-protocol.h")
    set(GEN_CODE "${CMAKE_CURRENT_BINARY_DIR}/include/protocols/${PROTOCOL_NAME}-protocol.c")

    add_custom_command(
        OUTPUT "${GEN_CLIENT_HEADER}"
        COMMAND ${WAYLAND_SCANNER} client-header "${PROTOCOL_PATH}" "${GEN_CLIENT_HEADER}"
        DEPENDS "${PROTOCOL_PATH}"
    )
    add_custom_command(
        OUTPUT "${GEN_CODE}"
        COMMAND ${WAYLAND_SCANNER} private-code "${PROTOCOL_PATH}" "${GEN_CODE}"
        DEPENDS "${PROTOCOL_PATH}"
    )

    set(${OUTPUT_LIST} ${${OUTPUT_LIST}} "${GEN_CLIENT_HEADER}" "${GEN_CODE}" PARENT_SCOPE)
endfunction()

set(WAYLAND_PROTOCOLS
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${WAYLAND_PROTOCOLS_DIR}/stable/tablet/tablet-v2.xml"
    "${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/protocols/wlr-layer-shell-unstable-v1.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/protocols/wlr-screencopy-unstable-v1.xml"
)

set(WAYLAND_PROTOCOL_SOURCES "")
foreach(PROTO ${WAYLAND_PROTOCOLS})
    generate_wayland_protocol(${PROTO} WAYLAND_PROTOCOL_SOURCES)
endforeach()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_definitions(PROJECT_NAME="${PROJECT_NAME}" _POSIX_C_SOURCE=200809L)
add_compile_options(
    -Wall -Wextra -Wpedantic -Wno-incompatible-pointer-types
    -march=nocona -mtune=core-avx2 -O2 -mfpmath=sse -pipe -flto
    -fwrapv -fPIC -ffunction-sections -fdata-sections -fno-plt
)
add_link_options("SHELL:LINKER:-s,-O1,--gc-sections,--as-needed")

add_executable(${PROJECT_NAME}
    ${WAYLAND_PROTOCOL_SOURCES}
    src/cleanup.c
    src/common.c
    src/input.c
    src/registry.c
    src/render.c
    src/save.c
    src/screencopy.c
    src/shm.c
    src/main.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ./include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${WAYLAND_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${XKB_INCLUDE_DIRS}
    #${GIO_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${WAYLAND_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${XKB_LIBRARIES}
    #${GIO_LIBRARIES}
    rt
)

add_custom_target(run
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
    COMMENT "Executing ${PROJECT_NAME}..."
)
