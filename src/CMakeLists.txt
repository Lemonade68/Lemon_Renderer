if(WIN32)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${LIBS_PATH}/vulkan)

    if(Vulkan_LIBRARY)
        message("Using bundled Vulkan library version")
    endif()
endif()

# find_library(Vulkan_LIBRARY NAMES vulkan-1 PATHS ${LIBS_PATH}/vulkan)
find_library(glfw_LIBRARY NAMES glfw3 PATHS ${LIBS_PATH}/glfw)

# 获取当前目录以及当前目录子目录下的所有.cpp文件，存在变量SOURCE_FILE中
file(GLOB_RECURSE SOURCE_FILE "*.h" "*.cpp" "*.hpp")
file(GLOB_RECURSE HEADER_FILE "*.h" "*.hpp")

# 这样有问题(vulkan_samples里面这样写的)
# # ktx lib
# set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/ktx)
# set(KTX_SOURCES
# ${KTX_DIR}/lib/checkheader.c
# ${KTX_DIR}/lib/dfdutils/createdfd.c
# ${KTX_DIR}/lib/dfdutils/colourspaces.c
# ${KTX_DIR}/lib/dfdutils/dfd.h
# ${KTX_DIR}/lib/dfdutils/dfd2vk.inl
# ${KTX_DIR}/lib/dfdutils/interpretdfd.c
# ${KTX_DIR}/lib/dfdutils/printdfd.c
# ${KTX_DIR}/lib/dfdutils/queries.c
# ${KTX_DIR}/lib/dfdutils/vk2dfd.c
# ${KTX_DIR}/lib/etcdec.cxx
# ${KTX_DIR}/lib/etcunpack.cxx
# ${KTX_DIR}/lib/filestream.c
# ${KTX_DIR}/lib/filestream.h
# ${KTX_DIR}/lib/formatsize.h
# ${KTX_DIR}/lib/hashlist.c
# ${KTX_DIR}/lib/info.c
# ${KTX_DIR}/lib/ktxint.h
# ${KTX_DIR}/lib/memstream.c
# ${KTX_DIR}/lib/memstream.h
# ${KTX_DIR}/lib/stream.h
# ${KTX_DIR}/lib/strings.c
# ${KTX_DIR}/lib/swap.c
# ${KTX_DIR}/lib/uthash.h
# ${KTX_DIR}/lib/texture.c
# ${KTX_DIR}/lib/texture.h

# # Basis Universal
# ${KTX_DIR}/lib/basis_sgd.h
# ${KTX_DIR}/lib/basis_transcode.cpp
# ${KTX_DIR}/lib/basisu/transcoder/basisu_containers.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_containers_impl.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_file_headers.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_global_selector_cb.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_global_selector_palette.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_transcoder_internal.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_transcoder_uastc.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu_transcoder.cpp
# ${KTX_DIR}/lib/basisu/transcoder/basisu_transcoder.h
# ${KTX_DIR}/lib/basisu/transcoder/basisu.h
# ${KTX_DIR}/lib/basisu/zstd/zstd.c

# # KT1
# ${KTX_DIR}/lib/texture1.c
# ${KTX_DIR}/lib/texture1.h

# # KTX2
# ${KTX_DIR}/lib/texture2.c
# ${KTX_DIR}/lib/texture2.h

# # Vulkan support
# ${KTX_DIR}/lib/vk_format.h
# ${KTX_DIR}/lib/vkformat_check.c
# ${KTX_DIR}/lib/vkformat_enum.h
# ${KTX_DIR}/lib/vkformat_str.c
# ${KTX_DIR}/lib/vk_funcs.c
# ${KTX_DIR}/lib/vk_funcs.h
# ${KTX_DIR}/lib/vkloader.c
# )

# set(KTX_INCLUDE_DIRS
# ${KTX_DIR}/include
# ${KTX_DIR}/lib
# ${KTX_DIR}/lib/basisu/zstd
# ${KTX_DIR}/lib/basisu/transcoder
# ${KTX_DIR}/other_include
# )

# add_library(ktx STATIC ${KTX_SOURCES})
# target_include_directories(ktx PUBLIC ${KTX_INCLUDE_DIRS})
# target_link_libraries(ktx PUBLIC ${Vulkan_LIBRARY})

# ktx
set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/ktx)
# file(GLOB KTX_HEADERS ${KTX_DIR}/include/*.h ${KTX_DIR}/lib/*.h ${KTX_DIR}/other_include/*.h)
set(KTX_FILES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c)
    
add_library(ktx STATIC ${KTX_FILES})
target_link_libraries(ktx PUBLIC ${Vulkan_LIBRARY})
target_include_directories(ktx INTERFACE ${KTX_DIR}/include ${KTX_DIR}/lib ${KTX_DIR}/other_include)

# imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)
set(IMGUI_FILES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imstb_rectpack.h
    ${IMGUI_DIR}/imstb_textedit.h
    ${IMGUI_DIR}/imstb_truetype.h
    ${IMGUI_DIR}/examples/imgui_impl_glfw.h
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp)

add_library(imgui STATIC ${IMGUI_FILES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
# target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# tinygltf
set(TINYGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/tinygltf)
add_library(tinygltf INTERFACE)
target_sources(tinygltf INTERFACE ${TINYGLTF_DIR}/tiny_gltf.h ${TINYGLTF_DIR}/json.hpp)
target_include_directories(tinygltf INTERFACE ${TINYGLTF_DIR})


add_executable(${NAME} ${SOURCE_FILE})
target_include_directories(${NAME} PUBLIC ${HEADER_FILE})
target_link_libraries(${NAME} ${Vulkan_LIBRARY} ${glfw_LIBRARY} ktx imgui tinygltf)