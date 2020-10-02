#ifndef SRC_LAVA_LIB_SESSION_LSD_H_
#define SRC_LAVA_LIB_SESSION_LSD_H_

#include <memory>

#include "grammar_lsd.h"
#include "../renderer_iface.h"

namespace lava {

class SessionLSD {

 public:
    SessionLSD(std::unique_ptr<RendererIface> pRendererIface);
    ~SessionLSD();

 public:
 	bool loadDisplayByType(const lsd::ast::DisplayType& display_type);
 	bool loadDisplayByFileName(const std::string& file_name);

 	void cmdSetEnv(const std::string& key, const std::string& value);
 	void cmdRaytrace();
    void cmdConfig(const std::string& file_name);

 private:
 	bool initRenderData();

 private:
 	std::unique_ptr<RendererIface> mpRendererIface;
 	std::vector<std::string> mGraphConfigs;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_SESSION_LSD_H_