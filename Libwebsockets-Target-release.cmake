# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(libwebsockets_FRAMEWORKS_FOUND_RELEASE "") # Will be filled later
conan_find_apple_frameworks(libwebsockets_FRAMEWORKS_FOUND_RELEASE "${libwebsockets_FRAMEWORKS_RELEASE}" "${libwebsockets_FRAMEWORK_DIRS_RELEASE}")

set(libwebsockets_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET libwebsockets_DEPS_TARGET)
    add_library(libwebsockets_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET libwebsockets_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:Release>:${libwebsockets_FRAMEWORKS_FOUND_RELEASE}>
             $<$<CONFIG:Release>:${libwebsockets_SYSTEM_LIBS_RELEASE}>
             $<$<CONFIG:Release>:openssl::openssl>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### libwebsockets_DEPS_TARGET to all of them
conan_package_library_targets("${libwebsockets_LIBS_RELEASE}"    # libraries
                              "${libwebsockets_LIB_DIRS_RELEASE}" # package_libdir
                              "${libwebsockets_BIN_DIRS_RELEASE}" # package_bindir
                              "${libwebsockets_LIBRARY_TYPE_RELEASE}"
                              "${libwebsockets_IS_HOST_WINDOWS_RELEASE}"
                              libwebsockets_DEPS_TARGET
                              libwebsockets_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELEASE"
                              "libwebsockets"    # package_name
                              "${libwebsockets_NO_SONAME_MODE_RELEASE}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${libwebsockets_BUILD_DIRS_RELEASE} ${CMAKE_MODULE_PATH})

########## COMPONENTS TARGET PROPERTIES Release ########################################

    ########## COMPONENT websockets_shared #############

        set(libwebsockets_websockets_shared_FRAMEWORKS_FOUND_RELEASE "")
        conan_find_apple_frameworks(libwebsockets_websockets_shared_FRAMEWORKS_FOUND_RELEASE "${libwebsockets_websockets_shared_FRAMEWORKS_RELEASE}" "${libwebsockets_websockets_shared_FRAMEWORK_DIRS_RELEASE}")

        set(libwebsockets_websockets_shared_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET libwebsockets_websockets_shared_DEPS_TARGET)
            add_library(libwebsockets_websockets_shared_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET libwebsockets_websockets_shared_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_FRAMEWORKS_FOUND_RELEASE}>
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_SYSTEM_LIBS_RELEASE}>
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_DEPENDENCIES_RELEASE}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'libwebsockets_websockets_shared_DEPS_TARGET' to all of them
        conan_package_library_targets("${libwebsockets_websockets_shared_LIBS_RELEASE}"
                              "${libwebsockets_websockets_shared_LIB_DIRS_RELEASE}"
                              "${libwebsockets_websockets_shared_BIN_DIRS_RELEASE}" # package_bindir
                              "${libwebsockets_websockets_shared_LIBRARY_TYPE_RELEASE}"
                              "${libwebsockets_websockets_shared_IS_HOST_WINDOWS_RELEASE}"
                              libwebsockets_websockets_shared_DEPS_TARGET
                              libwebsockets_websockets_shared_LIBRARIES_TARGETS
                              "_RELEASE"
                              "libwebsockets_websockets_shared"
                              "${libwebsockets_websockets_shared_NO_SONAME_MODE_RELEASE}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET websockets_shared
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_OBJECTS_RELEASE}>
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_LIBRARIES_TARGETS}>
                     )

        if("${libwebsockets_websockets_shared_LIBS_RELEASE}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET websockets_shared
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         libwebsockets_websockets_shared_DEPS_TARGET)
        endif()

        set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_LINKER_FLAGS_RELEASE}>)
        set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_INCLUDE_DIRS_RELEASE}>)
        set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_LIB_DIRS_RELEASE}>)
        set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_COMPILE_DEFINITIONS_RELEASE}>)
        set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:Release>:${libwebsockets_websockets_shared_COMPILE_OPTIONS_RELEASE}>)


    ########## AGGREGATED GLOBAL TARGET WITH THE COMPONENTS #####################
    set_property(TARGET websockets_shared APPEND PROPERTY INTERFACE_LINK_LIBRARIES websockets_shared)

########## For the modules (FindXXX)
set(libwebsockets_LIBRARIES_RELEASE websockets_shared)
