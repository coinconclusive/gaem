#include <gaem/gfx/renderer.hh>
#include <gaem/gfx/opengl.hh>
#include <gaem/util/log.hh>
#include <gaem/gfx/model.hh>
#include <gaem/gfx/material.hh>
#include <gaem/gfx/shader.hh>
#include <gaem/gfx/mesh.hh>
#include <gaem/gfx/texture.hh>

namespace gaem::gfx {
	void backend_gl3w::init() {
		if(gl3wInit() < 0)
			util::fail_error("Failed to initialize gl3w.");
	}

	void backend_glfw::init() {
		glfwSetErrorCallback([](int error, const char *message) {
			util::print_error("GLFW Error [{}] {}", error, message);
		});

		if(!glfwInit()) util::fail_error("Failed to initialize GLFW.");
		else initialized_ = true;
	}

	void backend_glfw::poll_events() {
		glfwPollEvents();
	}

	void backend_glfw::deinit() {
		glfwTerminate();
		initialized_ = false;
	}

	[[nodiscard]] float backend_glfw::get_time() {
		return glfwGetTime();
	}

	bool backend_glfw::initialized_ = false;

	void renderer::bind_vao_(unsigned int vao) {
		if(bound_vao_ != vao)
			glBindVertexArray(bound_vao_ = vao);
	}

	void renderer::bind_program_(unsigned int program) {
		if(bound_program_ != program)
			glUseProgram(bound_program_ = program);
	}

	void renderer::bind_texture_(unsigned int unit, unsigned int texture) {
		if(texture == 0) {
			bound_textures_.erase(texture);
		} else if(!bound_textures_.contains(texture)) {
			bound_textures_.insert(texture);
			glBindTextureUnit(unit, texture);
			
		}
	}

	void renderer::set_depth_test(bool enabled) {
		if(depth_test_ && enabled) return;
		if(!depth_test_ && !enabled) return;
		if(enabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		depth_test_ = enabled;
	}

	void renderer::init() {
		depth_test_ = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
	}

	void renderer::pre_render() {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		set_depth_test(true);
	}

	void renderer::viewport(glm::vec2 vp) {
		glViewport(0.0f, 0.0f, vp.x, vp.y);
	}

	void renderer::post_render() {

	}

	void renderer::render(const gfx::mesh &mesh) {
		bind_vao_(mesh.vao_);
		if(mesh.indexed_) {
			glDrawElements((GLenum)mesh.mode_, mesh.index_count_, GL_UNSIGNED_SHORT, nullptr);
		} else {
			glDrawArrays((GLenum)mesh.mode_, 0, mesh.vertex_count_);
		}
	}

	void renderer::render(const gfx::model &model) {
		for(const auto &[mesh, material] : model.parts) {
			bind_program_(material.get_from(resman_).shader_.get_from(resman_).id_);
			render(mesh.get_from(resman_));
		}
	}

	void renderer::bind_material(gfx::material &material) {
		auto &shader = material.shader_.get_from(resman_);
		bind_shader(shader);
		for(auto &[name, param] : material.params_) {
			if(!param.dirty) continue;
			if(std::holds_alternative<int>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<int>(param.value));
			} else if(std::holds_alternative<float>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<float>(param.value));
			} else if(std::holds_alternative<glm::vec2>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<glm::vec2>(param.value));
			} else if(std::holds_alternative<glm::vec3>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<glm::vec3>(param.value));
			} else if(std::holds_alternative<glm::vec4>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<glm::vec4>(param.value));
			} else if(std::holds_alternative<glm::mat4>(param.value)) {
				shader.set_uniform(name.c_str(), std::get<glm::mat4>(param.value));
			} else assert(false && "bad material param value variant type");
			param.dirty = false;
		}
		for(const auto &texture : material.bindings_) {
			bind_texture(texture.unit, texture.texture.get_from(resman_));
		}
	}

	void renderer::bind_shader(const gfx::shader &shader) {
		bind_program_(shader.id_);
	}

	void renderer::bind_texture(int unit, const gfx::texture &texture) {
		bind_texture_(unit, texture.id_);
	}
}
