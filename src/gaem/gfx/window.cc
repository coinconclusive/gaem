#include <gaem/gfx/window.hh>
#include <gaem/gfx/opengl.hh>
#include <gaem/gfx/renderer.hh>

namespace gaem::gfx {
	void window::init(const char *title, glm::ivec2 size) {
		// if(!backend_glfw::is_initialized()) util::fail_error("GLFW not initlaized.");
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		window = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);
		if(window == nullptr) util::fail_error("Failed to create GLFW window.");
		glfwSetWindowUserPointer((GLFWwindow*)window, this);

		glfwSetScrollCallback(
			(GLFWwindow*)window,
				[](GLFWwindow *window, double x_offset, double y_offset) {
				auto *self = (decltype(this))glfwGetWindowUserPointer(window);
				self->delta_scroll = glm::vec2(x_offset, y_offset);
			}
		);

		glfwSetFramebufferSizeCallback(
			(GLFWwindow*)window,
			[](GLFWwindow *window, int width, int height) {
				auto *self = (decltype(this))glfwGetWindowUserPointer(window);
				self->resize_event.dispatch(glm::ivec2(width, height));
			}
		);
	}

	void window::bind() {
		glfwMakeContextCurrent((GLFWwindow*)window);
	}

	bool window::is_open() {
		return !glfwWindowShouldClose((GLFWwindow*)window);
	}

	void window::update() {
		delta_scroll = glm::vec2(0.0f, 0.0f);
		glfwSwapBuffers((GLFWwindow*)window);
	}

	void window::close() {
		return glfwSetWindowShouldClose((GLFWwindow*)window, 1);
	}

	void window::deinit() {
		// if(!gfx::backend_glfw::is_initialized())
		// 	util::fail_error("GLFW not initlaized.");
		glfwDestroyWindow((GLFWwindow*)window);
	}

	[[nodiscard]] glm::ivec2 window::size() const {
		glm::ivec2 s;
		glfwGetFramebufferSize((GLFWwindow*)window, &s.x, &s.y);
		return s;
	}

	[[nodiscard]] float window::aspect() const {
		glm::fvec2 s = size();
		return s.x / s.y;
	}

	[[nodiscard]] bool window::get_key(int key) const {
		return glfwGetKey((GLFWwindow*)window, key);
	}

	[[nodiscard]] bool window::get_mouse_button(int button) const {
		return glfwGetMouseButton((GLFWwindow*)window, button);
	}

	[[nodiscard]] glm::vec2 window::get_mouse_position() const {
		glm::dvec2 pos;
		glfwGetCursorPos((GLFWwindow*)window, &pos.x, &pos.y);
		return pos;
	}

	[[nodiscard]] glm::vec2 window::get_scroll_delta() const {
		return delta_scroll;
	}
}
