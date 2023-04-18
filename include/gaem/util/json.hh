#ifndef GAEM_UTIL_JSON_HH_
#define GAEM_UTIL_JSON_HH_
#include <gaem/common.hh>
#include <nlohmann_json.hpp>
#include <gaem/util/log.hh>
#include <fstream>

namespace gaem::util::json {
	namespace nmann = ::nlohmann;

	nmann::json read_file(const stdfs::path &path);

	enum class value_kind { null, object, array, string, number, boolean };

	value_kind nmann_type_to_value_kind(nmann::json::value_t v);
	const char *type_name(value_kind type);

	template<std::convertible_to<value_kind> ...Ts>
	bool check_type(const nmann::json &j, value_kind first, Ts &&...rest) {
		auto jtype = nmann_type_to_value_kind(j.type());
		std::array<value_kind, 1 + sizeof...(Ts)> types { first, rest... };
		for(auto t : types) {
			if(jtype == t) return true;
		}
		util::print_error(
			"Expected one of {}, but got {} instead.",
			std::make_tuple(type_name(first), type_name(rest)...),
			type_name(jtype)
		);
		return false;
	}

	template<std::convertible_to<value_kind> ...Ts>
	void assert_type(const nmann::json &j, value_kind first, Ts &&...rest) {
		if(!check_type(j, first, std::forward<Ts>(rest)...)) util::fail();
	}

	[[maybe_unused]]
	static inline void assert_contains(const nmann::json &j, const nmann::json::object_t::key_type &key) {
		if(!j.contains(key)) util::fail_error("No required key: '{}'.", key);
	}

	[[maybe_unused]]
	static inline void assert_contains(const nmann::json &j, size_t index) {
		if(index >= j.size()) util::fail_error("Array too small: {} >= {}.", index, j.size());
	}
}


#endif
