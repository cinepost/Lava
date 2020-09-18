#include "renderer_iface_lsd.h"


namespace lava {

//RendererIfaceLSD::SharedPtr RendererIfaceLSD::create(Renderer *renderer) {
//	return RendererIfaceLSD::SharedPtr( new RendererIfaceLSD(renderer));
//}

RendererIfaceLSD::RendererIfaceLSD(Renderer *renderer): RendererIfaceBase(renderer) { }

RendererIfaceLSD::~RendererIfaceLSD() { }

}  // namespace lava