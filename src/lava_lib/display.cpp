#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display.h"


namespace lava {

Display::Display(const std::string& driver_name): driver_name(driver_name) {}

Display::UniquePtr Display::create(const std::string& driver_name) {
	char *error;
	char *lava_home = getenv("LAVA_HOME");

	boost::format libdspy_name("%1%/etc/d_%2%.so");
	libdspy_name % lava_home % driver_name;

	void* lib_handle = dlopen(libdspy_name.str().c_str(), RTLD_NOW);
	if (!lib_handle) {
        printf("[%s] Unable to load display driver: %s\n", __FILE__, dlerror());
        return nullptr;
    }

    Display* pDisplay = new Display(driver_name);

    pDisplay->m_OpenFunc = (PtDspyOpenFuncPtr)dlsym(lib_handle, "DspyImageOpen");
    if ((error = dlerror()) != NULL)  {
        fputs(error, stderr);
        exit(1);
    }
    pDisplay->m_WriteFunc = (PtDspyWriteFuncPtr)dlsym(lib_handle, "DspyImageWrite");
    pDisplay->m_CloseFunc = (PtDspyCloseFuncPtr)dlsym(lib_handle, "DspyImageClose");

    return UniquePtr(pDisplay);
}

}  // namespace lava