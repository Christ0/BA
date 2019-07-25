#pragma once

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <../glm/gtx/hash.hpp>
#include "VulkanUtils.h"


class Vertex {
public:
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 uvCoord;
	glm::vec3 normal;

	Vertex(glm::vec3 pos, glm::vec3 color, glm::vec2 uvCoord, glm::vec3 normal) :
		pos(pos), color(color), uvCoord(uvCoord), normal(normal) {}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && uvCoord == other.uvCoord && normal == other.normal;
	}

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription vertexInputBindingDescription;
		vertexInputBindingDescription.binding = 0;
		vertexInputBindingDescription.stride = sizeof(Vertex);
		vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return vertexInputBindingDescription;
	}

	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions(4);
		//Vertexposition
		vertexInputAttributeDescriptions[0].location = 0; //Gleiche Location wie im Vertexshader
		vertexInputAttributeDescriptions[0].binding = 0;
		vertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; //Steht für Vec3, Vec 1 = R32, Vec 2 = R32G32 etc.
		vertexInputAttributeDescriptions[0].offset = offsetof(Vertex, pos);

		//Vertexfarbe
		vertexInputAttributeDescriptions[1].location = 1;
		vertexInputAttributeDescriptions[1].binding = 0;
		vertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributeDescriptions[1].offset = offsetof(Vertex, color);

		//UV Coords
		vertexInputAttributeDescriptions[2].location = 2;
		vertexInputAttributeDescriptions[2].binding = 0;
		vertexInputAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		vertexInputAttributeDescriptions[2].offset = offsetof(Vertex, uvCoord);

		//Normals
		vertexInputAttributeDescriptions[3].location = 3;
		vertexInputAttributeDescriptions[3].binding = 0;
		vertexInputAttributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributeDescriptions[3].offset = offsetof(Vertex, normal);

		return vertexInputAttributeDescriptions;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const &vert) const {
			size_t h1 = hash<glm::vec3>()(vert.pos);
			size_t h2 = hash<glm::vec3>()(vert.color);
			size_t h3 = hash<glm::vec2>()(vert.uvCoord);
			size_t h4 = hash<glm::vec3>()(vert.normal);

			return ((((h1 ^ (h2 << 1)) >> 1) ^ h3) << 1) ^ h4;
		}
	};
}