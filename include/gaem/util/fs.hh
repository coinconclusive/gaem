#ifndef GAEM_UTIL_FS_HH_
#define GAEM_UTIL_FS_HH_
#include <gaem/common.hh>
#include <vector>
#include <fstream>

namespace gaem::util::fs {
	auto read_file(const stdfs::path &path) -> std::vector<char> {
		std::vector<char> v(stdfs::file_size(path));
		auto stream = std::ifstream(path);
		stream.exceptions(std::ios_base::badbit);
		stream.read(v.data(), v.size());
		return v;
	}
}

#endif
