#include "renderer_iface_lsd.h"


namespace lava {

RendererIfaceLSD::SharedPtr RendererIfaceLSD::create() {
	return RendererIfaceLSD::SharedPtr( new RendererIfaceLSD());
}

RendererIfaceLSD::RendererIfaceLSD() {

}

}  // namespace lava