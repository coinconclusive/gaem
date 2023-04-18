#ifndef GAEM_GFX_WINDOW_HH_
#define GAEM_GFX_WINDOW_HH_
#include <gaem/gfx.hh>
#include <gaem/event.hh>

namespace gaem::gfx {
	class window {
		void *window;
		glm::vec2 delta_scroll = glm::vec2(0.0f, 0.0f);
		event<void(glm::ivec2 new_size)> resize_event;
	public:
		void init(const char *title, glm::ivec2 size);
		void bind();
		bool is_open();
		void update();
		void close();
		void deinit();
		[[nodiscard]] glm::ivec2 size() const;
		[[nodiscard]] float aspect() const;
		[[nodiscard]] bool get_key(int key) const;
		[[nodiscard]] bool get_mouse_button(int button) const;
		[[nodiscard]] glm::vec2 get_mouse_position() const;
		[[nodiscard]] glm::vec2 get_scroll_delta() const;

		auto get_resize_hook() -> event_hook<void(glm::ivec2)> {
			return { resize_event };
		}
	};
}

#endif
