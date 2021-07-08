/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "stdafx.h"
// #include "Utils/StringUtils.h"
#include "Falcor/Core/Platform/OS.h"
// #include "Utils/Logger.h"
//

#ifdef _DEBUG
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/ptrace.h>
#endif

// #include <gtk/gtk.h>
// #include <fstream>
#include <fcntl.h>
// #include <libgen.h>
// #include <errno.h>
// #include <algorithm>
#include <dlfcn.h>

#include "Falcor/Utils/Debug/debug.h"

#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;


namespace Falcor {

enum class MsgResponseId {
    Cancel,
    Retry,
    Abort,
    Ignore
};

uint32_t msgBox(const std::string& msg, std::vector<MsgBoxCustomButton> buttons, MsgBoxIcon icon, uint32_t defaultButtonId) {
    return -1;
}

MsgBoxButton msgBox(const std::string& msg, MsgBoxType mbType, MsgBoxIcon icon) {
    return MsgBoxButton::Ignore;
}

std::string exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

size_t executeProcess(const std::string& appName, const std::string& commandLineArgs) {
    std::string linuxAppName = getExecutableDirectory(); linuxAppName += "/" + appName;
    std::vector<const char*> argv;
    std::vector<std::string> argvStrings;

    auto argStrings = splitString(commandLineArgs, " ");
    argvStrings.insert(argvStrings.end(), argStrings.begin(), argStrings.end());

    for (const std::string& argString : argvStrings ) {
        argv.push_back(argString.c_str());
    }
    argv.push_back(nullptr);

    int32_t forkVal = fork();

    assert(forkVal != -1);
    if(forkVal == 0) {
        if (execv(linuxAppName.c_str(), (char* const*)argv.data())) {
            msgBox("Failed to launch process");
        }
    }

    return forkVal;
}

bool isProcessRunning(size_t processID) {
    // TODO
    return static_cast<bool>(processID);
}

void terminateProcess(size_t processID) {
    (void)processID;
    should_not_get_here();
}

bool doesFileExist(const std::string& filename) {
    int32_t handle = open(filename.c_str(), O_RDONLY);
    struct stat fileStat;
    bool exists = fstat(handle, &fileStat) == 0;
    close(handle);
    return exists;
}

bool isDirectoryExists(const std::string& filename) {
    const char* pathname = filename.c_str();
    struct stat sb;
    return (stat(pathname, &sb) == 0) && S_ISDIR(sb.st_mode);
}

void monitorFileUpdates(const std::string& filePath, const std::function<void()>& callback) {
    (void)filePath; (void)callback;
    should_not_get_here();
}

void closeSharedFile(const std::string& filePath) {
    (void)filePath;
    should_not_get_here();
}

std::string getTempFilename() {
    std::string filePath = fs::temp_directory_path().string();
    filePath += "/fileXXXXXX";

    // The if is here to avoid the warn_unused_result attribute on mkstemp
    if(mkstemp(&filePath.front())) {}
    return filePath;
}

const std::string& getExecutableDirectory() {
    char result[PATH_MAX] = { 0 };
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string path;
    if (count != -1) {
        fs::path p(result);
        path = p.parent_path().string().c_str();
    }
    static std::string strpath(path);
    return strpath;
}

const std::string getWorkingDirectory() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        return std::string(cwd);
    }

    return std::string();
}

const std::string getAppDataDirectory() {
    //assert(0);
    //return std::string();
    return "/home/max/dev/Falcor/src/Falcor/Data/";
}

const std::string& getExecutableName() {
    static std::string filename = fs::path(program_invocation_name).filename().string();
    return filename;
}

bool getEnvironmentVariable(const std::string& varName, std::string& value) {
    const char* val = ::getenv(varName.c_str());
    if (val == 0)
    {
        return false;
    }
    static std::string strvar(val);
    value = strvar;
    return true;
}

template<bool bOpen>
bool fileDialogCommon(const FileDialogFilterVec& filters, std::string& filename) {
    bool success = false;
    return success;
}

bool openFileDialog(const FileDialogFilterVec& filters, std::string& filename) {
    return fileDialogCommon<true>(filters, filename);
}

bool saveFileDialog(const FileDialogFilterVec& filters, std::string& filename) {
    return fileDialogCommon<false>(filters, filename);
}

void setActiveWindowIcon(const std::string& iconFile) {
    // #TODO Not yet implemented
}

int getDisplayDpi() {
    // #TODO Not yet implemented
    return int(200);
}

float getDisplayScaleFactor() {
    return 1;
}

bool isDebuggerPresent() {
#ifdef _DEBUG
    static bool debuggerAttached = false;
    static bool isChecked = false;
    if (isChecked == false) {
        if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
            debuggerAttached = true;
        } else {
            ptrace(PTRACE_DETACH, 0, 1, 0);
        }
        isChecked = true;
    }

    return debuggerAttached;
#else
    return false;
#endif
}

void debugBreak() {
//    raise(SIGTRAP);
}

void printToDebugWindow(const std::string& s) {
    std::cerr << s;
}

void enumerateFiles(std::string searchString, std::vector<std::string>& filenames) {
/*
    DIR* pDir = opendir(searchString.c_str());
    if (pDir != nullptr) {
        struct dirent* pDirEntry = readdir(pDir);
        while (pDirEntry != nullptr) {
            // Only add files, no subdirectories, symlinks, or other objects
            if (pDirEntry->d_type == DT_REG) {
                filenames.push_back(pDirEntry->d_name);
            }

            pDirEntry = readdir(pDir);
        }
    }
*/
}

std::thread::native_handle_type getCurrentThread() {
    return pthread_self();
}

std::string threadErrorToString(int32_t error) {
    // Error details can vary depending on what function returned it,
    // just convert error id to string for easy lookup.
    switch (error) {
        case EFAULT: return "EFAULT";
        case ENOTSUP: return "ENOTSUP";
        case EINVAL: return "EINVAL";
        case EPERM: return "EPERM";
        case ESRCH: return "ESRCH";
        default: return std::to_string(error);
    }
}

void setThreadAffinity(std::thread::native_handle_type thread, uint32_t affinityMask) {
    cpu_set_t cpuMask;
    CPU_ZERO(&cpuMask);

    uint32_t bitCount = std::min(sizeof(cpu_set_t), sizeof(uint32_t)) * 8;
    for (uint32_t i = 0; i < bitCount; i++) {
        if ((affinityMask & (1 << i)) > 0) {
            CPU_SET(i, &cpuMask);
        }
    }

    int32_t result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuMask);
    if (result != 0) {
        logError("setThreadAffinity() - pthread_setaffinity_np() failed with error code " + threadErrorToString(result));
    }
}

void setThreadPriority(std::thread::native_handle_type thread, ThreadPriorityType priority) {
    pthread_attr_t thAttr;
    int32_t policy = 0;
    pthread_getattr_np(thread, &thAttr);
    pthread_attr_getschedpolicy(&thAttr, &policy);

    int32_t result = 0;
    if (priority >= ThreadPriorityType::Lowest) {
        // Remap enum value range to what was queried from system
        float minPriority = (float)sched_get_priority_min(policy);
        float maxPriority = (float)sched_get_priority_max(policy);
        float value = (float)priority * (maxPriority - minPriority) / (float)(ThreadPriorityType::Highest) + minPriority;
        result = pthread_setschedprio(thread, (int32_t)value);
        pthread_attr_destroy(&thAttr);
    } else {
        // #TODO: Is there a "Background" priority in Linux? Is there a way to emulate it?
        should_not_get_here();
    }

    if (result != 0) {
        logError("setThreadPriority() - pthread_setschedprio() failed with error code " + threadErrorToString(result));
    }
}

time_t getFileModifiedTime(const std::string& filename) {
    struct stat s;
    if (stat(filename.c_str(), &s) != 0) {
        logError("Can't get file time for '" + filename + "'");
        return 0;
    }

    return s.st_mtime;
}

uint32_t bitScanReverse(uint32_t a) {
    // __builtin_clz counts 0's from the MSB, convert to index from the LSB
    // __builtin_ctz(0) produces undefined results.
    return (a > 0) ? ((sizeof(uint32_t) * 8) - (uint32_t)__builtin_clz(a) - 1) : 0;
}

/** Returns index of least significant set bit, or 0 if no bits were set
*/
uint32_t bitScanForward(uint32_t a) {
    // __builtin_ctz() counts 0's from LSB, which is the same as the index of the first set bit
    // Manually return 0 if a is 0 to match Microsoft behavior. __builtin_ctz(0) produces undefined results.
    return (a > 0) ? ((uint32_t)__builtin_ctz(a)) : 0;
}

uint32_t popcount(uint32_t a) {
    return (uint32_t)__builtin_popcount(a);
}

DllHandle loadDll(const std::string& libPath) {
    void *handle = dlopen(libPath.c_str(), RTLD_LAZY);

    if (!handle) {
        LOG_ERR("Cannot open library: %s", + dlerror());
        return nullptr;
    }
    
    return handle;
}

void releaseDll(DllHandle dll) {
    dlclose(dll);
}

/** Get a function pointer from a library
*/
void* getDllProcAddress(DllHandle dll, const std::string& funcName) {
    return dlsym(dll, funcName.c_str());
}

void postQuitMessage(int32_t exitCode) {
    //PostQuitMessage(exitCode);
    exit(exitCode);
}

void OSServices::start() {
    //d3d_call(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
}

void OSServices::stop() {
    //CoUninitialize();
}

}  // namespace Falcor
