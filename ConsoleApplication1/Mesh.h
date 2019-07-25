#pragma once

#include <vector>
#include <unordered_map>
#include "Vertex.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

class Mesh {
private:
	std::vector<Vertex> m_vertices = {};

	std::vector<uint32_t> m_indices = {};

public:
	Mesh() {
		
	}

	void create(const char* path) {
		tinyobj::attrib_t vertexAttributes;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warnString;
		std::string errorString;

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
					m_vertices.push_back(vert);
				}

				m_indices.push_back(vertices[vert]);
			}
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
		return m_vertices;
	}
	std::vector<uint32_t> getIndices() {
		return m_indices;
	}
};