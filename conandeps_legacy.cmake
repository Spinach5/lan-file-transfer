message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(Libwebsockets)
find_package(LibArchive)

set(CONANDEPS_LEGACY  websockets_shared  LibArchive::LibArchive )