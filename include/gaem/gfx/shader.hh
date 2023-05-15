#ifndef GAEM_GFX_SHADER_HH_
#define GAEM_GFX_SHADER_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>

namespace gaem::gfx {
	class shader {
		// friend renderer;
		unsigned int id_;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id);
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &general_path);

		void set_uniform(const char *name, int v) const;
		void set_uniform(const char *name, float v) const;
		void set_uniform(const char *name, glm::vec2 v) const;
		void set_uniform(const char *name, glm::vec3 v) const;
		void set_uniform(const char *name, glm::vec4 v) const;
		void set_uniform(const char *name, const glm::mat4 &v) const;
	};
}

#endif
