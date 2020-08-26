/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#ifndef HDLAVA_ERROR_H_
#define HDLAVA_ERROR_H_

#include "debugCodes.h"

#include "pxr/base/arch/functionLite.h"
#include "pxr/base/tf/stringUtils.h"

#include "lava/lava.h"

#include <stdexcept>
#include <cassert>
#include <string>

#define LAVA_ERROR_CHECK_THROW(status, msg, ...) \
    do { \
        auto st = status; \
        if (st != LAVA_SUCCESS) { \
            assert(false); \
            throw LavaUsdError(st, msg, __ARCH_FILE__, __ARCH_FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0);

#define LAVA_ERROR_CHECK(status, msg, ...) \
    LavaUsdFailed(status, msg, __ARCH_FILE__, __ARCH_FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LAVA_GET_ERROR_MESSAGE(status, msg, ...) \
    LavaUsdConstructErrorMessage(status, msg, __ARCH_FILE__, __ARCH_FUNCTION__, __LINE__, ##__VA_ARGS__)

#define LAVA_THROW_ERROR_MSG(fmt, ...) \
    LavaUsdThrowErrorMsg(__ARCH_FILE__, __ARCH_FUNCTION__, __LINE__, fmt, ##__VA_ARGS__);

PXR_NAMESPACE_OPEN_SCOPE

inline std::string LavaUsdConstructErrorMessage(lava::Status errorStatus, std::string const& messageOnFail, char const* file, char const* function, size_t line, lava::Renderer* renderer = nullptr) {
    auto rprErrorString = [errorStatus, renderer]() -> std::string {
        if (renderer) {
            size_t lastErrorMessageSize = 0;
            auto status = renderer->getInfo(lava::LAVA_RENDERER_LAST_ERROR_MESSAGE, 0, nullptr, &lastErrorMessageSize);
            if (status == lava::LAVA_SUCCESS && lastErrorMessageSize > 1) {
                std::string message(lastErrorMessageSize, '\0');
                status = renderer->getInfo(lava::LAVA_RENDERER_LAST_ERROR_MESSAGE, message.size(), &message[0], nullptr);
                if (status == lava::LAVA_SUCCESS) {
                    return message;
                }
            }
        }

        switch (errorStatus) {
            case lava::LAVA_ERROR_INVALID_API_VERSION: return "invalid api version";
            case lava::LAVA_ERROR_INVALID_PARAMETER: return "invalid parameter";
            case lava::LAVA_ERROR_UNSUPPORTED: return "unsupported";
            case lava::LAVA_ERROR_INTERNAL_ERROR: return "internal error";
            default:
                break;
        }

        return "error code - " + std::to_string(errorStatus);
    };

    auto suffix = TfStringPrintf(" in %s at line %zu of %s", function, line, file);
#ifdef LAVA_GIT_SHORT_HASH
    suffix += TfStringPrintf("(%s)", LAVA_GIT_SHORT_HASH);
#endif // LAVA_GIT_SHORT_HASH
    if (errorStatus == lava::LAVA_SUCCESS) {
        return TfStringPrintf("[LAVA ERROR] %s%s", messageOnFail.c_str(), suffix.c_str());
    } else {
        auto errorStr = rprErrorString();
        return TfStringPrintf("[LAVA ERROR] %s -- %s%s", messageOnFail.c_str(), errorStr.c_str(), suffix.c_str());
    }
}

inline bool LavaUsdFailed(lava::Status status, const char* messageOnFail, char const* file, char const* function, size_t line, lava::Renderer* renderer = nullptr) {
    if (lava::LAVA_SUCCESS == status) {
        return false;
    }
    if ((status == lava::LAVA_ERROR_UNSUPPORTED || status == lava::LAVA_ERROR_UNIMPLEMENTED) && !TfDebug::IsEnabled(LAVA_USD_DEBUG_CORE_UNSUPPORTED_ERROR)) {
        return true;
    }

    auto errorMessage = LavaUsdConstructErrorMessage(status, messageOnFail, file, function, line, renderer);
    fprintf(stderr, "%s\n", errorMessage.c_str());
    return true;
}

class LavaUsdError : public std::runtime_error {
 public:
    LavaUsdError(lava::Status errorStatus, const char* messageOnFail, char const* file, char const* function, size_t line, lava::Renderer* renderer = nullptr)
        : std::runtime_error(LavaUsdConstructErrorMessage(errorStatus, messageOnFail, file, function, line, renderer)) {

    }

    LavaUsdError(std::string const& errorMesssage)
        : std::runtime_error(errorMesssage) {

    }
};

inline void LavaUsdThrowErrorMsg(char const* file, char const* function, size_t line, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto messageOnFail = TfVStringPrintf(fmt, ap);
    va_end(ap);
    throw LavaUsdError(LavaUsdConstructErrorMessage(lava::LAVA_SUCCESS, messageOnFail, file, function, line));
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDLAVA_ERROR_H_
