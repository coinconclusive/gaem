#ifndef GAEM_GFX_MESH_HH_
#define GAEM_GFX_MESH_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>

namespace gaem::gfx {
	enum class mesh_mode {
		triangle_strip,
		triangle_fan,
		triangles,
	};

	class mesh {
		// friend class gfx::renderer;
		bool indexed_;
		unsigned int vao_, vbo_, ebo_;
		std::size_t vertex_count_, index_count_;
		mesh_mode mode_;
	public:
		struct vertex_type {
			glm::vec3 pos;
			glm::vec3 norm;
			glm::vec2 texcoord;

			bool operator==(const vertex_type &other) const {
				return pos == other.pos && norm == other.norm && texcoord == other.texcoord;
			}
		};

		using index_type = uint16_t;

		void unload(res::res_manager &m, const res::res_id_type &id);

		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path);
		void load_from_gltf(res::res_manager &m, const res::res_id_type &id, const char *path);
		void load_from_obj(res::res_manager &m, const res::res_id_type &id, const char *path);

		void set_vertex_attributes();

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices,
			const std::span<index_type> &indices
		);

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices
		);
	};
}

#endif
