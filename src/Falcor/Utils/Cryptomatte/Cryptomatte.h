#ifndef SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_
#define SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_

#include <stdint.h>
#include <string>
#include "lava_utils_lib/logging.h"

namespace Falcor {

namespace {
	using ManifestMap = std::map<std::string, float>;

	// System values
	#define MAX_STRING_LENGTH 2048
	#define MAX_CRYPTOMATTE_DEPTH 99
	#define MAX_USER_CRYPTOMATTES 16
}

inline void write_manifest_to_string(const ManifestMap& map, std::string& manf_string) {
  ManifestMap::const_iterator map_it = map.begin();
  const size_t map_entries = map.size();
  const size_t max_entries = 100000;
  size_t metadata_entries = map_entries;
  if (map_entries > max_entries) {
    LLOG_WRN << "Cryptomatte: " << std::to_string(map_entries) << " entries in manifest, limiting to " << std::to_string(max_entries);
    metadata_entries = max_entries;
  }

  manf_string.append("{");
  std::string pair;
  pair.reserve(MAX_STRING_LENGTH);
  for (uint32_t i = 0; i < metadata_entries; i++) {
      std::string name = map_it->first;
      float hash_value = map_it->second;
      ++map_it;

      uint32_t float_bits;
      std::memcpy(&float_bits, &hash_value, 4);
      char hex_chars[9];
      sprintf(hex_chars, "%08x", float_bits);

      pair.clear();
      pair.append("\"");
      for (size_t j = 0; j < name.length(); j++) {
          // append the name, char by char
          const char c = name.at(j);
          if (c == '"' || c == '\\' || c == '/')
              pair += "\\";
          pair += c;
      }
      pair.append("\":\"");
      pair.append(hex_chars);
      pair.append("\"");
      if (i < map_entries - 1)
          pair.append(",");
      manf_string.append(pair);
  }
  manf_string.append("}");
}

inline void write_manifest_sidecar_file(const ManifestMap& map_md_asset, const std::vector<std::string>& manifest_paths) {
  std::string encoded_manifest = "";
  write_manifest_to_string(map_md_asset, encoded_manifest);
  for (const auto& manifest_path : manifest_paths) {
    std::ofstream out(manifest_path.c_str());
    LLOG_INF << "[Cryptomatte] writing file, " << manifest_path;
    out << encoded_manifest.c_str();
    out.close();
  }
}

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_