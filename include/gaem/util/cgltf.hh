#ifndef GAEM_UTIL_CGLTF_HH_
#define GAEM_UTIL_CGLTF_HH_
#include <cgltf.h>
#include <gaem/common.hh>
#include <gaem/util/log.hh>

namespace gaem::util::cgltf {
	const char *result_string(cgltf_result res) {
		switch(res) {
		case cgltf_result_success: return "success";
		case cgltf_result_data_too_short: return "data too short";
		case cgltf_result_unknown_format: return "unknown format";
		case cgltf_result_invalid_json: return "invalid json";
		case cgltf_result_invalid_gltf: return "invalid gltf";
		case cgltf_result_invalid_options: return "invalid options";
		case cgltf_result_file_not_found: return "file not found";
		case cgltf_result_io_error: return "io error";
		case cgltf_result_out_of_memory: return "out of memory";
		case cgltf_result_legacy_gltf: return "legacy gltf";
		default: break;
		}
		return "unknown";
	};
}

#endif
