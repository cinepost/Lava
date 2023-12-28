#include <fstream>
#include <istream>
#include <iostream>
#include <string>
#include <csignal>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <execinfo.h>
#endif
#include <signal.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <boost/algorithm/string/join.hpp>
namespace po = boost::program_options;

#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Core/Platform/OS.h"
#include "Falcor/Utils/Scripting/Scripting.h"
#include "Falcor/Utils/Timing/Profiler.h"

#include "lava_lib/version.h"
#include "lava_lib/renderer.h"
#include "lava_lib/scene_readers_registry.h"
#include "lava_lib/reader_lsd/reader_lsd.h"

#include "lava_utils_lib/logging.h"

#define PRE_RELEASE_TRACEBACK_HANDLER

using namespace lava;

namespace {
  // In the GNUC Library, sig_atomic_t is a typedef for int,
  // which is atomic on all systems that are supported by the
  // GNUC Library
  volatile sig_atomic_t do_shutdown = 0;

  // std::atomic is safe, as long as it is lock-free
  std::atomic<bool> shutdown_requested = false;
  static_assert( std::atomic<bool>::is_always_lock_free );
  // or, at runtime: assert( shutdown_requested.is_lock_free() );
}

static std::chrono::high_resolution_clock::time_point gExecTimeStart;

void signalHandler( int signum ){
  // ok, lock-free atomics
  do_shutdown = 1;
  shutdown_requested = true;

  const char str[] = "received signal\n";
  // ok, write is signal-safe
  write(STDERR_FILENO, str, sizeof(str) - 1);
}

#ifdef _WIN32
  // traceback not implemented
#else
void signalTraceHandler( int signum ){
  lava::ut::log::shutdown_log();
#ifdef PRE_RELEASE_TRACEBACK_HANDLER
  fprintf(stderr, "****************************************\n");
  fprintf(stderr, "************ Lava Traceback ************\n");
  fprintf(stderr, "****************************************\n");
  fprintf(stderr, "Error: signal %d:\n", signum);
  void *array[30];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 30);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
  exit(signum);
}
#endif

void atexitHandler()  {
  lava::ut::log::shutdown_log();

  auto duration = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::high_resolution_clock::now() - gExecTimeStart ).count();
  std::cout << "Scene rendered in: " << duration << " sec.\n";
  std::cout << "Exiting lava. Bye :)\n";
}

void listGPUs() {
  auto pDeviceManager = DeviceManager::create();
  std::cout << "Available rendering devices:\n";
  const auto& deviceMap = pDeviceManager->listDevices();
  for( auto const& [gpu_id, name]: deviceMap ) {
    std::cout << "\t[" << std::to_string(static_cast<uint32_t>(gpu_id)) << "] : " << name << "\n";
  }
  std::cout << std::endl;
}

typedef std::basic_ifstream<wchar_t, std::char_traits<wchar_t> > wifstream;

#ifdef FALCOR_ENABLE_PROFILER
Profiler* gProfiler = nullptr;
#endif

static void writeProfilerStatsToFile(const std::string& outputFilename) {
#ifdef FALCOR_ENABLE_PROFILER
  gProfiler->endFrame();
  Falcor::Profiler::Capture::SharedPtr profilerCapture = gProfiler->endCapture();
  if(profilerCapture) {
    profilerCapture->writeToFile(outputFilename, Falcor::Profiler::Capture::OuputFactory::BOOST_JSON);
    std::cout << "Profiler performance stats are written to file \'" << outputFilename << "\'";
  }
#endif
}


int main(int argc, char** argv){

    gExecTimeStart = std::chrono::high_resolution_clock::now();

    int gpuID = -1; // automatic gpu selection

    bool read_stdin = false;

#ifdef _DEBUG
    bool echo_input = true;
#else
    bool echo_input = false;
#endif

    std::atexit(atexitHandler);
    #ifdef _WIN32
      // traceback not implemented
    #else
    // setup signal handlers
    {
      struct sigaction action;
      action.sa_handler = signalHandler;
      sigemptyset(&action.sa_mask);
      action.sa_flags = 0;
      sigaction(SIGTERM, &action, NULL);
    }

    signal(SIGABRT, signalTraceHandler);
    signal(SIGSEGV, signalTraceHandler);
    #endif


    /// Program options
    std::string config_file;

    // Declare a group of options that will be allowed only on command line
    namespace po = boost::program_options; 
    po::options_description generic("Options"); 
    generic.add_options() 
      ("help,h", "Show help") 
      ("version,v", "Shout version information")
      ("list-devices,L", "List rendering devices")
      ;

    // Declare a group of options that will be allowed both on command line and in config file
    bool vtoff_flag = false; // virtual texturing enabled by default
    bool fconv_flag = false; // force virtual textures (re)conversion
    po::options_description config("Configuration");
    config.add_options()
      ("device,d", po::value<int>(&gpuID)->default_value(0), "Use specific device")
      ("vtoff", po::bool_switch(&vtoff_flag), "Turn off vitrual texturing")
      ("fconv", po::bool_switch(&fconv_flag), "Force textures (re)conversion")
      ("include-path,i", po::value< std::vector<std::string> >()->composing(), "Include path")
      ;

    std::string logFilename = "";
#ifdef _DEBUG
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::debug;
#else
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::warning;
#endif
    po::options_description logging("Logging");
    logging.add_options()
      ("log-level,l", po::value<boost::log::trivial::severity_level>(&logSeverity)->default_value(logSeverity),"Logging level")
      ("log-file", po::value<std::string>(&logFilename), "Output log to file")
      ;

    std::vector<std::string> inputFilenames;
    po::options_description input("Input");
    input.add_options()
      ("stdin,C", po::bool_switch(&read_stdin), "Read scene from stdin")
      ("echo-input,e", po::bool_switch(&echo_input), "Echo input scene")
      ("input-files,f", po::value< std::vector<std::string> >(&inputFilenames), "Input files")
      ;

    const std::string profilerCaptureDefaultFilename = "lava_profiling_stats.json";
    std::string vkValidationFilename;
    std::string profilerCaptureFilename;
    po::options_description profiling("Profiling");
    profiling.add_options()
      ("vk-validate", po::value<std::string>(&vkValidationFilename)->default_value(vkValidationFilename), "Output Vulkan validation info");
      ("perf-file", po::value<std::string>(&profilerCaptureFilename)->default_value(profilerCaptureDefaultFilename), "Output profiling file")
      ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config).add(input);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::options_description visible("Allowed options");
    visible.add(generic).add(config).add(input).add(logging).add(profiling);

    po::variables_map vm;  

    // Handle config file
    std::ifstream ifs(config_file.c_str());
    if (ifs) {
      try {
        po::store(parse_config_file(ifs, config_file_options), vm);
      } catch ( po::error& e ) {
        LLOG_ERR << e.what();
      } 
    }

    try {
      po::store(po::command_line_parser(argc, argv).options(visible).run(), vm); // can throw 
      po::notify(vm); // throws on error, so do after help in case there are any problems
    } catch ( po::error& e ) {
      LLOG_FTL << e.what();
      exit(EXIT_FAILURE);
    } 

    // Set up logging 
    lava::ut::log::init_log();
    if(logFilename != "") lava::ut::log::init_file_log(logFilename);
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >= logSeverity );

    /** --help option 
     */ 
    if ( vm.count("help")  ) { 
      std::cout << generic << "\n";
      std::cout << config << "\n";
      std::cout << input << "\n";
      std::cout << logging << "\n";
#ifdef FALCOR_ENABLE_PROFILER
      std::cout << profiling << "\n";
#endif
      exit(EXIT_SUCCESS);
    }

    if ( vm.count("list-devices")) {
      listGPUs();
      exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
      std::cout << "Lava, version " << lava::versionString() << "\n";
      exit(EXIT_SUCCESS);
    }

    auto& app_config = ConfigStore::instance();
    if(vtoff_flag) {
      app_config.set<bool>("vtoff", true);
    }

    if(fconv_flag) {
      app_config.set<bool>("fconv", true);
    }

    // Early termination ...

    // ---------------------

    std::cout << "Lava version " << lava::versionString() << "\n";
    LLOG_INF << "Lava mode: " << (isDevelopmentMode() ? "development" : "production");
    
    const bool enableValidationLayer = vkValidationFilename.empty() ? false : true;


    // Populate Renderer_IO_Registry with internal and external scene translators
    SceneReadersRegistry::getInstance().addReader(
      ReaderLSD::myExtensions, 
      ReaderLSD::myConstructor
    );

    {
      auto pDeviceManager = DeviceManager::create(enableValidationLayer);
      if (!pDeviceManager) exit(EXIT_FAILURE);

      pDeviceManager->setDefaultRenderingDevice(gpuID);

      Falcor::Device::Desc device_desc;
      device_desc.width = 1280;
      device_desc.height = 720;
      device_desc.validationLayerOuputFilename = vkValidationFilename;

      LLOG_DBG << "Creating rendering device id " << to_string(gpuID);
      auto pDevice = pDeviceManager->createRenderingDevice(gpuID, device_desc);
      LLOG_DBG << "Rendering device " << to_string(gpuID) << " created";

      // Start scripting system
      Falcor::Scripting::start();

      // Start profiler if needed
#ifdef FALCOR_ENABLE_PROFILER
      gProfiler = Falcor::Profiler::instancePtr(pDevice).get();
      gProfiler->setEnabled(true);
      gProfiler->startCapture();
 #endif

      Renderer::SharedPtr pRenderer = Renderer::create(pDevice);

      // main loop
      while( !do_shutdown && !shutdown_requested.load() ) {

        if (vm.count("input-files") && !read_stdin) {
          // Reading scenes from files...
          for (const std::string& inputFilename: inputFilenames) {
            std::ifstream in_file(inputFilename, std::ifstream::binary);
            if(!in_file) {
              LLOG_ERR << "Unable to open scene file \'" << inputFilename << "\'' !\n";
              exit(EXIT_FAILURE);
            }
            
            auto reader = SceneReadersRegistry::getInstance().getReaderByExt(fs::extension(inputFilename));
            reader->init(pRenderer, echo_input);

            LLOG_DBG << "Reading \'"<< inputFilename << "\'' scene file with " << reader->formatName() << " reader";
            if (!reader->readStream(in_file)) {
              LLOG_ERR << "Error reading scene from file: " << inputFilename;
              exit(EXIT_FAILURE);
            }

            std::string _captureFilename = profilerCaptureFilename;;
            if(profilerCaptureFilename == profilerCaptureDefaultFilename) {
              fs::path p(inputFilename);
              _captureFilename = p.string() + ".profilig_stats.json";
            }
            writeProfilerStatsToFile(_captureFilename);
          }
        } else {
          LLOG_DBG << "Reading scene from stdin ...\n";
          auto reader = SceneReadersRegistry::getInstance().getReaderByExt(".lsd"); // default format for reading stdin is ".lsd"
          
          //echo_input = true; // TODO: remove

          reader->init(pRenderer, echo_input);

          if (!reader->readStream(std::cin)) {
            LLOG_ERR << "Error loading scene from stdin !";
            exit(EXIT_FAILURE);
          }
          writeProfilerStatsToFile(profilerCaptureFilename);
        }

      do_shutdown = 1;
      shutdown_requested = true;

      } // main while loop

      // Shutdown scripting system before destroying renderer !
      Falcor::Scripting::shutdown();
    }

    return 0;
}