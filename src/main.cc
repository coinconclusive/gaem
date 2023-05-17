#include <gaem/common.hh>
#include <gaem/resource.hh>
#include <gaem/util/log.hh>
#include <gaem/util/json.hh>
#include <gaem/gfx/material.hh>
#include <gaem/gfx/texture.hh>
#include <gaem/gfx/mesh.hh>
#include <gaem/gfx/model.hh>
#include <gaem/gfx/shader.hh>
#include <gaem/gfx/renderer.hh>
#include <gaem/gfx/window.hh>
#include <gaem/gfx/shader/compiler.hh>
#include <iostream>

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
	using namespace gaem;
	clog.set_spread_out(0);
	// std::atexit([](){ clog.flush(); });

	gfx::emsl::shader_module mods[] = {
		gfx::emsl::compile_module("data/materials/glsl/frag_entry.emsl"),
		gfx::emsl::compile_module("data/materials/glsl/texture.part.emsl"),
		gfx::emsl::compile_module("data/materials/glsl/lit.part.emsl"),
	};
	gfx::emsl::link_modules(std::cerr, gfx::emsl::shader_type::fragment, { mods });

	return 0;

	// gfx::backend_glfw::init();
	// gfx::window window;

	// window.init("Hello!", { 640, 480 });
	// window.bind();

	// gfx::backend_gl3w::init();

	// res::res_manager resman;
	// resman.register_provider<gfx::shader>("shader");
	// resman.register_provider<gfx::material>("material");
	// resman.register_provider<gfx::mesh>("mesh");
	// resman.register_provider<gfx::model>("model");
	// resman.register_provider<gfx::texture>("texture");
	// resman.load_from_file("data/resman.json");
	// auto default_shader = resman.get_resource<gfx::shader>("shader.default");
	// auto default_material = resman.get_resource<gfx::material>("material.default");
	// default_shader.preload_from(resman);
	// default_material.preload_from(resman);

	// std::vector<std::string> mesh_names = { "mesh.cube", "mesh.house" };
	// int current_mesh_index = 0;

	// gfx::renderer rend{resman};
	// rend.init();
	
	// transform trans /* :o */ = {
	// 	.pos = glm::vec3(0.0f, 0.0f, 0.0f),
	// 	.rot = glm::identity<glm::quat>(),
	// 	.scl = glm::vec3(1.0f, 1.0f, 1.0f)
	// };

	// spherical_camera cam = {
	// 	.pos = trans.pos,
	// 	.rot = { 0.0f, glm::pi<float>() / 2.0f },
	// 	.range = 5.0f,
	// 	.aspect = window.aspect(),
	// 	.fov = 90.0f,
	// 	.z_near = 0.01f,
	// 	.z_far = 100.0f
	// };

	// window.get_resize_hook().add([&](glm::ivec2 size) {
	// 	cam.aspect = window.aspect();
	// });

	// float last_time = gfx::backend_glfw::get_time();
	// glm::vec2 last_mouse_pos = window.get_mouse_position();

	// bool right_left_key_was_down = false;

	// while(window.is_open()) {
	// 	gfx::backend_glfw::poll_events();
	// 	float current_time = gfx::backend_glfw::get_time();
	// 	float delta_time = current_time - last_time;

	// 	glm::vec2 current_mouse_pos = window.get_mouse_position();
	// 	glm::vec2 delta_mouse_pos = current_mouse_pos - last_mouse_pos;
	// 	if(window.get_mouse_button(0)) {
	// 		glm::vec2 sens = { 0.003f, -0.003f };
	// 		cam.rot.x += delta_mouse_pos.x * sens.x;
	// 		cam.rot.y += delta_mouse_pos.y * sens.y;
	// 		cam.rot.y = glm::clamp(cam.rot.y, glm::epsilon<float>(), +glm::pi<float>());
	// 	}

	// 	float delta_scroll = window.get_scroll_delta().y;
	// 	cam.range -= delta_scroll * 20.0f * delta_time;
	// 	cam.range = glm::clamp(cam.range, glm::epsilon<float>(), 100.0f);

	// 	if(window.get_key(256 /* escape */)) window.close();

	// 	if(window.get_key(262 /* right */)) {
	// 		if(!right_left_key_was_down)
	// 			current_mesh_index = (current_mesh_index + 1) % mesh_names.size();
	// 		right_left_key_was_down = true;
	// 	} else if(window.get_key(263 /* left */)) {
	// 		if(!right_left_key_was_down)
	// 			current_mesh_index = (current_mesh_index - 1) % mesh_names.size();
	// 		right_left_key_was_down = true;
	// 	} else {
	// 		right_left_key_was_down = false;
	// 	}

	// 	default_material.get_from(resman).set("uTransform", cam.matrix() * trans.matrix());

	// 	auto current_mesh = resman.get_resource<gfx::mesh>(mesh_names[current_mesh_index]);

	// 	rend.pre_render();
	// 	rend.viewport(window.size());
	// 	rend.bind_material(default_material.get_from(resman));
	// 	rend.render(current_mesh.get_from(resman));
	// 	rend.post_render();

	// 	window.update();
	// 	last_time = current_time;
	// 	last_mouse_pos = current_mouse_pos;
	// }

	// resman.delete_all();
	// window.deinit();
	
	// gfx::backend_glfw::deinit();
	return 0;
}
