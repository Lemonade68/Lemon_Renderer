#ifndef VULKAN_GLTF_MODEL_H
#define VULKAN_GLTF_MODEL_H

#define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"

#include "renderer/Renderer.h"

namespace LR {
    // Contains everything required to render a glTF model in Vulkan
    // This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
    class VulkanglTFModel {
    public:
        // The class requires some Vulkan objects so it can create it's own resources
        LR::VulkanDevice* vulkanDevice;
        VkQueue           copyQueue;

        // The vertex layout for the samples' model
        struct Vertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
            glm::vec3 color;
        };

        // Single vertex buffer for all primitives（primitives通过index来获得对应的vertex数据），一个model只有一个vertexBuffer
        struct {
            VkBuffer       buffer;
            VkDeviceMemory memory;
        } vertices;

        // Single index buffer for all primitives，一个model只有一个indexBuffer
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
            ~Node() {
                for (auto& child : children) {
                    delete child;
                }
            }
        };

        // A glTF material stores information in e.g. the texture that is attached to it and colors
        struct Material {
            glm::vec4 baseColorFactor = glm::vec4(1.0f);
            uint32_t  baseColorTextureIndex;
        };

        // Contains the texture for a single glTF image
        // Images may be reused by texture objects and are as such separated
        struct Image {
            LR::Texture2D texture;
            // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
            VkDescriptorSet descriptorSet;
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

        ~VulkanglTFModel() {
            for (auto node : nodes) {
                delete node;
            }
            // Release all Vulkan resources allocated for the model
            vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
            vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
            vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
            vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
            for (Image image : images) {
                vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
                vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
                vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
                vkFreeMemory(vulkanDevice->logicalDevice, image.texture.memory, nullptr);
            }
        }

        /*
		glTF loading functions

		The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
	*/

        void loadImages(tinygltf::Model& input) {
            // Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
            // loading them from disk, we fetch them from the glTF loader and upload the buffers
            images.resize(input.images.size());
            for (size_t i = 0; i < input.images.size(); i++) {
                tinygltf::Image& glTFImage = input.images[i];
                // Get the image data from the glTF loader
                unsigned char* buffer       = nullptr;
                VkDeviceSize   bufferSize   = 0;
                bool           deleteBuffer = false;
                // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
                if (glTFImage.component == 3) {
                    bufferSize          = glTFImage.width * glTFImage.height * 4;
                    buffer              = new unsigned char[bufferSize];
                    unsigned char* rgba = buffer;
                    unsigned char* rgb  = &glTFImage.image[0];
                    for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
                        memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                        rgba += 4;// a那个通道的数据直接空出来即可
                        rgb += 3;
                    }
                    deleteBuffer = true;
                } else {
                    buffer     = &glTFImage.image[0];
                    bufferSize = glTFImage.image.size();
                }
                // Load texture from image buffer
                images[i].texture.loadFromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
                if (deleteBuffer) {
                    delete[] buffer;
                }
            }
        }

        void loadTextures(tinygltf::Model& input) {
            textures.resize(input.textures.size());
            for (size_t i = 0; i < input.textures.size(); i++) {
                textures[i].imageIndex = input.textures[i].source;
            }
        }

        void loadMaterials(tinygltf::Model& input) {
            materials.resize(input.materials.size());
            for (size_t i = 0; i < input.materials.size(); i++) {
                // We only read the most basic properties required for our sample
                tinygltf::Material glTFMaterial = input.materials[i];
                // Get the base color factor
                if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
                    materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
                }
                // Get base color texture index
                if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
                    materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
                }
            }
        }

        // 利用tinygltf中的结构递归加载自己写的Node(Node本身的结构就是树)
        void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& inputModel, VulkanglTFModel::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer) {
            VulkanglTFModel::Node* node = new VulkanglTFModel::Node{};
            node->matrix                = glm::mat4(1.0f);
            node->parent                = parent;

            // Get the local node matrix
            // It's either made up from translation, rotation, scale or a 4x4 matrix
            if (inputNode.translation.size() == 3) {
                node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
            }
            if (inputNode.rotation.size() == 4) {
                glm::quat q = glm::make_quat(inputNode.rotation.data());
                node->matrix *= glm::mat4(q);
            }
            if (inputNode.scale.size() == 3) {
                node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
            }
            if (inputNode.matrix.size() == 16) {
                node->matrix = glm::make_mat4x4(inputNode.matrix.data());
            };

            // Load node's children
            if (inputNode.children.size() > 0) {
                for (size_t i = 0; i < inputNode.children.size(); i++) {
                    loadNode(inputModel.nodes[inputNode.children[i]], inputModel, node, indexBuffer, vertexBuffer);
                }
            }

            // If the node contains mesh data, we load vertices and indices from the buffers
            // In glTF this is done via accessors and buffer views

            // 大于-1说明有索引到某个具体的mesh
            if (inputNode.mesh > -1) {
                const tinygltf::Mesh mesh = inputModel.meshes[inputNode.mesh];
                // Iterate through all primitives of this node's mesh
                for (size_t i = 0; i < mesh.primitives.size(); i++) {
                    const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                    uint32_t                   firstIndex    = static_cast<uint32_t>(indexBuffer.size()); //第一个新增的index在原有的indexBuffer中的位置
                    uint32_t                   vertexStart   = static_cast<uint32_t>(vertexBuffer.size());//当前已经加入vertexBuffer的vertex个数，即当前节点的顶点数据在顶点缓冲区中的起始位置
                    uint32_t                   indexCount    = 0;                                         //该node的index总个数

                    // Vertices
                    {
                        const float* positionBuffer  = nullptr;
                        const float* normalsBuffer   = nullptr;
                        const float* texCoordsBuffer = nullptr;
                        size_t       vertexCount     = 0;

                        // Get buffer data for vertex positions
                        if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                            const tinygltf::Accessor&   accessor = inputModel.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                            const tinygltf::BufferView& view     = inputModel.bufferViews[accessor.bufferView];
                            positionBuffer                       = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));//见那张图
                            vertexCount                          = accessor.count;
                        }
                        // Get buffer data for vertex normals
                        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
                            const tinygltf::Accessor&   accessor = inputModel.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                            const tinygltf::BufferView& view     = inputModel.bufferViews[accessor.bufferView];
                            normalsBuffer                        = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        }
                        // Get buffer data for vertex texture coordinates
                        // glTF supports multiple sets, we only load the first one
                        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
                            const tinygltf::Accessor&   accessor = inputModel.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                            const tinygltf::BufferView& view     = inputModel.bufferViews[accessor.bufferView];
                            texCoordsBuffer                      = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        }

                        // Append data to model's vertex buffer(没有考虑去重问题)
                        for (size_t v = 0; v < vertexCount; v++) {
                            Vertex vert{};
                            vert.pos    = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                            vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                            vert.uv     = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                            vert.color  = glm::vec3(1.0f);
                            vertexBuffer.push_back(vert);
                        }
                    }

                    // Indices
                    {
                        const tinygltf::Accessor&   accessor   = inputModel.accessors[glTFPrimitive.indices];//针对于该Primitive的accessor
                        const tinygltf::BufferView& bufferView = inputModel.bufferViews[accessor.bufferView];
                        const tinygltf::Buffer&     buffer     = inputModel.buffers[bufferView.buffer];

                        indexCount += static_cast<uint32_t>(accessor.count);

                        // glTF supports different component types of indices
                        switch (accessor.componentType) {
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                                const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    // 为什么要加上vertexStart：buf[index]是相对于该Node而言的，即该Node中顶点的索引数值，但是indexBuffer中的索引是针对于整个model而言的
                                    // 因此要在这个本地的顶点索引前加上之前已经有的顶点个数，才是相对于整个model而言的索引数值
                                    // 举例：在该Node的顶点中，索引为a，但是vertexBuffer之前已经记录了别的node的共计b的顶点，因此总的索引就是b+a
                                    indexBuffer.push_back(buf[index] + vertexStart);
                                }
                                break;
                            }
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                                const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indexBuffer.push_back(buf[index] + vertexStart);
                                }
                                break;
                            }
                            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                                const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indexBuffer.push_back(buf[index] + vertexStart);
                                }
                                break;
                            }
                            default:
                                std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                                return;
                        }
                    }
                    Primitive primitive{};
                    primitive.firstIndex    = firstIndex;
                    primitive.indexCount    = indexCount;
                    primitive.materialIndex = glTFPrimitive.material;
                    node->mesh.primitives.push_back(primitive);
                }
            }

            if (parent) {
                parent->children.push_back(node);
            } else {
                nodes.push_back(node);// 没有父节点，说明是根节点，加入nodes
            }
        }

        /*
		glTF rendering functions
	*/

        // Draw a single node including child nodes (if present)
        void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node* node) {
            if (node->mesh.primitives.size() > 0) {
                // Pass the node's matrix via push constants
                // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node(组合自己与所有父结点的matrix)
                glm::mat4              nodeMatrix    = node->matrix;
                VulkanglTFModel::Node* currentParent = node->parent;
                while (currentParent) {
                    nodeMatrix    = currentParent->matrix * nodeMatrix;
                    currentParent = currentParent->parent;
                }
                // Pass the final matrix to the vertex shader using push constants
                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
                for (VulkanglTFModel::Primitive& primitive : node->mesh.primitives) {
                    if (primitive.indexCount > 0) {
                        // Get the texture index for this primitive
                        VulkanglTFModel::Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
                        // Bind the descriptor for the current primitive's texture
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
                        vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                    }
                }
            }
            for (auto& child : node->children) {
                drawNode(commandBuffer, pipelineLayout, child);
            }
        }

        // Draw the glTF scene starting at the top-level-nodes
        void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
            // All vertices and indices are stored in single buffers, so we only need to bind once
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            // Render all nodes at top-level
            for (auto& node : nodes) {
                drawNode(commandBuffer, pipelineLayout, node);
            }
        }
    };
}// namespace LR

#endif