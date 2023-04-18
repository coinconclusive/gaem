#ifndef GAEM_UTIL_FS_HH_
#define GAEM_UTIL_FS_HH_
#include <gaem/common.hh>
#include <vector>

namespace gaem::util::fs {
	auto read_file(const stdfs::path &path) -> std::vector<char>;
}

#endif
