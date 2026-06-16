# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/Libwebsockets-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${libwebsockets_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${Libwebsockets_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET websockets_shared)
    add_library(websockets_shared INTERFACE IMPORTED)
    message(${Libwebsockets_MESSAGE_MODE} "Conan: Target declared 'websockets_shared'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/Libwebsockets-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()