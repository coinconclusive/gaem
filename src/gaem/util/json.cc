#include <gaem/util/json.hh>

namespace gaem::util::json {
	nmann::json read_file(const stdfs::path &path) {
		std::ifstream inp(path);
		inp.exceptions(std::ios_base::badbit);
		return nmann::json::parse(inp);
	}

	value_kind nmann_type_to_value_kind(nmann::json::value_t v) {
		switch(v) {
		case nmann::detail::value_t::null: return value_kind::null;
		case nmann::detail::value_t::object: return value_kind::object;
		case nmann::detail::value_t::array: return value_kind::array;
		case nmann::detail::value_t::string: return value_kind::string;
		case nmann::detail::value_t::boolean:  return value_kind::boolean;
		case nmann::detail::value_t::number_integer:
		case nmann::detail::value_t::number_unsigned:
		case nmann::detail::value_t::number_float:
		case nmann::detail::value_t::binary:
		case nmann::detail::value_t::discarded:
			return value_kind::number;
		}
	}

	const char *type_name(value_kind type) {
		switch(type) {
		case value_kind::null: return "null";
		case value_kind::object: return "object";
		case value_kind::array: return "array";
		case value_kind::string: return "string";
		case value_kind::number: return "number";
		case value_kind::boolean: return "boolean";
		}
	}
}
