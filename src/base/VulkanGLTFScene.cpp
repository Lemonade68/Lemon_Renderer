// include xxx_implementation in cpp files before #include "tiny_gltf.h" to 
// ����ض������⣨����header_only libraries��ע�����tinygltf֮ǰ��������.cpp��ֻ��Ҫһ�Σ�
#define TINYGLTF_IMPLEMENTATION

#include "VulkanGLTFScene.h"

namespace LR {
    VulkanglTFScene::~VulkanglTFScene() {
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
        for (Material material : materials) {
            vkDestroyPipeline(vulkanDevice->logicalDevice, material.pipeline, nullptr);
        }
    }

    /*
	glTF loading functions

	The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
*/

    void VulkanglTFScene::loadImages(tinygltf::Model& input) {
        // POI: The textures for the glTF file used in this sample are stored as external ktx files, so we can directly load them from disk without the need for conversion
        images.resize(input.images.size());
        for (size_t i = 0; i < input.images.size(); i++) {
            tinygltf::Image& glTFImage = input.images[i];
            images[i].texture.loadFromFile(path + "/" + glTFImage.uri, VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, copyQueue);
        }
    }

    void VulkanglTFScene::loadTextures(tinygltf::Model& input) {
        textures.resize(input.textures.size());
        for (size_t i = 0; i < input.textures.size(); i++) {
            textures[i].imageIndex = input.textures[i].source;
        }
    }

    void VulkanglTFScene::loadMaterials(tinygltf::Model& input) {
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
            // Get the normal map texture index
            if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
                materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
            }
            // Get some additional material parameters that are used in this sample
            materials[i].alphaMode   = glTFMaterial.alphaMode;
            materials[i].alphaCutOff = (float)glTFMaterial.alphaCutoff;
            materials[i].doubleSided = glTFMaterial.doubleSided;
        }
    }

    // ��ȡ��ǰnode�µĶ��㼰�������ݣ���ӵ�vertexBuffer�Լ�indexBuffer
    void VulkanglTFScene::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& inputModel, VulkanglTFScene::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFScene::Vertex>& vertexBuffer) {
        VulkanglTFScene::Node* node = new VulkanglTFScene::Node{};
        node->name                  = inputNode.name;
        node->parent                = parent;

        // Get the local node matrix
        // It's either made up from translation, rotation, scale or a 4x4 matrix
        node->matrix = glm::mat4(1.0f);
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
        if (inputNode.mesh > -1) {
            const tinygltf::Mesh mesh = inputModel.meshes[inputNode.mesh];
            // Iterate through all primitives of this node's mesh
            for (size_t i = 0; i < mesh.primitives.size(); i++) {
                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                uint32_t                   firstIndex    = static_cast<uint32_t>(indexBuffer.size());
                uint32_t                   vertexStart   = static_cast<uint32_t>(vertexBuffer.size());
                uint32_t                   indexCount    = 0;
                // Vertices
                {
                    const float* positionBuffer  = nullptr;
                    const float* normalsBuffer   = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    const float* tangentsBuffer  = nullptr;
                    size_t       vertexCount     = 0;

                    // Get buffer data for vertex normals
                    if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor&   accessor = inputModel.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView& view     = inputModel.bufferViews[accessor.bufferView];
                        positionBuffer                       = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
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
                    // POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
                    if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor&   accessor = inputModel.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
                        const tinygltf::BufferView& view     = inputModel.bufferViews[accessor.bufferView];
                        tangentsBuffer                       = reinterpret_cast<const float*>(&(inputModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    // Append data to model's vertex buffer
                    for (size_t v = 0; v < vertexCount; v++) {
                        Vertex vert{};
                        vert.pos     = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                        vert.normal  = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.uv      = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        vert.color   = glm::vec3(1.0f);
                        vert.tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                        vertexBuffer.push_back(vert);
                    }
                }
                // Indices
                {
                    const tinygltf::Accessor&   accessor   = inputModel.accessors[glTFPrimitive.indices];
                    const tinygltf::BufferView& bufferView = inputModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer&     buffer     = inputModel.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);

                    // glTF supports different component types of indices
                    switch (accessor.componentType) {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                            const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                            for (size_t index = 0; index < accessor.count; index++) {
                                // ΪʲôҪ����vertexStart��buf[index]������ڸ�primitive���Եģ�����primitive�ж����������ֵ������indexBuffer�е����������������model���Ե�
                                // ���Ҫ��������صĶ�������ǰ����֮ǰ�Ѿ��еĶ���������������������model���Ե�������ֵ
                                // �������ڸ�Primitive�Ķ����У�����Ϊa������vertexBuffer֮ǰ�Ѿ���¼�˱��node�Ĺ���b�Ķ��㣬����ܵ���������b+a
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
            nodes.push_back(node);
        }
    }

    VkDescriptorImageInfo VulkanglTFScene::getTextureDescriptor(const size_t index) {
        return images[index].texture.descriptorInfo;
    }

    /*
	glTF rendering functions
*/

    // Draw a single node including child nodes (if present)
    void VulkanglTFScene::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFScene::Node* node) {
        if (!node->visible) {
            return;
        }
        if (node->mesh.primitives.size() > 0) {
            // Pass the node's matrix via push constants
            // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
            glm::mat4              nodeMatrix    = node->matrix;
            VulkanglTFScene::Node* currentParent = node->parent;
            while (currentParent) {
                nodeMatrix    = currentParent->matrix * nodeMatrix;
                currentParent = currentParent->parent;
            }
            // Pass the final matrix to the vertex shader using push constants
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
            for (VulkanglTFScene::Primitive& primitive : node->mesh.primitives) {
                if (primitive.indexCount > 0) {
                    VulkanglTFScene::Material& material = materials[primitive.materialIndex];
                    // POI: Bind the pipeline for the node's material
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
                    vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                }
            }
        }
        for (auto& child : node->children) {
            drawNode(commandBuffer, pipelineLayout, child);
        }
    }

    // Draw the glTF scene starting at the top-level-nodes
    void VulkanglTFScene::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
        // All vertices and indices are stored in single buffers, so we only need to bind once
        VkDeviceSize offsets[1] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        // Render all nodes at top-level
        for (auto& node : nodes) {
            drawNode(commandBuffer, pipelineLayout, node);
        }
    }
}// namespace LR