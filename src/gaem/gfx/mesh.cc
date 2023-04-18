#include <gaem/gfx/mesh.hh>
#include <gaem/gfx/opengl.hh>
#include <tiny_obj_loader.h>
#include <gaem/util/cgltf.hh>

namespace gaem::gfx {
	void mesh::unload(res::res_manager &m, const res::res_id_type &id) {
		if(indexed_) glDeleteBuffers(1, &ebo_);
		glDeleteBuffers(1, &vbo_);
		glDeleteVertexArrays(1, &vao_);
	}

	void mesh::load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
		clog.println("path: {}", path);
		if(path.extension() == ".gltf") {
			load_from_gltf(m, id, path.c_str());
		} else if(path.extension() == ".obj") {
			load_from_obj(m, id, path.c_str());
		} else {
			util::fail_error("Unknown file extension: {}", path.extension());
		}
	}

	void mesh::load_from_gltf(res::res_manager &m, const res::res_id_type &id, const char *path) {
		cgltf_options options {};
		cgltf_data *data = nullptr;
		cgltf_result result = cgltf_parse_file(&options, path, &data);
		if(result != cgltf_result_success) {
			util::fail_error("Failed to load gltf mesh: {}", util::cgltf::result_string(result));
		}
		// for(size_t i = 0; i < data->meshes_count; ++i) {
		// 	data->meshes[i].primitives[0].material;
		// }
		cgltf_free(data);
	}

	void mesh::load_from_obj(res::res_manager &m, const res::res_id_type &id, const char *path) {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path)) {
			util::fail_error("Failed to load .obj file:\n{}", warn + err);
		}

		std::unordered_map<vertex_type, index_type, decltype([](const vertex_type &v) {
			return (
				(std::hash<glm::vec3>()(v.pos) ^
				(std::hash<glm::vec3>()(v.norm) << 1)) >> 1
			) ^ (std::hash<glm::vec2>()(v.texcoord) << 1);
		})> unique_vertices{};

		std::vector<vertex_type> vertices;
		std::vector<index_type> indices;

		for (const auto &shape : shapes) {
			for (const auto &index : shape.mesh.indices) {
				vertex_type vertex {};
				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.norm = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};

				vertex.texcoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1]
				};

				if(unique_vertices.count(vertex) == 0) {
					unique_vertices[vertex] = vertices.size();
					vertices.push_back(vertex);
				}

				indices.push_back(unique_vertices[vertex]);
			}
		}

		load_from_data(mesh_mode::triangles, vertices, indices);
	}

	void mesh::set_vertex_attributes() {	
		glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, pos));
		glVertexArrayAttribBinding(vao_, 0, 0);
		glEnableVertexArrayAttrib(vao_, 0);

		glVertexArrayAttribFormat(vao_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, norm));
		glVertexArrayAttribBinding(vao_, 1, 0);
		glEnableVertexArrayAttrib(vao_, 1);
		
		glVertexArrayAttribFormat(vao_, 2, 2, GL_FLOAT, GL_FALSE, offsetof(vertex_type, texcoord));
		glVertexArrayAttribBinding(vao_, 2, 0);
		glEnableVertexArrayAttrib(vao_, 2);
	}

	void mesh::load_from_data(
		mesh_mode mode,
		const std::span<vertex_type> &vertices,
		const std::span<index_type> &indices
	) {
		clog.println("vertices: {}", vertices.size());
		clog.println("indices: {}", indices.size());
		indexed_ = true;
		this->mode_ = mode;
		vertex_count_ = vertices.size();
		index_count_ = indices.size();

		glCreateVertexArrays(1, &vao_);
		glCreateBuffers(1, &vbo_);
		glCreateBuffers(1, &ebo_);
		glVertexArrayElementBuffer(vao_, ebo_);
		glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(vertex_type));
		glNamedBufferData(vbo_, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
		glNamedBufferData(ebo_, indices.size_bytes(), indices.data(), GL_STATIC_DRAW);
		set_vertex_attributes();
	}

	void mesh::load_from_data(
		mesh_mode mode,
		const std::span<vertex_type> &vertices
	) {
		clog.println("vertices: {}", vertices.size());
		clog.println("indices: none");
		indexed_ = false;
		this->mode_ = mode;
		vertex_count_ = vertices.size();
		index_count_ = 0;
		glCreateVertexArrays(1, &vao_);
		glCreateBuffers(1, &vbo_);
		glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(vertex_type));
		glNamedBufferData(vbo_, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
		set_vertex_attributes();
	}
}
