#ifndef SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_
#define SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_

#include <stdint.h>
#include <string>

#include <Falcor/Core/Framework.h>

#include "MurmurHash.h"
#include "lava_utils_lib/logging.h"

namespace Falcor {

namespace Cryptomatte {

using ManifestMap = std::map<std::string, float>;

// System values
constexpr size_t MAX_STRING_LENGTH = 2048;
constexpr size_t MAX_CRYPTOMATTE_DEPTH = 99;
constexpr size_t MAX_USER_CRYPTOMATTES = 16;

enum class CryptoNameFlags : uint8_t {
  CRYPTO_NAME_NONE          = 0x00,
  CRYPTO_NAME_STRIP_NS      = 0x01, /* remove "namespace" */
  CRYPTO_NAME_MAYA          = 0x02, /* mtoa style */
  CRYPTO_NAME_PATHS         = 0x04, /* path based (starting with "/") */
  CRYPTO_NAME_OBJPATHPIPES  = 0x08, /* pipes are considered in paths (c4d) */
  CRYPTO_NAME_MATPATHPIPES  = 0x10, /* sitoa, old-c4d style */
  CRYPTO_NAME_LEGACY        = 0x20, /* sitoa, old-c4d style */
  CRYPTO_NAME_ALL           = 255
};

enum_class_operators(CryptoNameFlags);

namespace {

static constexpr size_t SOFTIMAGE_POINTCLOUD_INSTANCE_VERBOSITY = 0;

inline void safeCopyToBuffer(char buffer[MAX_STRING_LENGTH], const char* c) {
  if (c) strncpy(buffer, c, std::min(strlen(c), (size_t)MAX_STRING_LENGTH - 1));
  else buffer[0] = '\0';
}

inline bool softimagePointcloudInstanceHandling(const char* obj_full_name, char obj_name_out[MAX_STRING_LENGTH]) {
  if (SOFTIMAGE_POINTCLOUD_INSTANCE_VERBOSITY == 0 || !strstr(obj_full_name, ".SItoA.Instance.")) return false;
  
  char obj_name[MAX_STRING_LENGTH];
  safeCopyToBuffer(obj_name, obj_full_name);

  char* instance_start = strstr(obj_name, ".SItoA.Instance.");
  if (!instance_start) return false;

  char* space = strstr(instance_start, " ");
  if (!space) return false;

  char* instance_name = &space[1];
  char* obj_suffix2 = strstr(instance_name, ".SItoA.");
  if (!obj_suffix2) return false;

  obj_suffix2[0] = '\0'; // strip the suffix
  size_t chars_to_copy = strlen(instance_name);
  if (chars_to_copy >= MAX_STRING_LENGTH || chars_to_copy == 0) return false;
  
  if (SOFTIMAGE_POINTCLOUD_INSTANCE_VERBOSITY == 2) {
    char* frame_numbers = &instance_start[16]; // 16 chars in ".SItoA.Instance.", this gets us to the first number
    char* instance_ID = strstr(frame_numbers, ".");
    if (!instance_ID) return false;

    char* instance_ID_end = strstr(instance_ID, " ");
    if (!instance_ID_end) return false;
    
    instance_ID_end[0] = '\0';
    size_t ID_len = strlen(instance_ID);
    strncpy(&instance_name[chars_to_copy], instance_ID, ID_len);
    chars_to_copy += ID_len;
  }

  strncpy(obj_name_out, instance_name, chars_to_copy);
  return true;
}

inline void stripNamespaces(const char* obj_full_name, char obj_name_out[MAX_STRING_LENGTH]) {
  char* to = obj_name_out;
  size_t len = 0;
  size_t sublen = 0;
  const char* from = obj_full_name;
  const char* end = from + strlen(obj_full_name);
  const char* found = strchr(from, '|');
  const char* sep = nullptr;

  while (found) {
    sep = strchr(from, ':');
    if (sep && sep < found) {
      from = sep + 1;
    }
    sublen = found - from;
    memmove(to, from, sublen);
    to[sublen] = '|';

    len += sublen + 1;
    to += sublen + 1;
    from = found + 1;

    found = strchr(from, '|');
  }

  sep = strchr(from, ':');
  if (sep && sep < end) {
    from = sep + 1;
  }
  sublen = end - from;
  memmove(to, from, sublen);
  to[sublen] = '\0';
}

}

inline void getCleanObjectName(const char* obj_full_name, char obj_name_out[MAX_STRING_LENGTH], char ns_name_out[MAX_STRING_LENGTH], CryptoNameFlags flags) {
  if (flags == CryptoNameFlags::CRYPTO_NAME_NONE) {
      memmove(obj_name_out, obj_full_name, strlen(obj_full_name));
      strcpy(ns_name_out, "default");
      return;
  }

  char ns_name[MAX_STRING_LENGTH] = "";
  safeCopyToBuffer(ns_name, obj_full_name);
  bool obj_already_done = false;

  const bool do_strip_ns = is_set(flags, CryptoNameFlags::CRYPTO_NAME_STRIP_NS);
  const bool do_maya = is_set(flags, CryptoNameFlags::CRYPTO_NAME_MAYA);
  const bool do_paths = is_set(flags, CryptoNameFlags::CRYPTO_NAME_PATHS);
  const bool do_path_pipe = is_set(flags, CryptoNameFlags::CRYPTO_NAME_OBJPATHPIPES);
  const bool do_legacy = is_set(flags, CryptoNameFlags::CRYPTO_NAME_LEGACY);

  const uint8_t mode_maya = 0;
  const uint8_t mode_pathstyle = 1;
  const uint8_t mode_si = 2;
  const uint8_t mode_c4d = 3;

  uint8_t mode = mode_maya;
  if (ns_name[0] == '/') {
    // Path-style: /obj/hierarchy|obj_cache_hierarchy
    // For instance: /Null/Sphere
    //               /Null/Cloner|Null/Sphere1
    mode = mode_pathstyle;
  } else if (do_legacy && strncmp(ns_name, "c4d|", 4) == 0) {
    // C4DtoA prior 2.3: c4d|obj_hierarchy|...
    mode = mode_c4d;
    const char* nsp = ns_name + 4;
    size_t len = strlen(nsp);
    memmove(ns_name, nsp, len);
    ns_name[len] = '\0';
  } else if (do_legacy && strstr(ns_name, ".SItoA.")) {
    // in Softimage mode
    mode = mode_si;
    char* sitoa_suffix = strstr(ns_name, ".SItoA.");
    obj_already_done = softimagePointcloudInstanceHandling(obj_full_name, obj_name_out);
    sitoa_suffix[0] = '\0'; // cut off everything after the start of .SItoA
  } else {
    mode = mode_maya;
  }

  char* nsp_separator = nullptr;
  if (mode == mode_c4d && do_legacy) {
    nsp_separator = strrchr(ns_name, '|');
  } else if (mode == mode_pathstyle && do_paths) {
    char* lastPipe = do_path_pipe ? strrchr(ns_name, '|') : nullptr;
    char* lastSlash = strrchr(ns_name, '/');
    nsp_separator = lastSlash > lastPipe ? lastSlash : lastPipe;
  } else if (mode == mode_si && do_legacy) {
    nsp_separator = strchr(ns_name, '.');
  } else if (mode == mode_maya && do_maya)
    nsp_separator = strchr(ns_name, ':');

  if (!obj_already_done) {
    if (!nsp_separator || !do_strip_ns) { // use whole name
      memmove(obj_name_out, ns_name, strlen(ns_name));
    } else if (mode == mode_maya) { // maya
      stripNamespaces(ns_name, obj_name_out);
    } else { // take everything right of sep
      char* obj_name_start = nsp_separator + 1;
      memmove(obj_name_out, obj_name_start, strlen(obj_name_start));
    }
  }

  if (nsp_separator) {
    nsp_separator[0] = '\0';
    strcpy(ns_name_out, ns_name); // copy namespace
  } else {
    strcpy(ns_name_out, "default");
  }
}

inline void getCleanMaterialName(const char* mat_full_name, char mat_name_out[MAX_STRING_LENGTH], CryptoNameFlags flags) {
  safeCopyToBuffer(mat_name_out, mat_full_name);
  if (flags == CryptoNameFlags::CRYPTO_NAME_NONE) return;

  const bool do_strip_ns = is_set(flags, CryptoNameFlags::CRYPTO_NAME_STRIP_NS);
  const bool do_maya = is_set(flags, CryptoNameFlags::CRYPTO_NAME_MAYA);
  const bool do_paths = is_set(flags, CryptoNameFlags::CRYPTO_NAME_PATHS);
  const bool do_strip_pipes = is_set(flags, CryptoNameFlags::CRYPTO_NAME_MATPATHPIPES);
  const bool do_legacy = is_set(flags, CryptoNameFlags::CRYPTO_NAME_LEGACY);

  // Path Style Names /my/mat/name|root_node_name
  if (do_paths && mat_name_out[0] == '/') {
    char* mat_name = do_strip_pipes ? strtok(mat_name_out, "|") : nullptr;
    mat_name = mat_name ? mat_name : mat_name_out;
    if (do_strip_ns) {
      char* ns_separator = strrchr(mat_name, '/');
      if (ns_separator) mat_name = ns_separator + 1;
    }
    if (mat_name != mat_name_out) memmove(mat_name_out, mat_name, strlen(mat_name) + 1);
    return;
  }

  // C4DtoA prior 2.3: c4d|mat_name|root_node_name
  if (do_legacy) {
    if (strncmp(mat_name_out, "c4d|", 4) == 0) {
      char* mat_name = strtok(mat_name_out + 4, "|");
      if (mat_name) memmove(mat_name_out, mat_name, strlen(mat_name) + 1);
      return;
    }
  }

  // For maya, you get something simpler, like namespace:my_material_sg.
  if (do_maya) {
    char* ns_separator = strchr(mat_name_out, ':');
    if (do_strip_ns && ns_separator) {
      ns_separator[0] = '\0';
      char* mat_name = ns_separator + 1;
      memmove(mat_name_out, mat_name, strlen(mat_name) + 1);
      return;
    }
  }

  // Softimage: Sources.Materials.myLibraryName.myMatName.Standard_Mattes.uBasic.SITOA.25000....
  if (do_legacy) {
    char* mat_postfix = strstr(mat_name_out, ".SItoA.");
    if (mat_postfix) {
      char* mat_name = mat_name_out;
      mat_postfix[0] = '\0';

      char* mat_shader_name = strrchr(mat_name, '.');
      if (mat_shader_name)
        mat_shader_name[0] = '\0';

      char* standard_mattes = strstr(mat_name, ".Standard_Mattes");
      if (standard_mattes)
        standard_mattes[0] = '\0';

      const char* prefix = "Sources.Materials.";
      char* mat_prefix_separator = strstr(mat_name, prefix);
      if (mat_prefix_separator) mat_name = mat_prefix_separator + strlen(prefix);

      char* nsp_separator = strchr(mat_name, '.');
      if (do_strip_ns && nsp_separator) {
        nsp_separator[0] = '\0';
        mat_name = nsp_separator + 1;
      }
      if (mat_name != mat_name_out) memmove(mat_name_out, mat_name, strlen(mat_name) + 1);
      return;
    }
  }
}

inline void writeManifestToString(const ManifestMap& map, std::string& manf_string) {
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

inline void writeManifestSidecarFile(const ManifestMap& map_md_asset, const std::vector<std::string>& manifest_paths) {
  std::string encoded_manifest = "";
  writeManifestToString(map_md_asset, encoded_manifest);
  for (const auto& manifest_path : manifest_paths) {
    std::ofstream out(manifest_path.c_str());
    LLOG_INF << "[Cryptomatte] writing file, " << manifest_path;
    out << encoded_manifest.c_str();
    out.close();
  }
}

}  // namespace Cryptomatte

}  // namespace Falcor

#endif  // SRC_FALCOR_UTILS_CRYPTOMATTE_CRYPTOMATTE_H_