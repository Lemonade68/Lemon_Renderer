#ifndef VULKAN_GLTF_SCENE_H
#define VULKAN_GLTF_SCENE_H

#include <vector>

#include "vulkan.h"
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE// depth(0,1) in vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "base/VulkanBuffer.h"
#include "base/VulkanDevice.h"
#include "base/VulkanTexture.h"

// tiny_gltf放在这里，XXX_IMPLEMENTATION放在.cpp文件中的最上面（且所有.cpp文件中只需要写一次）
// #define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "tiny_gltf.h"

namespace LR {
    class VulkanglTFScene {
    public:
        // The class requires some Vulkan objects so it can create it's own resources
        LR::VulkanDevice* vulkanDevice;
        VkQueue           copyQueue;

        // The vertex layout for the samples' model（primitives通过index来获得对应的vertex数据），一个model只有一个vertexBuffer
        struct Vertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec3 color;
            glm::vec4 tangent;// normal + tangent 可以推出bitangent
        };

        // Single vertex buffer for all primitives，一个model只有一个indexBuffer
        struct {
            VkBuffer       buffer;
            VkDeviceMemory memory;
        } vertices;

        // Single index buffer for all primitives
        struct {
            int            count;
            VkBuffer       buffer;
            VkDeviceMemory memory;
        } indices;

        // The following structures roughly represent the glTF scene structure
        // To keep things simple, they only contain those properties that are required for this sample
        struct Node;

        // A primitive contains the data for a single draw call，表示了一系列同一primitive上的顶点以及其对应的material
        struct Primitive {
            uint32_t firstIndex;
            uint32_t indexCount;
            int32_t  materialIndex;//针对的是materials那个vector
        };

        // Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
        struct Mesh {
            std::vector<Primitive> primitives;
        };

        // A node represents an object in the glTF scene graph
        struct Node {
            Node*              parent;
            std::vector<Node*> children;
            Mesh               mesh;
            glm::mat4          matrix;
            std::string        name;
            bool               visible = true;
            ~Node() {
                for (auto& child : children) {
                    delete child;
                }
            }
        };

        // A glTF material stores information in e.g. the texture that is attached to it and colors
        struct Material {
            glm::vec4       baseColorFactor = glm::vec4(1.0f);
            uint32_t        baseColorTextureIndex;
            uint32_t        normalTextureIndex;
            std::string     alphaMode = "OPAQUE";
            float           alphaCutOff;
            bool            doubleSided = false;
            VkDescriptorSet descriptorSet;
            VkPipeline      pipeline;//每种材料对应一个管线
        };

        // Contains the texture for a single glTF image
        // Images may be reused by texture objects and are as such separated
        struct Image {
            LR::Texture2D texture;
        };

        // A glTF texture stores a reference to the image and a sampler
        // In this sample, we are only interested in the image
        struct Texture {
            int32_t imageIndex;
        };

        /*
		Model data
	*/
        std::vector<Image>    images;   // 包含实际的image数据
        std::vector<Texture>  textures; // 包含指向image的index以及sampler(这里没有)
        std::vector<Material> materials;// 包含指向texture的index
        std::vector<Node*>    nodes;    // 所有的根节点nodes，包含meshes(里面有primitives(里面有index & material) 和 transformation matrix)

        std::string path;

        ~VulkanglTFScene();
        VkDescriptorImageInfo getTextureDescriptor(const size_t index);
        void                  loadImages(tinygltf::Model& input);
        void                  loadTextures(tinygltf::Model& input);
        void                  loadMaterials(tinygltf::Model& input);
        void                  loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFScene::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFScene::Vertex>& vertexBuffer);
        void                  drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFScene::Node* node);
        void                  draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
    };
}// namespace LR

#endif