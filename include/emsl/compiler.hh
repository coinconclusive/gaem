#ifndef EMSL_COMPILER_HH_
#define EMSL_COMPILER_HH_
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace emsl {
	namespace comp {
		enum class value_type_kind {
			scalar,
			vector,
			matrix,
			structure,
			sampler,
		};

		struct value_type {
			value_type_kind kind;
		};

		template<value_type_kind Kind_>
		struct value_type_ {
			static constexpr value_type_kind Kind = Kind_;
		};

		struct scalar_value_type : value_type_<value_type_kind::scalar> {
			uint_least8_t size;
		};

		struct vector_value_type : value_type_<value_type_kind::vector> {
			uint_least8_t size;
		};

		struct matrix_value_type : value_type_<value_type_kind::matrix> {
			uint_least8_t rows, cols;
		};

		struct structure_value_type : value_type_<value_type_kind::structure> {
			std::unordered_map<std::string, std::unique_ptr<value_type>> fields;
		};

		struct sampler_value_type : value_type_<value_type_kind::sampler> {
			uint_least8_t dims;
		};

		struct input {
			std::string name;
			value_type type;
		};

		struct program {

		};
	}

	class compiler {

	};
}

#endif
