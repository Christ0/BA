#pragma once

#include <vector>
#include <unordered_map>
#include "Vertex.h"
#include <iterator>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

class Mesh {
private:

	struct MeshGroup {
		std::vector<Vertex> m_vertices = {};

		std::vector<uint32_t> m_indices = {};
	};


	struct BufferSection {
		VkBuffer buffer = {};
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;
		BufferSection() = default;

		BufferSection(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size) :
			buffer(buffer), offset(offset), size(size){}
	};

	

	struct MeshPart {
		BufferSection vertexBufferSection = {};
		BufferSection indexBufferSection = {};
		size_t indexCount = 0;

		MeshPart(const BufferSection& vertexBufferSection, const BufferSection& indexBufferSection, size_t indexCount) :
			vertexBufferSection(vertexBufferSection), indexBufferSection(indexBufferSection), indexCount(indexCount) {}
	};
	BufferSection vertexBufferSection = {};
	BufferSection indexBufferSection = {};
	std::vector<MeshGroup> groups;
	std::vector<MeshPart> m_meshParts;
	VkBuffer buffer;
	VkBuffer stagingBufferVertex;
	VkBuffer stagingBufferIndex;
	VkDeviceMemory bufferMemory;
	VkDeviceMemory stagingBufferVertexMemory;
	VkDeviceMemory stagingBufferIndexMemory;

public:
	Mesh() {
		
	}

	std::vector<MeshGroup> create(const char* path) {
		groups.resize(1);
		tinyobj::attrib_t vertexAttributes;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warnString;
		std::string errorString;

		/*std::vector<std::unordered_map <Vertex, size_t>> uniqueVerticesPerGroup(1);
		auto appendVertex = [&uniqueVerticesPerGroup, &groups](const Vertex& vertex) {
			auto& uniqueVertices = uniqueVerticesPerGroup[1];
			auto &group = groups[1];
			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = group.m_vertices.size();
				group.m_vertices.push_back(vertex);
			}
		};*/

		bool success = tinyobj::LoadObj(&vertexAttributes, &shapes, &materials, &warnString, &errorString, path, nullptr, true, true);

		if (!success) {
			throw std::runtime_error(warnString + "\n\n\n" + errorString);
		}

		std::unordered_map<Vertex, uint32_t> vertices;

		for (tinyobj::shape_t shape : shapes) {
			for (tinyobj::index_t index : shape.mesh.indices) {
				glm::vec3 pos = {
					vertexAttributes.vertices[3 * index.vertex_index + 0],
					vertexAttributes.vertices[3 * index.vertex_index + 2], //Indices vertauscht wegen anderen Achsen in OpenGL und Vulkan, 
					vertexAttributes.vertices[3 * index.vertex_index + 1]  //sonst ist Model gedreht
				};

				glm::vec3 normal = {
					vertexAttributes.normals[3 * index.normal_index + 0],
					vertexAttributes.normals[3 * index.normal_index + 2],
					vertexAttributes.normals[3 * index.normal_index + 1]
				};

				Vertex vert(pos, glm::vec3{ 0.0f, 1.0f, 1.0f }, glm::vec2{ 0, 0 }, normal);

				if (vertices.count(vert) == 0) {
					vertices[vert] = vertices.size();
					groups[0].m_vertices.push_back(vert);
				}

				groups[0].m_indices.push_back(vertices[vert]);
			}
		}
		return groups;
	}

	void loadModelFromFile(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, VkCommandPool commandPool, const char* path, const VkDescriptorPool& descriptorPool) {
		Mesh mesh;
		VkDeviceSize bufferSize = 0;
		auto groups = create(path);
		//for (int vertex = 0, int index = 0; vertex < groups.m_vertices.size(), index < groups.m_indices.size(); vertex++, index++) {

		//}

		for (const auto& group : groups) {
			if (group.m_indices.size() <= 0) continue;
			VkDeviceSize vertexSectionSize = sizeof(group.m_vertices[0]) * group.m_vertices.size();
			VkDeviceSize indexSectionSize = sizeof(group.m_indices[0]) * group.m_indices.size();
			bufferSize += vertexSectionSize;
			bufferSize += indexSectionSize;
		}
		createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferMemory);
		VkDeviceSize currentOffset = 0;

		for (const auto& group : groups) {
			if (group.m_indices.size() <= 0) continue;
			VkDeviceSize vertexSectionSize = sizeof(group.m_vertices[0]) * group.m_vertices.size();
			VkDeviceSize indexSectionSize = sizeof(group.m_indices[0]) * group.m_indices.size();

			//copy Vertex Data
			{
				vertexBufferSection = { buffer, currentOffset, vertexSectionSize };
				auto stagingBufferVertexSize = vertexSectionSize;
				auto hostData = group.m_vertices.data();

				createBuffer(device, physicalDevice, stagingBufferVertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBufferVertex, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferVertexMemory);
				void* data;
				vkMapMemory(device, stagingBufferVertexMemory, 0, stagingBufferVertexSize, 0, &data);
				memcpy(data, hostData, stagingBufferVertexSize);
				vkUnmapMemory(device, stagingBufferVertexMemory);

				copyBuffer(device, queue, commandPool, stagingBufferVertex, buffer, stagingBufferVertexSize, 0, currentOffset);
				currentOffset += stagingBufferVertexSize;
			}

			//copy Index Data
			{
				indexBufferSection = { buffer, currentOffset, indexSectionSize };
				auto stagingBufferIndexSize = indexSectionSize;
				auto hostData = group.m_indices.data();
				VkBuffer stagingBufferModel;
				VkBuffer stagingBufferModelMemory;
				createBuffer(device, physicalDevice, stagingBufferIndexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBufferIndex, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferIndexMemory);

				void* data;
				vkMapMemory(device, stagingBufferIndexMemory, 0, stagingBufferIndexSize, 0, &data);
				memcpy(data, hostData, stagingBufferIndexSize);
				vkUnmapMemory(device, stagingBufferIndexMemory);

				copyBuffer(device, queue, commandPool, stagingBufferIndex, buffer, stagingBufferIndexSize, 0, currentOffset);

				currentOffset += stagingBufferIndexSize;
			}	
			MeshPart part{ vertexBufferSection, indexBufferSection, group.m_indices.size() };
			m_meshParts.push_back(part);
		}
	}

	/*void loadPlanes() {
		m_vertices = {
			Vertex({-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}),
			Vertex({ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}),
			Vertex({-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}),
			Vertex({ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}),

			Vertex({-0.5f, -0.5f, -1.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
			Vertex({ 0.5f,  0.5f, -1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}),
			Vertex({-0.5f,  0.5f, -1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}),
			Vertex({ 0.5f, -0.5f, -1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}),
		};

		m_indices = {
			0, 1, 2,
			0, 3, 1,

			4, 5, 6,
			4, 7, 5
		};
	}*/

	std::vector<Vertex> getVertices() {
		return groups[0].m_vertices;
	}
	std::vector<uint32_t> getIndices() {
		return groups[0].m_indices;
	}

	const std::vector<MeshPart>& getMeshParts() const{
		return m_meshParts;
	}

	void cleanup(VkDevice device) {
		vkFreeMemory(device, bufferMemory, nullptr);
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, stagingBufferVertexMemory, nullptr);
		vkDestroyBuffer(device, stagingBufferVertex, nullptr);
		vkFreeMemory(device, stagingBufferIndexMemory, nullptr);
		vkDestroyBuffer(device, stagingBufferIndex, nullptr);
	}
};