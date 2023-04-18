#include <gaem/gfx/material.hh>
#include <gaem/gfx/opengl.hh>

namespace gaem::gfx {
	void material::remove_duplicate_textures_(res::res_manager &m, const res::res_id_type &id) {
		std::vector<int> to_delete; // indices that will be deleted.
		std::unordered_map<texture_binding::unit_type, int> used_units; // unit -> last index using it.
		used_units.reserve(bindings_.size()); // too much, but will fit all.
		for(int i = 0; i < bindings_.size(); ++i) {
			const auto &binding = bindings_[i];
			auto unit = used_units[binding.unit];
			if(used_units.contains(unit)) {
				m.remove_dependency(id, binding.texture.id);
				to_delete.push_back(i);
				used_units[unit] = i;
			}
		}

		for(int index : to_delete) {
			bindings_.erase(bindings_.begin() + index);
		}
	}

	void material::unload(res::res_manager &m, const res::res_id_type &id) {

	}

	void material::load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
		clog.println("path: {}", path);
		auto res = util::json::read_file(path);
		util::json::assert_type(res, util::json::value_kind::object);
		if(res.contains("inherit")) {
			util::json::assert_type(res["inherit"],
				util::json::value_kind::array,
				util::json::value_kind::object);

			const auto &load_base_material = [&](const auto &j) {
				util::json::assert_type(j, util::json::value_kind::object);
				res::res_id_type base_id;
				util::json::read_res_name_or_uuid(j, "name", "uuid", m, base_id);
				m.get_resource<gfx::material>(std::move(base_id))
					.context_from(m, [&](const gfx::material &mat) {
						shader_ = mat.shader_;
						params_.insert(mat.params_.begin(), mat.params_.end());
						bindings_.insert(mat.bindings_.end(), mat.bindings_.begin(), mat.bindings_.end());
					});
			};

			if(res["inherit"].is_array()) { // array of objects.
				for(const auto &base_json : res["inherit"])  {
					load_base_material(base_json);
				}
			} else { // single object.
				load_base_material(res["inherit"]);
			}
		}

		if(res.contains("shader")) {
			util::json::read_res_name_or_uuid(res, "shader", "shader-uuid", m, shader_.id);
			m.add_dependency(id, shader_.id);
		}

		if(res.contains("modules")) {
			util::json::assert_type(res["modules"], util::json::value_kind::array);
			for(const auto &module_json : res["modules"]) {
				util::json::assert_type(module_json, util::json::value_kind::string);
				stdfs::path module_path =
					path.parent_path() / module_json.get_ref<const std::string &>();
				
			}
		}

		if(res.contains("params")) {
				util::json::assert_type(res["params"], util::json::value_kind::object);
			for(auto &[key, value] : res["params"].items()) {
				util::json::assert_type(value,
					util::json::value_kind::number,
					util::json::value_kind::array,
					util::json::value_kind::string);
				if(value.is_number()) {
					params_[key].value = value.get<int>();
				} else if(value.is_string()) {
					const auto &type = value.get_ref<const std::string&>();
					if(type == "") params_[key].value = 0;
					else if(type == "int") params_[key].value = 0;
					else if(type == "float") params_[key].value = 0.0f;
					else if(type == "vec2") params_[key].value = glm::vec2{};
					else if(type == "vec3") params_[key].value = glm::vec2{};
					else if(type == "vec4") params_[key].value = glm::vec2{};
					else if(type == "mat4") params_[key].value = glm::vec2{};
					else {
						util::fail_error("Invalid material parameter type: '{}', "
							"must be one of [int, float, vec2, vec3, vec4, mat4]", type);
					}
				} else {
					if(value.size() == 1) params_[key].value = value[0].get<float>();
					else if(value.size() == 2) {
						params_[key].value = glm::vec2(
							value[0].get<float>(),
							value[1].get<float>()
						);
					} else if(value.size() == 3) {
						params_[key].value = glm::vec3(
							value[0].get<float>(),
							value[1].get<float>(),
							value[2].get<float>()
						);
					} else if(value.size() == 4) {
						params_[key].value = glm::vec4(
							value[0].get<float>(),
							value[1].get<float>(),
							value[2].get<float>(),
							value[3].get<float>()
						);
					} else {
						util::fail_error("Invalid number of vector items: {} is not in [1, 4].", value.size());
					}
				}
			}
		}

		if(res.contains("textures")) {
			util::json::assert_type(res["textures"], util::json::value_kind::array);
			for(const auto &tex_json : res["textures"]) {
				util::json::assert_type(tex_json, util::json::value_kind::object);
				util::json::assert_contains(tex_json, "unit");
				util::json::assert_type(tex_json["unit"], util::json::value_kind::number);
				bindings_.push_back({});
				bindings_.back().unit = tex_json["unit"].get<unsigned int>();
				util::json::read_res_name_or_uuid(tex_json, "name", "uuid", m, bindings_.back().texture.id);
				m.add_dependency(id, bindings_.back().texture.id);
			}
		}

		remove_duplicate_textures_(m, id);
	}
}
