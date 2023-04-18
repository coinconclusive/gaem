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
		
		[[nodiscard]] size_t get_size() const override {
			return sizeof(T);
		}
		
		void load(
			res_manager &m,
			const res_id_type &id,
			const stdfs::path &path,
			const std::span<std::byte> &data
		) override {
			assert(data.size_bytes() >= sizeof(T));
			T *t = new(data.data()) T;
			t->load_from_file(m, id, path);
		}

		void unload(res_manager &m, const res_id_type &id, const std::span<std::byte> &data) override {
			assert(data.size_bytes() >= sizeof(T));
			T *t = (T*)(data.data());
			t->unload(m, id);
			t->~T();
		}
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

		[[nodiscard]] res_id_type generate_new_id();

		void new_resource(const res_id_type &id, const stdfs::path &path, strv provider);
		void new_resource(res_id_type &&id, const stdfs::path &path, strv provider);

		[[nodiscard]] res_id_type new_resource(const stdfs::path &path, strv provider);

		template<typename T>
		ref<T> new_resource(const stdfs::path &path, strv provider) {
			return ref<T>(new_resource(path, provider));
		}

		void delete_resource(const res_id_type &id);
		void delete_resource(res_id_type &&id);

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

		[[nodiscard]] res_id_type get_id_by_name(strv name) const;
		void set_name(const res_id_type &id, strv name);

		void delete_all();

		void add_dependency(const res_id_type &id, const res_id_type &dep);
		void remove_dependency(const res_id_type &id, const res_id_type &dep);

		template<typename T, typename Provider = res_provider<T>, typename ...Args>
		void register_provider(strv name, Args &&...args) {
			providers_[name.data()] = std::unique_ptr<res_provider_base>(new Provider(args...));
		}

		void unregister_provider(strv name);

		void load_from_file(const stdfs::path &path);

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
			void *maybe_load(res_manager &m, const res_id_type &id);

			/* unload the resource if it hasn't been unloaded yet and no reverse dependencies are loaded. */
			void maybe_unload(res_manager &m, const res_id_type &id);

			[[nodiscard]] std::string to_string() const;
			~res_container();
		};

		std::unordered_map<std::string, res_id_type> names_;
		std::unordered_map<std::string, std::unique_ptr<res_provider_base>> providers_;
		std::unordered_map<res_id_type, res_container, res_id_type::hash> resources_;
	};
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
	);
}

#endif
