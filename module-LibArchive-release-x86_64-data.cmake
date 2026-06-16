########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(libarchive_COMPONENT_NAMES "")
if(DEFINED libarchive_FIND_DEPENDENCY_NAMES)
  list(APPEND libarchive_FIND_DEPENDENCY_NAMES ZLIB Iconv)
  list(REMOVE_DUPLICATES libarchive_FIND_DEPENDENCY_NAMES)
else()
  set(libarchive_FIND_DEPENDENCY_NAMES ZLIB Iconv)
endif()
set(ZLIB_FIND_MODE "MODULE")
set(Iconv_FIND_MODE "MODULE")

########### VARIABLES #######################################################################
#############################################################################################
set(libarchive_PACKAGE_FOLDER_RELEASE "/home/zqw/.conan2/p/b/libarca7ca13f0788c/p")
set(libarchive_BUILD_MODULES_PATHS_RELEASE )


set(libarchive_INCLUDE_DIRS_RELEASE "${libarchive_PACKAGE_FOLDER_RELEASE}/include")
set(libarchive_RES_DIRS_RELEASE )
set(libarchive_DEFINITIONS_RELEASE )
set(libarchive_SHARED_LINK_FLAGS_RELEASE )
set(libarchive_EXE_LINK_FLAGS_RELEASE )
set(libarchive_OBJECTS_RELEASE )
set(libarchive_COMPILE_DEFINITIONS_RELEASE )
set(libarchive_COMPILE_OPTIONS_C_RELEASE )
set(libarchive_COMPILE_OPTIONS_CXX_RELEASE )
set(libarchive_LIB_DIRS_RELEASE "${libarchive_PACKAGE_FOLDER_RELEASE}/lib")
set(libarchive_BIN_DIRS_RELEASE "${libarchive_PACKAGE_FOLDER_RELEASE}/bin")
set(libarchive_LIBRARY_TYPE_RELEASE SHARED)
set(libarchive_IS_HOST_WINDOWS_RELEASE 0)
set(libarchive_LIBS_RELEASE archive)
set(libarchive_SYSTEM_LIBS_RELEASE )
set(libarchive_FRAMEWORK_DIRS_RELEASE )
set(libarchive_FRAMEWORKS_RELEASE )
set(libarchive_BUILD_DIRS_RELEASE )
set(libarchive_NO_SONAME_MODE_RELEASE FALSE)


# COMPOUND VARIABLES
set(libarchive_COMPILE_OPTIONS_RELEASE
    "$<$<COMPILE_LANGUAGE:CXX>:${libarchive_COMPILE_OPTIONS_CXX_RELEASE}>"
    "$<$<COMPILE_LANGUAGE:C>:${libarchive_COMPILE_OPTIONS_C_RELEASE}>")
set(libarchive_LINKER_FLAGS_RELEASE
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${libarchive_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${libarchive_SHARED_LINK_FLAGS_RELEASE}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${libarchive_EXE_LINK_FLAGS_RELEASE}>")


set(libarchive_COMPONENTS_RELEASE )