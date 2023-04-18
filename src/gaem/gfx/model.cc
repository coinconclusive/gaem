#include <gaem/gfx/model.hh>

namespace gaem::gfx {
	void model::unload(res::res_manager &m, const res::res_id_type &id) {}
	void model::load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
		clog.println("path: {}", path);
		auto res = util::json::read_file(path);
		util::json::assert_type(res, util::json::value_kind::object);
		util::json::assert_contains(res, "parts");
		util::json::assert_type(res["parts"], util::json::value_kind::array);
		for(auto &part : res["parts"]) {
			util::json::assert_type(part, util::json::value_kind::object);
			util::json::assert_contains(part, "mesh");
			util::json::assert_contains(part, "material");
		}
	}
}
