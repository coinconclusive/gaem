#ifndef GAEM_GFX_MODEL_HH_
#define GAEM_GFX_MODEL_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>
#include <gaem/gfx/mesh.hh>
#include <gaem/gfx/material.hh>

namespace gaem::gfx {
	class model {
		// friend gfx::renderer;
		std::vector<std::pair<res_ref<gfx::mesh>, res_ref<gfx::material>>> parts;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id);
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path);
	};
}

#endif
