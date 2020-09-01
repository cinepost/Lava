#ifndef SRC_LAVA_LIB_LOADER_LSD_H_
#define SRC_LAVA_LIB_LOADER_LSD_H_

#include <boost/lambda/bind.hpp>
#include <boost/spirit/include/classic.hpp>

#include "loader_base.h"
#include "renderer_iface_lsd.h"
#include "syntax_lsd.h"


namespace lava {

class LoaderLSD: public LoaderBase {
 public:
 	LoaderLSD();
 	~LoaderLSD();

 private:
 	virtual void parseLine(const std::string& line);

 private:
 	SyntaxLSD* 						mpSyntax;
 	RendererIfaceLSD::SharedPtr 	mpIface;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_LOADER_LSD_H_
