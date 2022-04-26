#include <fstream>
#include <istream>
#include <iostream>
#include <string>
#include <csignal>
#include <chrono>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
namespace po = boost::program_options;

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "Falcor/Core/API/DeviceManager.h"
#include "lava_utils_lib/logging.h"


using namespace lava;

static const uint32_t kWindowInitWidth = 1280;
static const uint32_t kWindowInitHeight = 720;  


static VkAllocationCallbacks*   g_Allocator = NULL;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;


static void check_vk_result(VkResult err) {
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

static void atexitHandler()  {
  lava::ut::log::shutdown_log();
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, Falcor::Device::SharedPtr pDevice, int width, int height) {
    VkInstance instance = pDevice->getVkInstance();
    VkPhysicalDevice physicalDevice = pDevice->getVkPhysicalDevice();
    VkDevice device = pDevice->getApiHandle();
    uint32_t queueFamily = pDevice->getApiCommandQueueType(Falcor::LowLevelContextData::CommandQueueType::Direct);

    wd->Surface = pDevice->getVkSurface();

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, wd->Surface, &res);
    if (res != VK_TRUE) {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(physicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(instance, physicalDevice, device, wd, queueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkanWindow(Falcor::Device::SharedPtr pDevice) {
    ImGui_ImplVulkanH_DestroyWindow(pDevice->getVkInstance(), pDevice->getApiHandle(), &g_MainWindowData, g_Allocator);
}


int main(int argc, char** argv){
    std::atexit(atexitHandler);

    /// Program options
    std::string configFile;

    // Declare a group of options that will be allowed only on command line
    namespace po = boost::program_options; 
    po::options_description generic("Options"); 
    generic.add_options() 
        ("help,h", "Show helps") 
        ("version,v", "Shout version information")
        ;

    po::options_description input("Input");
    input.add_options()
        ("stdin,C", "stdin compatibility mode")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(input);

    po::options_description visible("Allowed options");
    visible.add(generic).add(input);

    po::positional_options_description p;
    p.add("input-file", -1);
 
    po::variables_map vm; 
    po::store(po::command_line_parser(argc, argv). options(cmdline_options).positional(p).run(), vm); // can throw 
    po::notify(vm); // throws on error, so do after help in case there are any problems

    // Set up logger 
    lava::ut::log::init_log();
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >=  boost::log::trivial::debug );

    /** --help option 
     */ 
    if ( vm.count("help")  ) { 
      std::cout << generic << "\n";
      std::cout << input << "\n";
      exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
      std::cout << "Ltxview, version 0.0\n";
      exit(EXIT_SUCCESS);
    }

    // Handle config file
    std::ifstream ifs(configFile.c_str());
    if (ifs) {
        po::store(po::parse_config_file(ifs, cmdline_options), vm);
        notify(vm);
    }

    if (vm.count("input-file")) {
      // loading provided texture files
      std::vector<std::string> ltx_files = vm["input-file"].as< std::vector<std::string> >();
      
    }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        LLOG_FTL << "Error: " << SDL_GetError();
        exit(EXIT_FAILURE);
    }

    // Setup window
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Ltxview", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWindowInitWidth, kWindowInitHeight, window_flags);


    auto pDeviceManager = Falcor::DeviceManager::create();
    if (!pDeviceManager) exit(EXIT_FAILURE);

    VkInstance vulkanInstance = pDeviceManager->vulkanInstance();

    VkResult err;

    // Create Window Surface
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, vulkanInstance, &surface) == 0) {
        LLOG_FTL << "Failed to create Vulkan surface.";
        exit(EXIT_FAILURE);
    }

    Falcor::Device::Desc deviceDesc;
    deviceDesc.width = kWindowInitWidth;
    deviceDesc.height = kWindowInitHeight;
    deviceDesc.surface = surface;

    // Create device
    auto pDevice = pDeviceManager->createRenderingDevice(0, deviceDesc);
    if (!pDevice) {
        LLOG_FTL << "Failed to create vulkan rendering device.";
        exit(EXIT_FAILURE);
    }

    VkDevice            vulkanDevice = pDevice->getApiHandle();
    VkPhysicalDevice    physicalDevice = pDevice->getVkPhysicalDevice();
    uint32_t            queueFamily = pDevice->getApiCommandQueueType(Falcor::LowLevelContextData::CommandQueueType::Direct);
    auto                descriptorPool = pDevice->getGpuDescriptorPool()->getApiHandle(0);

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, pDevice, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();


    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanInstance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = vulkanDevice;
    init_info.QueueFamily = queueFamily;
    init_info.Queue = pDevice->getCommandQueueHandle(Falcor::LowLevelContextData::CommandQueueType::Direct, 1);
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = descriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);


    // Main loop
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Resize swap chain?
        if (g_SwapChainRebuild) {
            int width, height;
            SDL_GetWindowSize(window, &width, &height);
            if (width > 0 && height > 0) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(vulkanInstance, physicalDevice, vulkanDevice, &g_MainWindowData, queueFamily, g_Allocator, width, height, g_MinImageCount);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;
            }
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(vulkanDevice);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow(pDevice);
    pDeviceManager = nullptr; //CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();

    lava::ut::log::shutdown_log();
    std::cout << "Exiting ltxview. Bye :)\n";
    exit(EXIT_SUCCESS);
}