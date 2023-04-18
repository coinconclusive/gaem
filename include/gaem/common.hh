#ifndef GAEM_COMMON_HH_
#define GAEM_COMMON_HH_
#include <filesystem>
#include <span>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace gaem {
	namespace stdfs {
		using namespace ::std::filesystem;
	}

	using namespace ::std::literals;

	/* null-terminated string view. (alias) */
	using strv = std::string_view;
}

#endif
