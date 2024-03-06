cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 17)      #c++17版本

set(NAME Lemon_Render)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(${NAME} LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/target/lib)    # 设置默认存放静态库的位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/target/bin)    # 设置默认存放动态库的位置(和exe放在一起，这样exe运行时可以找到)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/target/bin)    # 可执行文件位置

include_directories(external)
include_directories(external/glm)
include_directories(external/gli)
include_directories(external/imgui)
include_directories(external/tinygltf)
include_directories(external/ktx/include)
include_directories(external/ktx/other_include)

# if(WIN32)
#     find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${CMAKE_SOURCE_DIR}/libs/vulkan)
#     if(Vulkan_LIBRARY)
#         message("Using bundled Vulkan library version")
#     endif()
# endif()

# 不知道是否需要用到  \" 是转义
add_definitions(-ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/assets/\")
add_definitions(-SHADERS_DIR=\"${CMAKE_SOURCE_DIR}/shaders/\")

set(LIBS_PATH ${PROJECT_SOURCE_DIR}/libs)

# 非系统/环境变量中的动态库，需要提前指定dll文件的位置
# target_link_directories(${NAME} PUBLIC ${LIBS_PATH})      # 链接vulkan-1.lib

add_subdirectory(src)

