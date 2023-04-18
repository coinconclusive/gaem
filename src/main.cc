#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <tiny_obj_loader.h>
#include <cgltf.h>
#include <stb_image.h>
#include <uuid.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

#include <gaem/common.hh>
#include <gaem/resource.hh>
#include <gaem/util/log.hh>
#include <gaem/util/json.hh>

namespace stdfs = std::filesystem;

namespace util {
}

namespace gfx {
	class renderer;
	
	class shader {
		friend gfx::renderer;
		GLuint id_;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id) {
			glDeleteProgram(id_);
		}

		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &general_path) {
			stdfs::path vs_path = general_path / "vert.glsl";
			stdfs::path fs_path = general_path / "frag.glsl";
			clog.println("path: {}", general_path);
			clog.println("vs path: {}", vs_path);
			clog.println("fs path: {}", fs_path);

			auto vs = glCreateShader(GL_VERTEX_SHADER);
			auto vs_content = util::read_file(vs_path);
			vs_content.push_back('\0');
			const char *vs_content_data = vs_content.data();
			glShaderSource(vs, 1, &vs_content_data, nullptr);
			glCompileShader(vs);

			GLint success = GL_FALSE;
			glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
			if(!success) {
				GLchar message[1024];
				glGetShaderInfoLog(vs, 1024, nullptr, message);
				util::fail_error("Failed to compile vertex shader:\n{}", message);
			}

			auto fs = glCreateShader(GL_FRAGMENT_SHADER);
			auto fs_content = util::read_file(fs_path);
			fs_content.push_back('\0');
			const char *fs_content_data = fs_content.data();
			glShaderSource(fs, 1, &fs_content_data, nullptr);
			glCompileShader(fs);

			glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
			if(!success) {
				GLchar message[1024];
				glGetShaderInfoLog(fs, 1024, nullptr, message);
				util::fail_error("Failed to compile fragment shader:\n{}", message);
			}

			id_ = glCreateProgram();
			glAttachShader(id_, fs);
			glAttachShader(id_, vs);
			glLinkProgram(id_);
			glGetProgramiv(id_, GL_LINK_STATUS, &success);
			if(success != GL_TRUE) {
				GLsizei log_length = 0;
				GLchar message[1024];
				glGetProgramInfoLog(id_, 1024, &log_length, message);
				util::fail_error("Failed to link shader program:\n{}", message);
			}
		}

		void set_uniform(const char *name, int v) const {
			glProgramUniform1i(id_, glGetUniformLocation(id_, name), v);
		}

		void set_uniform(const char *name, float v) const {
			glProgramUniform1f(id_, glGetUniformLocation(id_, name), v);
		}

		void set_uniform(const char *name, glm::vec2 v) const {
			glProgramUniform2f(id_, glGetUniformLocation(id_, name),
				v.x, v.y);
		}

		void set_uniform(const char *name, glm::vec3 v) const {
			glProgramUniform3f(id_, glGetUniformLocation(id_, name),
				v.x, v.y, v.z);
		}

		void set_uniform(const char *name, glm::vec4 v) const {
			glProgramUniform4f(id_, glGetUniformLocation(id_, name),
				v.x, v.y, v.z, v.w);
		}

		void set_uniform(const char *name, const glm::mat4 &v) const {
			glProgramUniformMatrix4fv(id_, glGetUniformLocation(id_, name),
				1, GL_FALSE, glm::value_ptr(v));
		}
	};

	class texture {
		friend gfx::renderer;
		GLuint id_;
		glm::ivec2 size_;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id) {
			glDeleteTextures(1, &id_);
		}

		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
			clog.println("path: {}", path);
			
			int channels;
			uint8_t *data = stbi_load(path.c_str(), &size_.x, &size_.y, &channels, 4);
			glCreateTextures(GL_TEXTURE_2D, 1, &id_);
			glTextureParameteri(id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTextureParameteri(id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			glTextureStorage2D(id_, 1, GL_RGBA8, size_.x, size_.y);
			glTextureSubImage2D(id_, 0, 0, 0, size_.x, size_.y, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}

		glm::ivec2 get_size() const { return size_; }
	};

	enum class mesh_mode {
		triangle_strip = GL_TRIANGLE_STRIP,
		triangle_fan = GL_TRIANGLE_FAN,
		triangles = GL_TRIANGLES,
	};

	class mesh {
		friend gfx::renderer;
		bool indexed_;
		GLuint vao_, vbo_, ebo_;
		GLsizei vertex_count_, index_count_;
		mesh_mode mode_;
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

		void unload(res::res_manager &m, const res::res_id_type &id) {
			if(indexed_) glDeleteBuffers(1, &ebo_);
			glDeleteBuffers(1, &vbo_);
			glDeleteVertexArrays(1, &vao_);
		}

		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
			clog.println("path: {}", path);
			if(path.extension() == ".gltf") {
				load_from_gltf(m, id, path.c_str());
			} else if(path.extension() == ".obj") {
				load_from_obj(m, id, path.c_str());
			} else {
				util::fail_error("Unknown file extension: {}", path.extension());
			}
		}

		void load_from_gltf(res::res_manager &m, const res::res_id_type &id, const char *path) {
			cgltf_options options {};
			cgltf_data *data = NULL;
			cgltf_result result = cgltf_parse_file(&options, path, &data);
			if(result != cgltf_result_success) {
				util::fail_error("Failed to load gltf mesh: {}", util::cgltf_result_string(result));
			}
			// for(size_t i = 0; i < data->meshes_count; ++i) {
			// 	data->meshes[i].primitives[0].material;
			// }
			cgltf_free(data);
		}

		void load_from_obj(res::res_manager &m, const res::res_id_type &id, const char *path) {
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;
			std::string warn, err;

			if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path)) {
				util::fail_error("Failed to load .obj file:\n{}", warn + err);
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
			glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, pos));
			glVertexArrayAttribBinding(vao_, 0, 0);
			glEnableVertexArrayAttrib(vao_, 0);

			glVertexArrayAttribFormat(vao_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(vertex_type, norm));
			glVertexArrayAttribBinding(vao_, 1, 0);
			glEnableVertexArrayAttrib(vao_, 1);
			
			glVertexArrayAttribFormat(vao_, 2, 2, GL_FLOAT, GL_FALSE, offsetof(vertex_type, texcoord));
			glVertexArrayAttribBinding(vao_, 2, 0);
			glEnableVertexArrayAttrib(vao_, 2);
		}

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices,
			const std::span<index_type> &indices
		) {
			clog.println("vertices: {}", vertices.size());
			clog.println("indices: {}", indices.size());
			indexed_ = true;
			this->mode_ = mode;
			vertex_count_ = vertices.size();
			index_count_ = indices.size();

			glCreateVertexArrays(1, &vao_);
			glCreateBuffers(1, &vbo_);
			glCreateBuffers(1, &ebo_);
			glVertexArrayElementBuffer(vao_, ebo_);
			glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(vertex_type));
			glNamedBufferData(vbo_, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
			glNamedBufferData(ebo_, indices.size_bytes(), indices.data(), GL_STATIC_DRAW);
			set_vertex_attributes();
		}

		void load_from_data(
			mesh_mode mode,
			const std::span<vertex_type> &vertices
		) {
			clog.println("vertices: {}", vertices.size());
			clog.println("indices: none");
			indexed_ = false;
			this->mode_ = mode;
			vertex_count_ = vertices.size();
			index_count_ = 0;
			glCreateVertexArrays(1, &vao_);
			glCreateBuffers(1, &vbo_);
			glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(vertex_type));
			glNamedBufferData(vbo_, vertices.size_bytes(), vertices.data(), GL_STATIC_DRAW);
			set_vertex_attributes();
		}
	};

	class material {
		friend gfx::renderer;

		struct texture_binding {
			using unit_type = GLuint;
			res_ref<texture> texture;
			unit_type unit;
		};

		using value_type = std::variant<int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

		// note: sizeof(param_type) is large-ish.
		struct param_type {
			value_type value;
			bool dirty = true;

			template<typename T>
			void set(const T &t) {
				value = t;
				dirty = true;
			}
		};

		std::unordered_map<std::string, param_type> params_;
		std::vector<texture_binding> bindings_;
		res_ref<shader> shader_;

		/* remove duplicate textures, keeping the ones closer to the end. */
		void remove_duplicate_textures_(res::res_manager &m, const res::res_id_type &id) {
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
	public:
		void set(strv name, float v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec2 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec3 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::vec4 &v) { params_[name.data()].set(v); }
		void set(strv name, const glm::mat4 &v) { params_[name.data()].set(v); }

		void unload(res::res_manager &m, const res::res_id_type &id) {}
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
			clog.println("path: {}", path);
			auto res = util::json::read_file(path);
			util::json::assert_type(res, util::json::value_kind::object);
			if(res.contains("inherit")) {
				util::json::assert_type(res["inherit"],
					util::json::value_kind::array,
					util::json::value_kind::object);

				const auto &load_base_material = [&](const nmann::json &j) {
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
	};

	class model {
		friend gfx::renderer;
		std::vector<std::pair<res_ref<gfx::mesh>, res_ref<gfx::material>>> parts;
	public:
		void unload(res::res_manager &m, const res::res_id_type &id) {}
		void load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
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
	};

	class backend_gl3w {
	public:
		static void init() {
			if(gl3wInit() < 0)
				util::fail_error("Failed to initialize gl3w.");
		}
	};

	class backend_glfw {
		static bool initialized_;
	public:
		static inline bool is_initialized() { return initialized_; }

		static void init() {
			glfwSetErrorCallback([](int error, const char *message) {
				util::print_error("GLFW Error [{}] {}", error, message);
			});

			if(!glfwInit()) util::fail_error("Failed to initialize GLFW.");
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

	bool backend_glfw::initialized_ = false;

	class window {
		GLFWwindow *window;
		glm::vec2 delta_scroll = glm::vec2(0.0f, 0.0f);
		util::event<void(glm::ivec2 new_size)> resize_event;
	public:
		void init(const char *title, glm::ivec2 size) {
			if(!gfx::backend_glfw::is_initialized()) util::fail_error("GLFW not initlaized.");
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
			window = glfwCreateWindow(size.x, size.y, title, nullptr, nullptr);
			if(window == nullptr) util::fail_error("Failed to create GLFW window.");
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
			if(!gfx::backend_glfw::is_initialized()) util::fail_error("GLFW not initlaized.");
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

		auto get_resize_hook() -> util::event_hook<void(glm::ivec2)> {
			return util::event_hook<void(glm::ivec2)>(resize_event);
		}
	};

	class renderer {
		GLuint bound_vao_ = 0, bound_program_ = 0;
		std::set<GLuint> bound_textures_;
		bool depth_test_ = false;

		res::res_manager &resman_;

		void bind_vao_(GLuint vao) {
			if(bound_vao_ != vao)
				glBindVertexArray(bound_vao_ = vao);
		}

		void bind_program_(GLuint program) {
			if(bound_program_ != program)
				glUseProgram(bound_program_ = program);
		}

		void bind_texture_(GLuint unit, GLuint texture) {
			if(texture == 0) {
				bound_textures_.erase(texture);
			} else if(!bound_textures_.contains(texture)) {
				bound_textures_.insert(texture);
				glBindTextureUnit(unit, texture);
				
			}
		}

		void set_depth_test(bool enabled) {
			if(depth_test_ && enabled) return;
			if(!depth_test_ && !enabled) return;
			if(enabled) glEnable(GL_DEPTH_TEST);
			else glDisable(GL_DEPTH_TEST);
			depth_test_ = enabled;
		}

	public:
		renderer(res::res_manager &m) : resman_(m) {}

		void init() {
			depth_test_ = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
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

		void render(const gfx::mesh &mesh) {
			bind_vao_(mesh.vao_);
			if(mesh.indexed_) {
				glDrawElements((GLenum)mesh.mode_, mesh.index_count_, GL_UNSIGNED_SHORT, nullptr);
			} else {
				glDrawArrays((GLenum)mesh.mode_, 0, mesh.vertex_count_);
			}
		}

		void render(const gfx::model &model) {
			for(const auto &[mesh, material] : model.parts) {
				bind_program_(material.get_from(resman_).shader_.get_from(resman_).id_);
				render(mesh.get_from(resman_));
			}
		}

		void bind_material(gfx::material &material) {
			auto &shader = material.shader_.get_from(resman_);
			bind_shader(shader);
			for(auto &[name, param] : material.params_) {
				if(!param.dirty) continue;
				if(std::holds_alternative<int>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<int>(param.value));
				} else if(std::holds_alternative<float>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<float>(param.value));
				} else if(std::holds_alternative<glm::vec2>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<glm::vec2>(param.value));
				} else if(std::holds_alternative<glm::vec3>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<glm::vec3>(param.value));
				} else if(std::holds_alternative<glm::vec4>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<glm::vec4>(param.value));
				} else if(std::holds_alternative<glm::mat4>(param.value)) {
					shader.set_uniform(name.c_str(), std::get<glm::mat4>(param.value));
				} else assert(false && "bad material param value variant type");
				param.dirty = false;
			}
			for(const auto &texture : material.bindings_) {
				bind_texture(texture.unit, texture.texture.get_from(resman_));
			}
		}

		void bind_shader(const gfx::shader &shader) {
			bind_program_(shader.id_);
		}

		void bind_texture(int unit, const gfx::texture &texture) {
			bind_texture_(unit, texture.id_);
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
	clog.set_spread_out(0);
	std::atexit([](){ clog.flush(); });

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
	resman.register_provider<gfx::texture>("texture");
	resman.load_from_file("data/resman.json");
	auto default_shader = resman.get_resource<gfx::shader>("shader.default");
	auto default_material = resman.get_resource<gfx::material>("material.default");
	default_shader.preload_from(resman);
	default_material.preload_from(resman);

	std::vector<std::string> mesh_names = { "mesh.cube", "mesh.house" };
	int current_mesh_index = 0;

	gfx::renderer rend{resman};
	rend.init();
	
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

		default_material.get_from(resman).set("uTransform", cam.matrix() * trans.matrix());

		auto current_mesh = resman.get_resource<gfx::mesh>(mesh_names[current_mesh_index]);

		rend.pre_render();
		rend.viewport(window.size());
		rend.bind_material(default_material.get_from(resman));
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
