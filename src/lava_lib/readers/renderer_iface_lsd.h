#ifndef SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_
#define SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_

#include <memory>

#include "grammar_lsd.h"
#include "../renderer_iface_base.h"

namespace lava {

class RendererIfaceLSD: public RendererIfaceBase {

 public:
    RendererIfaceLSD(Renderer *renderer);
    ~RendererIfaceLSD();

 public:
 	bool loadDisplayByType(const lsd::ast::DisplayType& display_type);
 	bool loadDisplayByFileName(const std::string& file_name);

 	void cmdRaytrace();
    void cmdConfig(const std::string& file_name);

 private:
 	bool initRenderData();

 private:
 	std::vector<std::string> mGraphConfigs;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_RENDERER_IFACE_LSD_H_