#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <nlohmann_json.hpp>
#include <tiny_obj_loader.h>
#include <cgltf.h>
#include <uuid.h>

#include <cstdio>
#include <string_view>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <span>
#include <variant>
#include <utility>
#include <unordered_map>
#include <set>
// #include <chrono>

namespace stdfs = std::filesystem;
namespace nmann = nlohmann;
using namespace std::literals;
// namespace stdch = std::chrono;

namespace res { class res_id_type; }
std::ostream &operator<<(std::ostream &os, const ::res::res_id_type &id);

namespace util {
	template<typename T>
	class event_hook;

	template<typename T>
	class event {
		friend event_hook<T>;
	public:
		template<typename ...Args> requires std::invocable<std::function<T>, Args...>
		void dispatch(Args &&...args) const {
			for(const auto &handler : handlers)
				handler(std::forward<Args>(args)...);    
		}
	private:
		std::vector<std::function<T>> handlers;
	};

	template<typename T>
	class event_hook {
		::util::event<T> &event;
	public:
		event_hook(::util::event<T> &event) : event(event) {}

		template<typename F>
		void add(F &&f) {
			event.handlers.push_back(std::move(f));
		}

		template<typename F>
		void add(const F &f) {
			event.handlers.push_back(f);
		}
	};


	template<typename A, typename B>
	struct combined { const A &a; const B &b; };

	template<typename A, typename B>
	std::ostream &operator<<(std::ostream &os, const combined<A, B> &p) {
		os << p.a << p.b;
		return os;
	}

	template<typename ...Ts>
	void print_error(Ts ...ts) {
		std::cerr << "\033[31mError\33[m: ";
		(std::cerr << ... << std::forward<Ts>(ts));
		std::cerr << std::endl;
	}

	void fail() {
		exit(1);
	}

	template<typename ...Ts>
	void fail_error(Ts ...ts) {
		print_error(std::forward<Ts>(ts)...);
		fail();
	}

	const char *cgltf_result_string(cgltf_result res) {
		switch(res) {
		case cgltf_result_success: return "success";
		case cgltf_result_data_too_short: return "data too short";
		case cgltf_result_unknown_format: return "unknown format";
		case cgltf_result_invalid_json: return "invalid json";
		case cgltf_result_invalid_gltf: return "invalid gltf";
		case cgltf_result_invalid_options: return "invalid options";
		case cgltf_result_file_not_found: return "file not found";
		case cgltf_result_io_error: return "io error";
		case cgltf_result_out_of_memory: return "out of memory";
		case cgltf_result_legacy_gltf: return "legacy gltf";
		default: break;
		}
		return "unknown";
	};

	nmann::json read_json_file(const stdfs::path &path) {
		std::ifstream inp(path);
		inp.exceptions(std::ios_base::badbit);
		return nmann::json::parse(inp);
	}

	enum class json_value_type { null, object, array, string, number, boolean };

	json_value_type nmann_type_to_json_type(nmann::json::value_t v) {
		switch(v) {
		case nlohmann::detail::value_t::null: return json_value_type::null;
		case nlohmann::detail::value_t::object: return json_value_type::object;
		case nlohmann::detail::value_t::array: return json_value_type::array;
		case nlohmann::detail::value_t::string: return json_value_type::string;
		case nlohmann::detail::value_t::boolean:  return json_value_type::boolean;
		case nlohmann::detail::value_t::number_integer:
		case nlohmann::detail::value_t::number_unsigned:
		case nlohmann::detail::value_t::number_float:
		case nlohmann::detail::value_t::binary:
		case nlohmann::detail::value_t::discarded:
			return json_value_type::number;
		}
	}

	const char *json_typename(json_value_type type) {
		switch(type) {
		case json_value_type::null: return "null";
		case json_value_type::object: return "object";
		case json_value_type::array: return "array";
		case json_value_type::string: return "string";
		case json_value_type::number: return "number";
		case json_value_type::boolean: return "boolean";
		}
	}

	template<std::convertible_to<json_value_type> ...Ts>
	bool json_check_type(const nmann::json &j, json_value_type first, Ts &&...rest) {
		auto jtype = nmann_type_to_json_type(j.type());
		std::array<json_value_type, 1 + sizeof...(Ts)> types { first, rest... };
		for(auto t : types) {
			if(jtype == t) return true;
		}
		print_error(
			"expected one of ",
			json_typename(first),
			combined<const char*, const char*>{ ", ", json_typename(rest) }...,
			", but got ",
			json_typename(jtype)
		);
		return false;
	}

	template<std::convertible_to<json_value_type> ...Ts>
	void json_assert_type(const nmann::json &j, json_value_type first, Ts &&...rest) {
		if(!json_check_type(j, first, std::forward<Ts>(rest)...)) fail();
	}

	void json_assert_contains(const nmann::json &j, const nmann::json::object_t::key_type &key) {
		if(!j.contains(key)) fail_error("No required key: ", key);
	}

	void json_assert_contains(const nmann::json &j, size_t index) {
		if(index >= j.size()) fail_error("Array too small: ", index, " >= ", j.size());
	}

	auto read_file(const stdfs::path &path) -> std::vector<char> {
		std::vector<char> v(stdfs::file_size(path));
		auto stream = std::ifstream(path);
		stream.exceptions(std::ios_base::badbit);
		stream.read(v.data(), v.size());
		return v;
	}
}

namespace res {
	class res_manager;

	struct res_provider_base {
		virtual ~res_provider_base() {}
		virtual size_t get_size() const = 0;
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
		
		size_t get_size() const override { return sizeof(T); }
		
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
		friend struct std::hash<::res::res_id_type>;
	public:
		res_id_type() {}
		res_id_type(uuids::uuid &&m) : id(std::move(m)) {}
		res_id_type(const uuids::uuid &m) : id(m) {}
		res_id_type(res_id_type &&m) : id(std::move(std::move(m).id)) {}
		res_id_type(const res_id_type &m) : id(m.id) {}

		res_id_type &operator=(res_id_type &&m) {
			new(this) res_id_type(std::move(m));
			return *this;
		}

		res_id_type &operator=(uuids::uuid &&m) {
			new(this) res_id_type(std::move(m));
			return *this;
		}

		void print(std::ostream &os) const { os << id; }
		bool operator==(const res_id_type &other) const { return id == other.id; }

		std::string to_string() const {
			std::ostringstream ss;
			print(ss);
			return ss.str();
		}

		struct hash {
			std::size_t operator()(const ::res::res_id_type &id) const {
				return std::hash<uuids::uuid>{}(id.id);
			}
		};
	};

	std::ostream &operator<<(std::ostream &os, const ::res::res_id_type &id) {
		id.print(os);
		return os;
	}

	class res_manager {
	public:
		friend struct ref;

		template<like_resource_type T>
		struct ref {
			res_id_type id;

			ref() {}

			ref(res_id_type &&id) : id(std::move(id)) {}
			ref(const res_id_type &id) : id(id) {}

			void preload_from(res_manager &m) const {
				m.resources_.find(id)->second.maybe_load(m, id);
			}

			[[nodiscard]] const T &get_from(res_manager &m) const {
				return *(T*)m.resources_.find(id)->second.maybe_load(m, id);
			}

			void context_from(res_manager &m, const auto &callable) {
				callable(get_from(m));
			}
		};

		res_id_type generate_new_id() { return generator_(); }


		void new_resource(const res_id_type &id, const stdfs::path &path, std::string_view provider) {
			resources_.emplace(id, res_container(id, path, providers_[provider.data()].get()));
		}

		void new_resource(res_id_type &&id, const stdfs::path &path, std::string_view provider) {
			std::cerr << "[info] new resource {" << id << "} @" << path << " : " << provider << std::endl;
			resources_.emplace(std::move(id), res_container(id, path, providers_[provider.data()].get()));
		}

		res_id_type new_resource(const stdfs::path &path, std::string_view provider) {
			res_id_type id = generate_new_id();
			new_resource(id, path, provider);
			return id;
		}

		template<typename T>
		ref<T> new_resource(const stdfs::path &path, std::string_view provider) {
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
		ref<T> get_resource(std::string_view name) {
			if(!names_.contains(name.data()))
				throw std::runtime_error("no such resource: '"s + name.data() + "'");
			return get_resource<T>(names_[name.data()]);
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
			resources_.find(id)->second.deps.insert(dep);
			if(!resources_.contains(dep))
				throw std::runtime_error("no such resource: '"s + dep.to_string() + "'");
			resources_.find(dep)->second.rdeps.insert(id);
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
		void register_provider(std::string_view name, Args &&...args) {
			providers_[name.data()] = std::unique_ptr<res_provider_base>(new Provider(args...));
		}

		void unregister_provider(std::string_view name) { providers_.erase(name.data()); }

		void load_from_file(const stdfs::path &path) {
			auto res = ::util::read_json_file(path);
			::util::json_assert_type(res, ::util::json_value_type::object);
			::util::json_assert_contains(res, "resources");
			::util::json_assert_type(res["resources"], ::util::json_value_type::array);
			for(const auto &item : res["resources"]) {
				::util::json_assert_type(item, ::util::json_value_type::object);
				::util::json_assert_contains(item, "provider");
				::util::json_assert_type(item["provider"], ::util::json_value_type::string);
				auto provider = item["provider"].get_ref<const std::string &>();
				if(!providers_.contains(provider))
					::util::fail_error("Unknown provider: ", item["provider"]);
				
				::util::json_assert_contains(item, "uuid");
				::util::json_assert_type(item["uuid"], ::util::json_value_type::string);
				auto uuid = uuids::uuid::from_string(item["uuid"].get_ref<const std::string &>());
				if(!uuid.has_value()) ::util::fail_error("Invalid UUID: ", res["shader"]);

				::util::json_assert_contains(item, "path");
				::util::json_assert_type(item["path"], ::util::json_value_type::string);
				auto path = item["path"].get_ref<const std::string &>();

				new_resource(uuid.value(), path, provider);

				if(item.contains("name")) {
					::util::json_assert_type(item["name"], ::util::json_value_type::string);
					auto name = item["name"].get_ref<const std::string &>();
					std::cerr << "  `- name: " << name << std::endl;
					names_.emplace(name, uuid.value());
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
			res_provider_base *provider; /* resource provider. */
			using set_cmp_ = decltype([](const res_id_type &a, const res_id_type &b) -> bool {
				return res_id_type::hash{}(a) < res_id_type::hash{}(b);
			});
			std::set<res_id_type, set_cmp_> deps; /* dependencies. */
			std::set<res_id_type, set_cmp_> rdeps; /* reverse dependencies. */

			std::byte *data; /* resource data as an opaque pointer. */
			bool loaded = false; /* true if resource has been loaded, false otherwise. */

			res_container(
				res_id_type id,
				const stdfs::path &path,
				res_provider_base *provider
			) : id(id), path(path), provider(provider),
			    data(nullptr), loaded(false) {}

			/* load the resource if it hasn't been loaded yet. will also load dependencies. */
			void *maybe_load(res_manager &m, const res_id_type &id) {
				if(!loaded) {
					data = new std::byte[provider->get_size()];
					for(const auto &dep : deps) {
						if(auto it = m.resources_.find(dep); it != m.resources_.end()) {
							it->second.maybe_load(m, it->first);
						}
					}
					provider->load(m, id, path, std::span<std::byte>(data, provider->get_size()));
					loaded = true;
				}
				return data;
			}

			/* unload the resource if it hasn't been unloaded yet and no reverse dependencies are loaded. */
			void maybe_unload(res_manager &m, const res_id_type &id) {
				if(!loaded) return;
				for(const auto &dep : rdeps) {
					if(auto it = m.resources_.find(dep); it != m.resources_.end()) {
						if(it->second.loaded) return; // do not unload, reverse dependency still loaded.
					}
				}
				provider->unload(m, id, std::span<std::byte>(data, provider->get_size()));
				delete data;
				loaded = false;
			}

			~res_container() {
				if(loaded) {
					::util::print_error("resource leak: ", id);
				}
			}
		};

		std::unordered_map<std::string, res_id_type> names_;
		std::unordered_map<std::string, std::unique_ptr<res_provider_base>> providers_;
		std::unordered_map<res_id_type, res_container, res_id_type::hash> resources_;
	};

	std::mt19937 res_manager::rand_engine_{};
}

template<typename T>
using res_ref = res::res_manager::ref<T>;

namespace gfx {
	class renderer;
	
	class shader {
		friend ::gfx::renderer;
		GLuint id;
	public:
		void unload(::res::res_manager &m, const ::res::res_id_type &rid) {
			glDeleteProgram(id);
		}

		void load_from_file(::res::res_manager &m, const ::res::res_id_type &rid, const stdfs::path &general_path) {
			stdfs::path vs_path = general_path / "vert.glsl";
			stdfs::path fs_path = general_path / "frag.glsl";
			std::cerr << "  .--------------\n";
			std::cerr << "-: loading shader\n";
			std::cerr << "  `--------------\n";
			std::cerr << "     path: " << general_path << "\n";
			std::cerr << "  vs path: " << vs_path << "\n";
			std::cerr << "  fs path: " << fs_path << "\n";
			std::cerr << std::endl;

			auto vs = glCreateShader(GL_VERTEX_SHADER);
			auto vs_content = ::util::read_file(vs_path);
			vs_content.push_back('\0');
			const char *vs_content_data = vs_content.data();
			glShaderSource(vs, 1, &vs_content_data, nullptr);
			glCompileShader(vs);

			GLint success = GL_FALSE;
			glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
			if(!success) {
				GLchar message[1024];
				glGetShaderInfoLog(vs, 1024, nullptr, message);
				::util::fail_error("failed to compile vertex shader:\n", message);
			}

			auto fs = glCreateShader(GL_FRAGMENT_SHADER);
			auto fs_content = ::util::read_file(fs_path);
			fs_content.push_back('\0');
			const char *fs_content_data = fs_content.data();
			glShaderSource(fs, 1, &fs_content_data, nullptr);
			glCompileShader(fs);

			glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
			if(success != GL_TRUE) {
				GLchar message[1024];
				glGetShaderInfoLog(fs, 1024, nullptr, message);
				::util::fail_error("failed to compile fragment shader:\n", message);
			}

			id = glCreateProgram();
			glAttachShader(id, fs);
			glAttachShader(id, vs);
			glLinkProgram(id);
			glGetProgramiv(id, GL_LINK_STATUS, &success);
			if(success != GL_TRUE) {
				GLsizei log_length = 0;
				GLchar message[1024];
				glGetProgramInfoLog(id, 1024, &log_length, message);
				::util::fail_error("failed to link shader program", message);
			}
		}

		void set_uniform(const char *name, glm::vec2 v) const {
			glProgramUniform2f(id, glGetUniformLocation(id, name),
				v.x, v.y);
		}

		void set_uniform(const char *name, glm::vec3 v) const {
			glProgramUniform3f(id, glGetUniformLocation(id, name),
				v.x, v.y, v.z);
		}

		void set_uniform(const char *name, glm::vec4 v) const {
			glProgramUniform4f(id, glGetUniformLocation(id, name),
				v.x, v.y, v.z, v.w);
		}

		void set_uniform(const char *name, const glm::mat4 &v) const {
			glProgramUniformMatrix4fv(id, glGetUniformLocation(id, name),
				1, GL_FALSE, glm::value_ptr(v));
		}
	};

	enum class mesh_mode {
		triangle_strip = GL_TRIANGLE_STRIP,
		triangle_fan = GL_TRIANGLE_FAN,
		triangles = GL_TRIANGLES,
	};

	class mesh {
		friend ::gfx::renderer;
		bool indexed;
		GLuint vao, vbo, ebo;
		GLsizei vertex_count, index_count;
		mesh_mode mode;
	public:
		struct vertex_type {
			glm::vec3 pos;
			glm::vec3 norm;
			glm::vec2 texcoord;

			bool operator==(const vertex_type &other) const {
				return pos == other.pos && norm == other.norm && texcoord == other.texcoord;
			}
		};

		using index_type = uint16_t;

		void unload(::res::res_manager &m, const ::res::res_id_type &id) {
			if(indexed) glDeleteBuffers(1, &ebo);
			glDeleteBuffers(1, &vbo);
			glDeleteVertexArrays(1, &vao);
		}

		void load_from_file(::res::res_manager &m, const ::res::res_id_type &id, const stdfs::path &path) {
			std::cerr << "  .------------\n";
			std::cerr << "-: loading mesh\n";
			std::cerr << "  `------------\n";
			std::cerr << "     path: " << path << "\n";
			if(path.extension() == ".gltf") {
				load_from_gltf(m, id, path.c_str());
			} else if(path.extension() == ".obj") {
				load_from_obj(m, id, path.c_str());
			} else {
				::util::fail_error("unkonwn file extension: ", path.extension());
			}
		}

		void load_from_gltf(::res::res_manager &m, const ::res::res_id_type &id, const char *path) {
			cgltf_options options {};
			cgltf_data *data = NULL;
			cgltf_result result = cgltf_parse_file(&options, path, &data);
			if(result != cgltf_result_success) {
				::util::fail_error("failed to load gltf mesh: ", ::util::cgltf_result_string(result));
			}
			// for(size_t i = 0; i < data->meshes_count; ++i) {
			// 	data->meshes[i].primitives[0].material;
			// }
			cgltf_free(data);
		}

		void load_from_obj(::res::res_manager &m, const ::res::res_id_type &id, const char *path) {
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;
			std::string warn, err;

			if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path)) {
				::util::fail_error(warn + err);
			}

			std::unordered_map<vertex_type, index_type, decltype([](const vertex_type &v) {
				return (
					(std::hash<glm::vec3>()(v.pos) ^
					(std::hash<glm::vec3>()(v.norm) << 1)) >> 1
				) ^ (std::hash<glm::vec2>()(v.texcoord) << 1);
			})> unique_vertices{};

			std::vector<vertex_type> vertices;
			std::vector<index_type> indices;

			for (const auto &shape : shapes) {
				for (const auto &index : shape.mesh.indices) {
					vertex_type vertex {};
					vertex.pos = {
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2]
					};

					vertex.norm = {
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2]
					};

					vertex.texcoord = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						attrib.texcoords[2 * index.texcoord_index + 1]
					};

					if(unique_vertices.count(vertex) == 0) {
						unique_vertices[vertex] = vertices.size();
						vertices.push_back(vertex);
					}

					indices.push_back(unique_vertices[vertex]);
				}
			}

			load_from_data(mesh_mode::triangles, vertices, indices);
		}

		void set_vertex_attributes() {			
			glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, pos));
			glVertexArrayAttribBinding(vao, 0, 0);
			glEnableVertexArrayAttrib(vao, 0);

			glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, norm));
			glVertexArrayAttribBinding(vao, 1, 0);
			glEnableVertexArrayAttrib(vao, 1);
			
			glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(vertex_type, texcoord));
			glVertexArrayAttribBinding(vao, 2, 0);
			glEnableVertexArrayAttrib(vao, 2);
		}

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices,
			const std::span<index_type> &indices
		) {
			std::cerr << " vertices: " << vertices.size() << "\n";
			std::cerr << "  indices: " << indices.size() << "\n";
			std::cerr << std::endl;
			indexed = true;
			this->mode = mode;
			vertex_count = vertices.size();
			index_count = indices.size();

			glCreateVertexArrays(1, &vao);
			glCreateBuffers(1, &vbo);
			glCreateBuffers(1, &ebo);
			glVertexArrayElementBuffer(vao, ebo);
			glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(vertex_type));
			glNamedBufferData(vbo, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
			glNamedBufferData(ebo, indices.size_bytes(), indices.data(), GL_STATIC_DRAW);
			set_vertex_attributes();
		}

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices
		) {
			std::cerr << " vertices: " << vertices.size() << "\n";
			std::cerr << "  indices: none" << "\n";
			std::cerr << std::endl;
			indexed = false;
			this->mode = mode;
			vertex_count = vertices.size();
			index_count = 0;
			glCreateVertexArrays(1, &vao);
			glCreateBuffers(1, &vbo);
			glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(vertex_type));
			glNamedBufferData(vbo, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
			set_vertex_attributes();
		}
	};

	class material {
		friend ::gfx::renderer;
		using value_type = std::variant<float, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;
		std::unordered_map<std::string, value_type> params;
		// note: sizeof(value_type) is large
		::res_ref<shader> shader;
	public:
		void unload(::res::res_manager &m, const ::res::res_id_type &id) {}
		void load_from_file(::res::res_manager &m, const ::res::res_id_type &id, const stdfs::path &path) {
			std::cerr << "  .----------------\n";
			std::cerr << "-: loading material\n";
			std::cerr << "  `----------------\n";
			std::cerr << "  path: " << path << "\n";
			std::cerr << std::endl;
			auto res = ::util::read_json_file(path);
			::util::json_assert_type(res, ::util::json_value_type::object);
			if(res.contains("shader")) {
				::util::json_assert_type(res["shader"], ::util::json_value_type::string);
				auto shader_name = res["shader"].get_ref<const std::string &>();
				shader.id = std::move(m.get_resource<::gfx::shader>(shader_name).id);
			} else if(res.contains("shader-uuid")) {
				::util::json_assert_type(res["shader-uuid"], ::util::json_value_type::string);
				auto shader_uuid = uuids::uuid::from_string(res["shader-uuid"].get_ref<const std::string &>());
				if(!shader_uuid.has_value()) {
					::util::fail_error("Invalid UUID: ", res["shader-uuid"]);
				}
				shader.id = std::move(shader_uuid).value();
			} else {
				::util::fail_error("no shader or shader-uuid fields.");
			}
			m.add_dependency(id, shader.id);
			if(res.contains("params")) {
				::util::json_assert_type(res["params"], ::util::json_value_type::object);
				for(auto &[key, value] : res["params"].items()) {
					::util::json_assert_type(value,
						::util::json_value_type::number,
						::util::json_value_type::array);
					if(value.is_number()) params[key] = value.get<float>();
					else {
						if(value.size() == 1) params[key] = value[0].get<float>();
						else if(value.size() == 2) {
							params[key] = glm::vec2(
								value[0].get<float>(),
								value[1].get<float>()
							);
						} else if(value.size() == 3) {
							params[key] = glm::vec3(
								value[0].get<float>(),
								value[1].get<float>(),
								value[2].get<float>()
							);
						} else if(value.size() == 4) {
							params[key] = glm::vec4(
								value[0].get<float>(),
								value[1].get<float>(),
								value[2].get<float>(),
								value[3].get<float>()
							);
						} else {
							::util::fail_error("Invalid vector with ", value.size(), " items.");
						}
					}
				}
			}
		}
	};

	class model {
		friend ::gfx::renderer;
		std::vector<std::pair<::res_ref<::gfx::mesh>, ::res_ref<::gfx::material>>> parts;
	public:
		void unload(::res::res_manager &m, const ::res::res_id_type &id) {}
		void load_from_file(::res::res_manager &m, const ::res::res_id_type &id, const stdfs::path &path) {
			std::cerr << " -- loading model -- @" << path << std::endl;
			auto res = ::util::read_json_file(path);
			::util::json_assert_type(res, ::util::json_value_type::object);
			::util::json_assert_contains(res, "parts");
			::util::json_assert_type(res["parts"], ::util::json_value_type::array);
			for(auto &part : res["parts"]) {
				::util::json_assert_type(part, ::util::json_value_type::object);
				::util::json_assert_contains(part, "mesh");
				::util::json_assert_contains(part, "material");
			}
		}
	};

	class backend_gl3w {
	public:
		static void init() {
			if(gl3wInit() < 0)
				::util::fail_error("Failed to initialize gl3w");
		}
	};

	class backend_glfw {
		static bool initialized_;
	public:
		static inline bool is_initialized() { return initialized_; }

		static void init() {
			glfwSetErrorCallback([](int error, const char *message) {
				::util::print_error("GLFW Error: ", error, ": ", message);
			});

			if(!glfwInit()) ::util::fail_error("Failed to initialize GLFW");
			else initialized_ = true;
		}

		static void poll_events() {
			glfwPollEvents();
		}

		static void deinit() {
			glfwTerminate();
			initialized_ = false;
		}

		static float get_time() { return glfwGetTime(); }
	};

	bool ::gfx::backend_glfw::initialized_ = false;

	class window {
		GLFWwindow *window;
		glm::vec2 delta_scroll = glm::vec2(0.0f, 0.0f);
		::util::event<void(glm::ivec2 new_size)> resize_event;
	public:
		void init(const char *title, glm::ivec2 size) {
			if(!::gfx::backend_glfw::is_initialized()) ::util::fail_error("GLFW not initlaized.");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
			window = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);
			if(window == nullptr) ::util::fail_error("Failed to create GLFW window");
			glfwSetWindowUserPointer(window, this);

			glfwSetScrollCallback(window, [](GLFWwindow *window, double x_offset, double y_offset) {
				auto *self = (decltype(this))glfwGetWindowUserPointer(window);
				self->delta_scroll = glm::vec2(x_offset, y_offset);
			});

			glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
				auto *self = (decltype(this))glfwGetWindowUserPointer(window);
				self->resize_event.dispatch(glm::ivec2(width, height));
			});
		}
		
		void bind() { glfwMakeContextCurrent(window); }
		bool is_open() { return !glfwWindowShouldClose(window); }
		void update() {
			delta_scroll = glm::vec2(0.0f, 0.0f);
			glfwSwapBuffers(window);
		}

		void close() { return glfwSetWindowShouldClose(window, 1); }

		void deinit() {
			if(!::gfx::backend_glfw::is_initialized()) ::util::fail_error("GLFW not initlaized.");
			glfwDestroyWindow(window);
		}

		glm::ivec2 size() const {
			glm::ivec2 s;
			glfwGetFramebufferSize(window, &s.x, &s.y);
			return s;
		}

		float aspect() const {
			glm::fvec2 s = size();
			return s.x / s.y;
		}

		bool get_key(int key) const {
			return glfwGetKey(window, key);
		}

		bool get_mouse_button(int button) const {
			return glfwGetMouseButton(window, button);
		}

		glm::vec2 get_mouse_position() const {
			glm::dvec2 pos;
			glfwGetCursorPos(window, &pos.x, &pos.y);
			return pos;
		}

		glm::vec2 get_scroll_delta() const {
			return delta_scroll;
		}

		auto get_resize_hook() -> ::util::event_hook<void(glm::ivec2)> {
			return ::util::event_hook<void(glm::ivec2)>(resize_event);
		}
	};

	class renderer {
		GLuint bound_vao = 0, bound_shader = 0;
		bool depth_test = false;

		::res::res_manager &resman;

		void bind_vao(GLuint vao) {
			if(bound_vao != vao)
				glBindVertexArray(bound_vao = vao);
		}

		void bind_shader(GLuint shader) {
			if(bound_shader != shader)
				glUseProgram(shader);
		}

		void set_depth_test(bool enabled) {
			if(depth_test && enabled) return;
			if(!depth_test && !enabled) return;
			if(enabled) glEnable(GL_DEPTH_TEST);
			else glDisable(GL_DEPTH_TEST);
			depth_test = enabled;
		}

	public:
		renderer(::res::res_manager &m) : resman(m) {}

		void init() {
			depth_test = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
		}

		void pre_render() {
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			set_depth_test(true);
		}

		void viewport(glm::vec2 vp) {
			glViewport(0.0f, 0.0f, vp.x, vp.y);
		}

		void post_render() {

		}

		void render(const ::gfx::mesh &mesh) {
			bind_vao(mesh.vao);
			if(mesh.indexed) {
				glDrawElements((GLenum)mesh.mode, mesh.index_count, GL_UNSIGNED_SHORT, nullptr);
			} else {
				glDrawArrays((GLenum)mesh.mode, 0, mesh.vertex_count);
			}
		}

		void render(const ::gfx::model &model) {
			for(const auto &[mesh, material] : model.parts) {
				bind_shader(material.get_from(resman).shader.get_from(resman).id);
				render(mesh.get_from(resman));
			}
		}

		void use(const ::gfx::shader &shader) {
			bind_shader(shader.id);
		}
	};
}

struct spherical_camera {
	glm::vec3 pos;
	glm::vec2 rot;
	float range;
	float aspect;
	float fov;
	float z_near, z_far;

	glm::mat4 matrix() const {
		auto proj = glm::perspective(fov, aspect, z_near, z_far);
		auto view = glm::lookAt(
			pos + glm::vec3(
				range * glm::sin(rot.y) * glm::cos(rot.x),
				range * glm::cos(rot.y),
				range * glm::sin(rot.y) * glm::sin(rot.x)
			),
			pos,
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		return proj * view;
	}
};

struct transform {
	glm::vec3 pos;
	glm::quat rot;
	glm::vec3 scl;

	glm::mat4 matrix() const {
		auto m = glm::mat4(1.0f);
		m = glm::translate(m, pos);
		m = glm::scale(m, scl);
		m = glm::mat4_cast(rot) * m;
		return m;
	}
};

int main(int argc, char *argv[]) {
	gfx::backend_glfw::init();
	gfx::window window;

	window.init("Hello!", { 640, 480 });
	window.bind();

	gfx::backend_gl3w::init();

	res::res_manager resman;
	resman.register_provider<gfx::shader>("shader");
	resman.register_provider<gfx::material>("material");
	resman.register_provider<gfx::mesh>("mesh");
	resman.register_provider<gfx::model>("model");
	resman.load_from_file("data/resman.json");
	auto default_shader = resman.get_resource<gfx::shader>("shader.default");
	auto default_material = resman.get_resource<gfx::material>("material.default");
	default_shader.preload_from(resman);
	default_material.preload_from(resman);

	std::vector<std::string> mesh_names = { "mesh.cube", "mesh.house" };
	int current_mesh_index = 0;

	gfx::renderer rend{resman};
	rend.init();
	rend.use(default_shader.get_from(resman));
	
	transform trans /* :o */ = {
		.pos = glm::vec3(0.0f, 0.0f, 0.0f),
		.rot = glm::identity<glm::quat>(),
		.scl = glm::vec3(1.0f, 1.0f, 1.0f)
	};

	spherical_camera cam = {
		.pos = trans.pos,
		.rot = { 0.0f, glm::pi<float>() / 2.0f },
		.range = 5.0f,
		.aspect = window.aspect(),
		.fov = 90.0f,
		.z_near = 0.01f,
		.z_far = 100.0f
	};

	window.get_resize_hook().add([&](glm::ivec2 size) {
		cam.aspect = window.aspect();
	});

	struct { glm::vec3 dir; glm::vec4 col; } sun = {
		.dir = glm::normalize(glm::vec3(1.0f, 2.0f, -0.6f)),
		.col = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
	};

	default_shader.context_from(resman, [&](const gfx::shader &shader) {
		shader.set_uniform("uSunDir", sun.dir);
		shader.set_uniform("uSunCol", sun.col);
	});

	float last_time = gfx::backend_glfw::get_time();
	glm::vec2 last_mouse_pos = window.get_mouse_position();

	bool right_left_key_was_down = false;

	while(window.is_open()) {
		gfx::backend_glfw::poll_events();
		float current_time = gfx::backend_glfw::get_time();
		float delta_time = current_time - last_time;

		glm::vec2 current_mouse_pos = window.get_mouse_position();
		glm::vec2 delta_mouse_pos = current_mouse_pos - last_mouse_pos;
		if(window.get_mouse_button(0)) {
			glm::vec2 sens = { 0.003f, -0.003f };
			cam.rot.x += delta_mouse_pos.x * sens.x;
			cam.rot.y += delta_mouse_pos.y * sens.y;
			cam.rot.y = glm::clamp(cam.rot.y, glm::epsilon<float>(), +glm::pi<float>());
		}

		float delta_scroll = window.get_scroll_delta().y;
		cam.range -= delta_scroll * 20.0f * delta_time;
		cam.range = glm::clamp(cam.range, glm::epsilon<float>(), 100.0f);

		if(window.get_key(256 /* escape */)) window.close();

		if(window.get_key(262 /* right */)) {
			if(!right_left_key_was_down)
				current_mesh_index = (current_mesh_index + 1) % mesh_names.size();
			right_left_key_was_down = true;
		} else if(window.get_key(263 /* left */)) {
			if(!right_left_key_was_down)
				current_mesh_index = (current_mesh_index - 1) % mesh_names.size();
			right_left_key_was_down = true;
		} else {
			right_left_key_was_down = false;
		}

		default_shader.context_from(resman, [&](const gfx::shader &shader) {
			shader.set_uniform("uTransform", cam.matrix() * trans.matrix());
		});

		auto current_mesh = resman.get_resource<gfx::mesh>(mesh_names[current_mesh_index]);

		rend.pre_render();
		rend.viewport(window.size());
		rend.render(current_mesh.get_from(resman));
		rend.post_render();

		window.update();
		last_time = current_time;
		last_mouse_pos = current_mouse_pos;
	}

	resman.delete_all();
	window.deinit();
	
	gfx::backend_glfw::deinit();

	return 0;
}
