#ifndef GAEM_GFX_RENDERGRAPH_HH_
#define GAEM_GFX_RENDERGRAPH_HH_
#include <gaem/gfx/core.hh>
#include <gaem/common.hh>
#include <gaem/gfx.hh>

namespace gaem::gfx {
	class render_graph {
	public:
		enum class attachment_size_type {
			one_by_one,
			swapchain
		};

		struct attachment_info {
			attachment_size_type size_type = attachment_size_type::swapchain;
			glm::vec2 size_multiplier = { 1.0f, 1.0f };
			core::storage_format format = core::storage_format::undefined;
			unsigned int samples = 1;
			unsigned int levels = 1;
			unsigned int layers = 1;
		};

		class pass_iface {
		public:
			void add_color_input(strv name);
			void add_color_output(strv name);

			render_graph &graph() { return render_graph_; }
		private:
			friend render_graph;
			pass_iface(render_graph &graph) : render_graph_(graph) { }

			render_graph &render_graph_;
		};

		pass_iface &add_pass(strv name, core::pipeline_stage_bit stages);
	};
}

#endif
