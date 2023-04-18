#ifndef GAEM_GFX_RENDERER_HH_
#define GAEM_GFX_RENDERER_HH_
#include <gaem/resource.hh>
#include <gaem/gfx.hh>
#include <set>

namespace gaem::gfx {
	class backend_gl3w {
	public:
		static void init();
	};

	class backend_glfw {
		static bool initialized_;
	public:
		static inline bool is_initialized() { return initialized_; }

		static void init();
		static void poll_events();
		static void deinit();
		[[nodiscard]] static float get_time();
	};

	class renderer {
		unsigned int bound_vao_ = 0, bound_program_ = 0;
		std::set<unsigned int> bound_textures_;
		bool depth_test_ = false;

		res::res_manager &resman_;

		void bind_vao_(unsigned int vao);
		void bind_texture_(unsigned int unit, unsigned int texture);
		void set_depth_test(bool enabled);
		void bind_program_(unsigned int program);

	public:
		renderer(res::res_manager &m) : resman_(m) {}

		void init();
		void pre_render();
		void viewport(glm::vec2 vp);
		void post_render();
		void render(const gfx::mesh &mesh);
		void render(const gfx::model &model);
		void bind_material(gfx::material &material);
		void bind_shader(const gfx::shader &shader);
		void bind_texture(int unit, const gfx::texture &texture);
	};
}

#endif
