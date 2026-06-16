########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

list(APPEND libwebsockets_COMPONENT_NAMES websockets_shared)
list(REMOVE_DUPLICATES libwebsockets_COMPONENT_NAMES)
if(DEFINED libwebsockets_FIND_DEPENDENCY_NAMES)
  list(APPEND libwebsockets_FIND_DEPENDENCY_NAMES OpenSSL)
  list(REMOVE_DUPLICATES libwebsockets_FIND_DEPENDENCY_NAMES)
else()
  set(libwebsockets_FIND_DEPENDENCY_NAMES OpenSSL)
endif()
set(OpenSSL_FIND_MODE "NO_MODULE")

########### VARIABLES #######################################################################
#############################################################################################
set(libwebsockets_PACKAGE_FOLDER_RELEASE "/home/zqw/.conan2/p/b/libwe7e21572057626/p")
set(libwebsockets_BUILD_MODULES_PATHS_RELEASE )


set(libwebsockets_INCLUDE_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/include")
set(libwebsockets_RES_DIRS_RELEASE )
set(libwebsockets_DEFINITIONS_RELEASE )
set(libwebsockets_SHARED_LINK_FLAGS_RELEASE )
set(libwebsockets_EXE_LINK_FLAGS_RELEASE )
set(libwebsockets_OBJECTS_RELEASE )
set(libwebsockets_COMPILE_DEFINITIONS_RELEASE )
set(libwebsockets_COMPILE_OPTIONS_C_RELEASE )
set(libwebsockets_COMPILE_OPTIONS_CXX_RELEASE )
set(libwebsockets_LIB_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/lib")
set(libwebsockets_BIN_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/bin")
set(libwebsockets_LIBRARY_TYPE_RELEASE SHARED)
set(libwebsockets_IS_HOST_WINDOWS_RELEASE 0)
set(libwebsockets_LIBS_RELEASE websockets)
set(libwebsockets_SYSTEM_LIBS_RELEASE dl m)
set(libwebsockets_FRAMEWORK_DIRS_RELEASE )
set(libwebsockets_FRAMEWORKS_RELEASE )
set(libwebsockets_BUILD_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/lib/cmake")
set(libwebsockets_NO_SONAME_MODE_RELEASE FALSE)


# COMPOUND VARIABLES
set(libwebsockets_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${libwebsockets_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${libwebsockets_COMPILE_OPTIONS_C_RELEASE}>")
set(libwebsockets_LINKER_FLAGS_RELEASE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${libwebsockets_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${libwebsockets_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${libwebsockets_EXE_LINK_FLAGS_RELEASE}>")


set(libwebsockets_COMPONENTS_RELEASE websockets_shared)
########### COMPONENT websockets_shared VARIABLES ############################################

set(libwebsockets_websockets_shared_INCLUDE_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/include")
set(libwebsockets_websockets_shared_LIB_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/lib")
set(libwebsockets_websockets_shared_BIN_DIRS_RELEASE "${libwebsockets_PACKAGE_FOLDER_RELEASE}/bin")
set(libwebsockets_websockets_shared_LIBRARY_TYPE_RELEASE SHARED)
set(libwebsockets_websockets_shared_IS_HOST_WINDOWS_RELEASE 0)
set(libwebsockets_websockets_shared_RES_DIRS_RELEASE )
set(libwebsockets_websockets_shared_DEFINITIONS_RELEASE )
set(libwebsockets_websockets_shared_OBJECTS_RELEASE )
set(libwebsockets_websockets_shared_COMPILE_DEFINITIONS_RELEASE )
set(libwebsockets_websockets_shared_COMPILE_OPTIONS_C_RELEASE "")
set(libwebsockets_websockets_shared_COMPILE_OPTIONS_CXX_RELEASE "")
set(libwebsockets_websockets_shared_LIBS_RELEASE websockets)
set(libwebsockets_websockets_shared_SYSTEM_LIBS_RELEASE dl m)
set(libwebsockets_websockets_shared_FRAMEWORK_DIRS_RELEASE )
set(libwebsockets_websockets_shared_FRAMEWORKS_RELEASE )
set(libwebsockets_websockets_shared_DEPENDENCIES_RELEASE openssl::openssl)
set(libwebsockets_websockets_shared_SHARED_LINK_FLAGS_RELEASE )
set(libwebsockets_websockets_shared_EXE_LINK_FLAGS_RELEASE )
set(libwebsockets_websockets_shared_NO_SONAME_MODE_RELEASE FALSE)

# COMPOUND VARIABLES
set(libwebsockets_websockets_shared_LINKER_FLAGS_RELEASE
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${libwebsockets_websockets_shared_SHARED_LINK_FLAGS_RELEASE}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${libwebsockets_websockets_shared_SHARED_LINK_FLAGS_RELEASE}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${libwebsockets_websockets_shared_EXE_LINK_FLAGS_RELEASE}>
)
set(libwebsockets_websockets_shared_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${libwebsockets_websockets_shared_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${libwebsockets_websockets_shared_COMPILE_OPTIONS_C_RELEASE}>")