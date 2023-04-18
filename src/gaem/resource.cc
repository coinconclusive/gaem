#include <gaem/resource.hh>

namespace gaem::res {
	[[nodiscard]] std::string res_id_type::to_string() const {
		std::ostringstream ss;
		print(ss);
		return ss.str();
	}

	[[nodiscard]] res_id_type res_manager::generate_new_id() {
		return generator_();
	}

	void res_manager::new_resource(
		const res_id_type &id,
		const stdfs::path &path,
		strv provider
	) {
		resources_.emplace(id, res_container(id, path, providers_[provider.data()].get()));
	}
	
	void res_manager::new_resource(
		res_id_type &&id,
		const stdfs::path &path,
		strv provider
	) {
		clog.println("New resource:");
		clog.indent();
		clog.println("id: {{{}}}", id.to_string());
		clog.println("path: {}", path);
		clog.println("provider: {}", provider);
		clog.dedent();
		resources_.emplace(std::move(id), res_container(id, path, providers_[provider.data()].get()));
	}

	[[nodiscard]] res_id_type res_manager::new_resource(
		const stdfs::path &path,
		strv provider
	) {
		res_id_type id = generate_new_id();
		new_resource(id, path, provider);
		return id;
	}
	
	void res_manager::delete_resource(const res_id_type &id) {
		resources_.find(id)->second.maybe_unload(*this, id);
	}

	void res_manager::delete_resource(res_id_type &&id) {
		resources_.find(std::move(id))->second.maybe_unload(*this, id);
	}

	[[nodiscard]] res_id_type res_manager::get_id_by_name(strv name) const {
		if(!names_.contains(name.data()))
			throw std::runtime_error("no such resource: '"s + name.data() + "'");
		return names_.find(name.data())->second;
	}

	void res_manager::set_name(const res_id_type &id, strv name) {
		if(!resources_.contains(id))
			throw std::runtime_error("no such resource: '"s + id.to_string() + "'");
		names_.emplace(name, id);
		resources_.find(id)->second.name = name;
	}

	void res_manager::delete_all() {
		bool any_loaded;
		do {
			any_loaded = false;
			for(auto &[id, container] : resources_) {
				if(container.loaded) {
					any_loaded = true;
					container.maybe_unload(*this, id);
				}
			}
		} while(any_loaded);
	}

	void res_manager::add_dependency(
		const res_id_type &id,
		const res_id_type &dep
	) {
		if(!resources_.contains(id))
			throw std::runtime_error("no such resource: '"s + id.to_string() + "'");
		if(!resources_.contains(dep))
			throw std::runtime_error("no such resource: '"s + dep.to_string() + "'");
		auto &primary = resources_.find(id)->second;
		auto &secondary = resources_.find(dep)->second;
		primary.deps.insert(dep);
		secondary.rdeps.insert(id);
		clog.println("New dependency: {} on {}.", primary.to_string(), secondary.to_string());
	}

	void res_manager::remove_dependency(
		const res_id_type &id,
		const res_id_type &dep
	) {
		if(!resources_.contains(id))
			throw std::runtime_error("no such resource: '"s + id.to_string() + "'");
		resources_.find(id)->second.deps.erase(dep);
		if(!resources_.contains(dep))
			throw std::runtime_error("no such resource: '"s + dep.to_string() + "'");
		resources_.find(dep)->second.rdeps.erase(id);
	}

	void res_manager::unregister_provider(strv name) {
		providers_.erase(name.data());
	}

	void res_manager::load_from_file(const stdfs::path &path) {
		auto res = util::json::read_file(path);
		util::json::assert_type(res, util::json::value_kind::object);
		util::json::assert_contains(res, "resources");
		util::json::assert_type(res["resources"], util::json::value_kind::array);
		for(const auto &item : res["resources"]) {
			util::json::assert_type(item, util::json::value_kind::object);
			util::json::assert_contains(item, "provider");
			util::json::assert_type(item["provider"], util::json::value_kind::string);
			auto provider = item["provider"].get_ref<const std::string &>();
			if(!providers_.contains(provider))
				util::fail_error("Unknown provider: '{}'.", item["provider"]);
			
			util::json::assert_contains(item, "uuid");
			util::json::assert_type(item["uuid"], util::json::value_kind::string);
			auto uuid = uuids::uuid::from_string(item["uuid"].get_ref<const std::string &>());
			if(!uuid.has_value()) util::fail_error("Invalid UUID: '{}'.", res["shader"]);

			util::json::assert_contains(item, "path");
			util::json::assert_type(item["path"], util::json::value_kind::string);
			auto path = item["path"].get_ref<const std::string &>();

			new_resource(uuid.value(), path, provider); // (1)

			if(item.contains("name")) {
				util::json::assert_type(item["name"], util::json::value_kind::string);
				auto name = item["name"].get_ref<const std::string &>();
				clog.indent(); // indented because it binds to (1)
				clog.println("name: {}", name);
				clog.dedent();
				set_name(uuid.value(), name);
			}
		}
	}

	void *res_manager::res_container::maybe_load(
		res_manager &m,
		const res_id_type &id
	) {
		if(loaded) return data;
		clog.println("Trying to load {}.", to_string());
		clog.indent();
		data = new std::byte[provider->get_size()];
		clog.println("Loading...");
		clog.indent();
		provider->load(m, id, path, std::span<std::byte>(data, provider->get_size()));
		clog.dedent();
		for(const auto &dep : deps) {
			clog.println("Dependency: {}.", dep.to_string());
			if(auto it = m.resources_.find(dep); it != m.resources_.end()) {
				clog.indent();
				it->second.maybe_load(m, it->first);
				clog.dedent();
			}
		}
		loaded = true;
		clog.dedent();
		return data;
	}
	
	void res_manager::res_container::maybe_unload(
		res_manager &m,
		const res_id_type &id
	) {
		if(!loaded) return;
		clog.println("Trying to unload {}.", to_string());
		clog.indent();
		for(const auto &dep : rdeps) {
			if(auto it = m.resources_.find(dep); it != m.resources_.end()) {
				if(it->second.loaded) {
					clog.println("Will not unload due to rev. dependencies.");
					clog.dedent();
					return; // do not unload, reverse dependency still loaded.
				}
			}
		}
		clog.println("Unloading...");
		clog.indent();
		provider->unload(m, id, std::span<std::byte>(data, provider->get_size()));
		clog.dedent();
		delete data;
		loaded = false;
		clog.dedent();
	}

	res_manager::res_container::~res_container() {
		if(loaded) {
			util::print_error("Resource leak: {}.", to_string());
		}
	}

	[[nodiscard]] std::string res_manager::res_container::to_string() const {
		if(name.has_value()) return "'" + name.value() + "'";
		return "{" + id.to_string() + "}";
	}

	std::mt19937 res_manager::rand_engine_{};
}

namespace gaem::util::json {
	void read_res_name_or_uuid(
		const nmann::json &j,
		strv name_field, strv uuid_field,
		res::res_manager &m,
		res::res_id_type &id
	) {
		if(j.contains(name_field)) { // name field present.
			util::json::assert_type(j[name_field], util::json::value_kind::string);
			auto name = j[name_field].get_ref<const std::string &>();
			id = m.get_id_by_name(name);
		} else if(j.contains(uuid_field)) { // uuid field present.
			util::json::assert_type(j[uuid_field], util::json::value_kind::string);
			auto uuid = uuids::uuid::from_string(j[uuid_field].get_ref<const std::string &>());
			if(!uuid.has_value()) // must be a valid uuid.
				util::fail_error("Invalid UUID: '{}'.", j[uuid_field]);
			id = std::move(uuid).value();
		} else { // no field present, fail.
			util::fail_error("No '{}' or '{}' fields.", name_field, uuid_field);
		}
	}
}
