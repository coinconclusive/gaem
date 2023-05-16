#ifndef GAEM_GFX_SHADER_COMPILER_HH_
#define GAEM_GFX_SHADER_COMPILER_HH_
#include <gaem/common.hh>
#include <gaem/gfx.hh>
#include <gaem/gfx/shader.hh>

namespace gaem::gfx::emsl {
	enum class shader_module_type {
		entry_vertex, entry_fragment,
		partial_vertex, partial_fragment
	};

	enum class shader_type { vertex, fragment };

	enum class shader_module_value_type {
		float1, float2, float3, float4, float4x4, sampler2D, void1, int1
	};

	struct shader_module_variable {
		shader_module_value_type type;
		std::string name;
	};

	enum class shader_module_parameter_type {
		input, output, input_output
	};

	struct shader_module_parameter : shader_module_variable {
		shader_module_parameter_type param_type;
	};

	enum class shader_module_field_type {
		input, output, uniform
	};

	struct shader_module_field : shader_module_variable {
		std::variant<std::monostate, std::string, int> tag;
		shader_module_field_type field_type;
	};

	struct shader_module_source_code;

	struct shader_module_function {
		shader_module_value_type return_type;
		std::vector<shader_module_parameter> params;
		std::string name;
		shader_module_source_code *code;
		~shader_module_function();
	};

	class shader_module {
	public:
		shader_module_type type;
		std::vector<shader_module_function> functions;
		std::vector<shader_module_field> fields;
	};

	/** compile shader modules */
	shader_module compile_module(const stdfs::path &path);

	/** link modules (partials, program entry points, shared variables, etc.)
	  * and generate source code for a shader type from the linked modules */
	std::string link_modules(shader_type type, std::span<shader_module> modules);
}

#endif // GAEM_GFX_SHADER_COMPILER_HH_
