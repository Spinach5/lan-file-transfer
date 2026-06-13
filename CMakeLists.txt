cmake_minimum_required(VERSION 3.10)
project(lanft C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_GUI "Build with SDL2 GUI (requires SDL2, SDL2_ttf)" ON)

# Dependencies required by both modes
find_package(PkgConfig REQUIRED)

if(WIN32)
    # Windows: use vcpkg or manual paths
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "vcpkg toolchain")
    endif()
    pkg_check_modules(LIBWEBSOCKETS libwebsockets)
    pkg_check_modules(LIBARCHIVE libarchive)
else()
    pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
    pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
endif()
find_package(Threads REQUIRED)

# Base sources (always compiled)
set(SOURCES
    src/network.c
    src/transfer.c
    src/cli.c
)

if(BUILD_GUI)
    message(STATUS "Building with SDL2 GUI")
    find_package(SDL2 REQUIRED)
    list(APPEND SOURCES src/main.c src/scanner.c src/ui.c)
    add_definitions(-DBUILD_GUI)
else()
    message(STATUS "Building CLI-only (no SDL2)")
    list(APPEND SOURCES src/main_cli.c)
endif()

add_executable(lanft ${SOURCES})

target_include_directories(lanft PRIVATE
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

if(BUILD_GUI)
    target_include_directories(lanft PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

target_link_libraries(lanft
    ${LIBWEBSOCKETS_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    Threads::Threads
)

if(BUILD_GUI)
    target_link_libraries(lanft ${SDL2_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(lanft ws2_32 iphlpapi)
endif()
