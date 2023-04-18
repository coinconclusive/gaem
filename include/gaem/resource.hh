#ifndef GAEM_RESOURCE_HH_
#define GAEM_RESOURCE_HH_
#include <gaem/common.hh>
#include <gaem/util/log.hh>
#include <gaem/util/json.hh>
#include <set>
#include <utility>
#include <uuid.h>

namespace gaem::res {
	class res_id_type;
	class res_manager;

	struct res_provider_base {
		virtual ~res_provider_base() = default;
		[[nodiscard]] virtual size_t get_size() const = 0;
		virtual void load(
			res_manager &m,
			const res_id_type &id,
			const stdfs::path &path,
			const std::span<std::byte> &data
		) = 0;
		virtual void unload(res_manager &m, const res_id_type &id, const std::span<std::byte> &data) = 0;
	};
	

	template<typename T>
	concept like_resource_type = requires(T a, const stdfs::path &path, const res_id_type &id, res_manager &mngr) {
		a.load_from_file(mngr, id, path);
		a.unload(mngr, id);
	};

	template<like_resource_type T>
	struct res_provider : res_provider_base {
		using res_type = T;
		
		[[nodiscard]] size_t get_size() const override;
		// size_t get_size() const override { return sizeof(T); }
		
		void load(
			res_manager &m,
			const res_id_type &id,
			const stdfs::path &path,
			const std::span<std::byte> &data
		) override;
		// void load(
		// 	res_manager &m,
		// 	const res_id_type &id,
		// 	const stdfs::path &path,
		// 	const std::span<std::byte> &data
		// ) override {
		// 	assert(data.size_bytes() >= sizeof(T));
		// 	T *t = new(data.data()) T;
		// 	t->load_from_file(m, id, path);
		// }

		void unload(res_manager &m, const res_id_type &id, const std::span<std::byte> &data) override;
		// void unload(res_manager &m, const res_id_type &id, const std::span<std::byte> &data) override {
		// 	assert(data.size_bytes() >= sizeof(T));
		// 	T *t = (T*)(data.data());
		// 	t->unload(m, id);
		// 	t->~T();
		// }
	};

	class res_id_type {
		uuids::uuid id;
	public:
		res_id_type() = default;
		res_id_type(uuids::uuid &&m) : id(std::move(m)) {}
		res_id_type(const uuids::uuid &m) : id(m) {}
		res_id_type(res_id_type &&m) : id(std::move(std::move(m).id)) {}
		res_id_type(const res_id_type &m) = default;

		res_id_type &operator=(const res_id_type &m) = default;
		res_id_type &operator=(res_id_type &&m) { id = std::move(m.id); return *this; }
		res_id_type &operator=(const uuids::uuid &m) { id = m; return *this; }
		res_id_type &operator=(uuids::uuid &&m) { id = std::move(m); return *this; }

		void print(std::ostream &os) const { os << id; }
		bool operator==(const res_id_type &other) const { return id == other.id; }

		[[nodiscard]] std::string to_string() const;
		// std::string to_string() const {
		// 	std::ostringstream ss;
		// 	print(ss);
		// 	return ss.str();
		// }

		struct hash {
			std::size_t operator()(const res::res_id_type &id) const {
				return std::hash<uuids::uuid>{}(id.id);
			}
		};
	};

	class res_manager {
	public:
		friend struct ref;

		template<like_resource_type T>
		struct ref {
			res_id_type id;

			ref() = default;

			ref(res_id_type &&id) : id(std::move(id)) {}
			ref(const res_id_type &id) : id(id) {}

			ref(const ref &other) : id(other.id) {}
			ref(ref &&other) : id(std::move(std::move(other).id)) {}

			ref &operator=(const ref &other) {
				id = other.id;
				return *this;
			}

			void preload_from(res_manager &m) const {
				m.resources_.find(id)->second.maybe_load(m, id);
			}

			[[nodiscard]] T &get_from(res_manager &m) const {
				return *(T*)m.resources_.find(id)->second.maybe_load(m, id);
			}

			void context_from(res_manager &m, const auto &callable) {
				callable(get_from(m));
			}
		};

		res_id_type generate_new_id() { return generator_(); }


		void new_resource(const res_id_type &id, const stdfs::path &path, strv provider) {
			resources_.emplace(id, res_container(id, path, providers_[provider.data()].get()));
		}

		void new_resource(res_id_type &&id, const stdfs::path &path, strv provider) {
			clog.println("New resource:");
			clog.indent();
			clog.println("id: {{{}}}", id.to_string());
			clog.println("path: {}", path);
			clog.println("provider: {}", provider);
			clog.dedent();
			resources_.emplace(std::move(id), res_container(id, path, providers_[provider.data()].get()));
		}

		res_id_type new_resource(const stdfs::path &path, strv provider) {
			res_id_type id = generate_new_id();
			new_resource(id, path, provider);
			return id;
		}

		template<typename T>
		ref<T> new_resource(const stdfs::path &path, strv provider) {
			return ref<T>(new_resource(path, provider));
		}

		void delete_resource(const res_id_type &id) {
			resources_.find(id)->second.maybe_unload(*this, id);
		}

		void delete_resource(res_id_type &&id) {
			resources_.find(std::move(id))->second.maybe_unload(*this, id);
		}

		template<typename T>
		void delete_resource(/* const */ ref<T> &r) { delete_resource(r.id); }

		template<like_resource_type T>
		ref<T> get_resource(const res_id_type &id) { return ref<T>(id); }

		template<like_resource_type T>
		ref<T> get_resource(res_id_type &&id) { return ref<T>(std::move(id)); }

		template<like_resource_type T>
		ref<T> get_resource(strv name) {
			if(!names_.contains(name.data()))
				throw std::runtime_error("no such resource: '"s + name.data() + "'");
			return get_resource<T>(names_[name.data()]);
		}

		res_id_type get_id_by_name(strv name) const {
			if(!names_.contains(name.data()))
				throw std::runtime_error("no such resource: '"s + name.data() + "'");
			return names_.find(name.data())->second;
		}

		void set_name(const res_id_type &id, strv name) {
			if(!resources_.contains(id))
				throw std::runtime_error("no such resource: '"s + id.to_string() + "'");
			names_.emplace(name, id);
			resources_.find(id)->second.name = name;
		}

		void delete_all() {
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

		void add_dependency(const res_id_type &id, const res_id_type &dep) {
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

		void remove_dependency(const res_id_type &id, const res_id_type &dep) {
			if(!resources_.contains(id))
				throw std::runtime_error("no such resource: '"s + id.to_string() + "'");
			resources_.find(id)->second.deps.erase(dep);
			if(!resources_.contains(dep))
				throw std::runtime_error("no such resource: '"s + dep.to_string() + "'");
			resources_.find(dep)->second.rdeps.erase(id);
		}

		template<typename T, typename Provider = res_provider<T>, typename ...Args>
		void register_provider(strv name, Args &&...args) {
			providers_[name.data()] = std::unique_ptr<res_provider_base>(new Provider(args...));
		}

		void unregister_provider(strv name) { providers_.erase(name.data()); }

		void load_from_file(const stdfs::path &path) {
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
					::util::fail_error("Unknown provider: '{}'.", item["provider"]);
				
				util::json::assert_contains(item, "uuid");
				util::json::assert_type(item["uuid"], util::json::value_kind::string);
				auto uuid = uuids::uuid::from_string(item["uuid"].get_ref<const std::string &>());
				if(!uuid.has_value()) ::util::fail_error("Invalid UUID: '{}'.", res["shader"]);

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

		res_manager() : generator_(rand_engine_) {}
	private:
		static std::mt19937 rand_engine_;
		uuids::uuid_random_generator generator_;

		struct res_container {
			res_id_type id; /* resource id. */
			stdfs::path path; /* path to resource. */
			std::optional<std::string> name; /* resource name. */
			res_provider_base *provider; /* resource provider. */
			using set_cmp_ = decltype([](const res_id_type &a, const res_id_type &b) -> bool {
				return res_id_type::hash{}(a) < res_id_type::hash{}(b);
			});
			std::set<res_id_type, set_cmp_> deps; /* dependencies. */
			std::set<res_id_type, set_cmp_> rdeps; /* reverse dependencies. */

			std::byte *data = nullptr; /* resource data as an opaque pointer. */
			bool loaded = false; /* true if resource has been loaded, false otherwise. */

			res_container(
				res_id_type id,
				stdfs::path path,
				res_provider_base *provider
			) : id(std::move(id)), path(std::move(path)), provider(provider) {}

			/* load the resource if it hasn't been loaded yet. will also load dependencies. */
			void *maybe_load(res_manager &m, const res_id_type &id) {
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

			/* unload the resource if it hasn't been unloaded yet and no reverse dependencies are loaded. */
			void maybe_unload(res_manager &m, const res_id_type &id) {
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

			[[nodiscard]] std::string to_string() const {
				if(name.has_value()) return "'" + name.value() + "'";
				return "{" + id.to_string() + "}";
			}

			~res_container() {
				if(loaded) {
					::util::print_error("Resource leak: {}.", to_string());
				}
			}
		};

		std::unordered_map<std::string, res_id_type> names_;
		std::unordered_map<std::string, std::unique_ptr<res_provider_base>> providers_;
		std::unordered_map<res_id_type, res_container, res_id_type::hash> resources_;
	};

	std::mt19937 res_manager::rand_engine_{};
}

namespace gaem {
	template<typename T>
	using res_ref = res::res_manager::ref<T>;
}

namespace gaem::util::json {
	/** load either name field (and convert to uuid by using a resource manager)
	  * or load a uuid field from json object. */
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
				::util::fail_error("Invalid UUID: '{}'.", j[uuid_field]);
			id = std::move(uuid).value();
		} else { // no field present, fail.
			::util::fail_error("No '{}' or '{}' fields.", name_field, uuid_field);
		}
	}
}

#endif
