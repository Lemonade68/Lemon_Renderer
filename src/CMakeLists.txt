if(WIN32)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${LIBS_PATH}/vulkan)
    if(Vulkan_LIBRARY)
        message("Using bundled Vulkan library version")
    endif()
endif()
# find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${LIBS_PATH}/vulkan)
find_library(glfw_LIBRARY NAMES glfw3 PATHS ${LIBS_PATH}/glfw)

# 获取当前目录以及当前目录子目录下的所有.cpp文件，存在变量SOURCE_FILE中
file(GLOB SOURCE_FILE LIST_DIRECTORIES true *.cpp)  
file(GLOB HEADER_FILE LIST_DIRECTORIES true *.h)  


add_executable( ${NAME}
# ${SOURCE_FILE}
    main.cpp
    base/Camera.cpp
    renderer/Renderer.cpp
)

target_include_directories(${NAME} PUBLIC ${HEADER_FILE})

target_link_libraries(${NAME} ${Vulkan_LIBRARY})
target_link_libraries(${NAME} ${glfw_LIBRARY})