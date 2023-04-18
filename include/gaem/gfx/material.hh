#ifndef GAEM_GFX_MATERIAL_HH_
#define GAEM_GFX_MATERIAL_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>
#include <gaem/resource.hh>
#include <gaem/gfx/texture.hh>
#include <gaem/gfx/shader.hh>

namespace gaem::gfx {
	class material {
		friend gfx::renderer;

		struct texture_binding {
			using unit_type = unsigned int;
			res_ref<texture> texture;
			unit_type unit;
		};

		using value_type = std::variant<
			int,
			float,
			glm::vec2,
			glm::vec3,
			glm::vec4,
			glm::mat4
		>;

		// note: sizeof(param_type) is large-ish.
		struct param_type {
			value_type value;
			bool dirty = true;

			template<typename T>
			void set(const T &t) {
				value = t;
				dirty = true;
			}
		};

		std::unordered_map<std::string, param_type> params_;
		std::vector<texture_binding> bindings_;
		res_ref<shader> shader_;

		/* remove duplicate textures, keeping the ones closer to the end. */
		void remove_duplicate_textures_(res::res_manager &m, const res::res_id_type &id);
	public:
		void set(strv name, float v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec2 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec3 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec4 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::mat4 &v) { params_[name.data()].set(v); }

		void unload(res::res_manager &m, const res::res_id_type &id);
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path);
	};
}

#endif
