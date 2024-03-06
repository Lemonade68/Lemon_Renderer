if(WIN32)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
    if(Vulkan_LIBRARY)
        message("Using bundled Vulkan library version")
    endif()
endif()
# find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${LIBS_PATH}/vulkan)
find_library(glfw_LIBRARY NAMES glfw3 PATHS ${LIBS_PATH}/glfw)

add_executable( ${NAME}
    main.cpp
)

target_link_libraries(${NAME} ${Vulkan_LIBRARY})
target_link_libraries(${NAME} ${glfw_LIBRARY})