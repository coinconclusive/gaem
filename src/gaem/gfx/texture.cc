#include <gaem/gfx/texture.hh>
#include <gaem/gfx/opengl.hh>
#include <stb_image.h>

namespace gaem::gfx {
	void texture::unload(res::res_manager &m, const res::res_id_type &id) {
		glDeleteTextures(1, &id_);
	}

	void texture::load_from_file(res::res_manager &m, const res::res_id_type &id, const stdfs::path &path) {
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
}
