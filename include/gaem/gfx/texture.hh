#ifndef GAEM_GFX_TEXTURE_HH_
#define GAEM_GFX_TEXTURE_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>

namespace gaem::gfx {
	class texture {
		// friend renderer;
		unsigned int id_;
		glm::ivec2 size_;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id);
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path);
		[[nodiscard]] glm::ivec2 get_size() const { return size_; }
	};
}

#endif
