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

// tiny_gltf�������XXX_IMPLEMENTATION����.cpp�ļ��е������棨������.cpp�ļ���ֻ��Ҫдһ�Σ�
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

        // The vertex layout for the samples' model��primitivesͨ��index����ö�Ӧ��vertex���ݣ���һ��modelֻ��һ��vertexBuffer
        struct Vertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec3 color;
            glm::vec4 tangent;// normal + tangent �����Ƴ�bitangent
        };

        // Single vertex buffer for all primitives��һ��modelֻ��һ��indexBuffer
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

        // A primitive contains the data for a single draw call����ʾ��һϵ��ͬһprimitive�ϵĶ����Լ����Ӧ��material
        struct Primitive {
            uint32_t firstIndex;
            uint32_t indexCount;
            int32_t  materialIndex;//��Ե���materials�Ǹ�vector
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
            VkPipeline      pipeline;//ÿ�ֲ��϶�Ӧһ������
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
        std::vector<Image>    images;   // ����ʵ�ʵ�image����
        std::vector<Texture>  textures; // ����ָ��image��index�Լ�sampler(����û��)
        std::vector<Material> materials;// ����ָ��texture��index
        std::vector<Node*>    nodes;    // ���еĸ��ڵ�nodes������meshes(������primitives(������index & material) �� transformation matrix)

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