#ifndef SRC_LAVA_LIB_DISPLAY_H_
#define SRC_LAVA_LIB_DISPLAY_H_

#include <string>
#include <vector>
#include <memory>


#include "prman/ndspy.h"

namespace lava {

class Display {
 public:
    using UniquePtr = std::unique_ptr<Display>;

    UniquePtr create(const std::string& driver_name);


 private:
    Display(const std::string& driver_name);

 private:
    std::string driver_name = "";

    PtDspyOpenFuncPtr               m_OpenFunc = nullptr;
    PtDspyWriteFuncPtr              m_WriteFunc;
    PtDspyDeepWriteFuncPtr          m_DeepWriteFunc;
    PtDspyActiveRegionFuncPtr       m_ActiveRegionFunc;
    PtDspyCloseFuncPtr              m_CloseFunc;
    PtDspyFlushFuncPtr              m_FlushFunc;
    PtDspyReopenFuncPtr             m_ReopenFunc;
    PtDspyDelayCloseFuncPtr         m_DelayCloseFunc;
    PtDspyQueryFuncPtr              m_QueryFunc;
};

}  // namespace lava

#endif  // SRC_LAVA_LIB_DISPLAY_H_