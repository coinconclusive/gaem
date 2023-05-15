#ifndef GAEM_GFX_CORE_HH_
#define GAEM_GFX_CORE_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>
#include <gaem/gfx/window.hh>

namespace gaem::gfx::core {
	using buffer_handle = void*;
	using shader_handle = void*;
	using texture_handle = void*;

	enum class primitive_type {
		triangles,
		triangle_strip,
		triangle_fan,
	};

	struct cmd_base {
		uint16_t length;
		uint16_t type;
	};

	class command_buffer {
		std::vector<uint8_t> data_;
	public:
		void cmd_bind_vertex_buffers(std::span<buffer_handle> bufs);
		void cmd_bind_index_buffer(buffer_handle buf);
		void cmd_bind_shader(shader_handle shader);
		void cmd_bind_texture(uint32_t point, texture_handle texture);
		void cmd_draw(uint32_t first_vertex, uint32_t vertex_count, primitive_type type);
		void cmd_draw_indexed(uint32_t first_index, uint32_t index_count, primitive_type type);

		class command_buffer_iterator {
			friend command_buffer;
			const uint8_t *ptr_;
			command_buffer_iterator(const uint8_t *ptr) : ptr_(ptr) {}
		public:
			cmd_base &operator*() { return *(cmd_base*)ptr_; }
			const cmd_base &operator*() const { return *(cmd_base*)ptr_; }
			cmd_base *operator->() { return &*(*this); }
			const cmd_base *operator->() const { return &*(*this); }
			command_buffer_iterator &operator++() { ptr_ += (*this)->length; return *this; }
		};

		[[nodiscard]] command_buffer_iterator begin() const { return { data_.data() }; }
		[[nodiscard]] command_buffer_iterator end() const { return { data_.data() + data_.size() }; }
	};

	class renderer {
		virtual void new_renderpass();
	};

	enum class buffer_memory {
		gpu_only,
		sequential_write,
		random_write
	};

	enum class storage_format {
		undefined,
		r8_uint,
		r8g8_uint,
		r8g8b8_uint,
		r8g8b8a8_uint,
		r8_srgb,
		r8g8_srgb,
		r8g8b8_srgb,
		r8g8b8a8_srgb,
		best_vec3_unorm,
		best_vec3_snorm,
		best_vec2_unorm,
		best_vec2_snorm,
		best_vec4_unorm,
		best_vec4_snorm,
	};

	using pipeline_stage_bit = uint32_t;
	enum class pipeline_stage_bits : pipeline_stage_bit {
		vertex_shader = 1,
		pixel_shader = 2,
		compute_shader = 4,
		all_graphics = 8,
	};

	enum class shader_module_type {
		vertex,
		pixel,
		compute
	};

	struct shader_module {
		std::span<uint32_t> code;
		shader_module_type type;
	};

	class backend {
		virtual void init(
			window *window,
			strv app_name, version app_version,
			strv engine_name, version engine_version
		);
		virtual void deinit();

		virtual renderer *get_renderer();
		virtual shader_handle new_shader(std::span<shader_module> modules);
		virtual buffer_handle new_buffer(size_t size, buffer_memory memory);
		virtual buffer_handle new_buffer(std::span<uint8_t> data, buffer_memory memory);
		virtual texture_handle new_texture(buffer_handle buffer, glm::uvec2 size, storage_format format);
		virtual void delete_shader(shader_handle shader);
		virtual void delete_buffer(buffer_handle buffer);
		virtual void delete_texture(texture_handle texture);
	};
}

#endif
