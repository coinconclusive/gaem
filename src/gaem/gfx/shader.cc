#include <gaem/gfx/shader.hh>
#include <gaem/util/log.hh>
#include <gaem/util/fs.hh>
#include <gaem/gfx/opengl.hh>

namespace gaem::gfx {
	void shader::load_from_file(
		res::res_manager &m,
		const res::res_id_type &id,
		const stdfs::path &general_path
	) {
		stdfs::path vs_path = general_path / "vert.glsl";
		stdfs::path fs_path = general_path / "frag.glsl";
		clog.println("path: {}", general_path);
		clog.println("vs path: {}", vs_path);
		clog.println("fs path: {}", fs_path);

		auto vs = glCreateShader(GL_VERTEX_SHADER);
		auto vs_content = util::fs::read_file(vs_path);
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
		auto fs_content = util::fs::read_file(fs_path);
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

	void shader::unload(res::res_manager &m, const res::res_id_type &id) {
		glDeleteProgram(id_);
	}

	void shader::set_uniform(const char *name, int v) const {
		glProgramUniform1i(id_, glGetUniformLocation(id_, name), v);
	}

	void shader::set_uniform(const char *name, float v) const {
		glProgramUniform1f(id_, glGetUniformLocation(id_, name), v);
	}

	void shader::set_uniform(const char *name, glm::vec2 v) const {
		glProgramUniform2f(id_, glGetUniformLocation(id_, name),
			v.x, v.y);
	}

	void shader::set_uniform(const char *name, glm::vec3 v) const {
		glProgramUniform3f(id_, glGetUniformLocation(id_, name),
			v.x, v.y, v.z);
	}

	void shader::set_uniform(const char *name, glm::vec4 v) const {
		glProgramUniform4f(id_, glGetUniformLocation(id_, name),
			v.x, v.y, v.z, v.w);
	}

	void shader::set_uniform(const char *name, const glm::mat4 &v) const {
		glProgramUniformMatrix4fv(id_, glGetUniformLocation(id_, name),
			1, GL_FALSE, glm::value_ptr(v));
	}
}
