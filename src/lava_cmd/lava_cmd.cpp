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

#include <execinfo.h>
#include <signal.h>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
namespace po = boost::program_options;

#include "Falcor/Utils/ConfigStore.h"
#include "Falcor/Core/API/DeviceManager.h"

#include "lava_lib/renderer.h"
#include "lava_lib/scene_readers_registry.h"
#include "lava_lib/reader_lsd/reader_lsd.h"

#include "lava_utils_lib/logging.h"

using namespace lava;

void signalHandler( int signum ){
  fprintf(stderr, "Error: signal %d:\n", signum);
  // cleanup and close up stuff here
  // terminate program
  exit(signum);
}

void signalTraceHandler( int signum ){
  fprintf(stderr, "Error: signal %d:\n", signum);
  // cleanup and close up stuff here
  // terminate program
  void *array[30];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 30);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(signum);
}

void atexitHandler()  {
  lava::ut::log::shutdown_log();
  std::cout << "Exiting lava. Bye :)\n";
}

void listGPUs() {
  auto pDeviceManager = DeviceManager::create();
  std::cout << "Available rendering devices:\n";
  const auto& deviceMap = pDeviceManager->listDevices();
  for( auto const& [gpu_id, name]: deviceMap ) {
    std::cout << "\t[" << to_string(gpu_id) << "] : " << name << "\n";
  }
  std::cout << std::endl;
}

typedef std::basic_ifstream<wchar_t, std::char_traits<wchar_t> > wifstream;

int main(int argc, char** argv){
    int gpuID = -1; // automatic gpu selection

    boost::log::trivial::severity_level logSeverity;
#ifdef DEBUG
    boost::log::trivial::severity_level logSeverityDefault = boost::log::trivial::debug;
#else
    boost::log::trivial::severity_level logSeverityDefault = boost::log::trivial::warning;
#endif
    bool echo_input = true;
    bool vtoff_flag = false; // virtual texturing enabled by default
    bool fconv_flag = false; // force virtual textures (re)conversion

    //std::atexit(atexitHandler);

    signal(SIGTERM, signalHandler);
    signal(SIGABRT, signalTraceHandler);
    signal(SIGSEGV, signalTraceHandler);

    /// Program options
    int opt;
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
    po::options_description config("Configuration");
    config.add_options()
      ("device,d", po::value<int>(&gpuID)->default_value(0), "Use specific device")
      ("vtoff", po::bool_switch(&vtoff_flag), "Turn off vitrual texturing")
      ("fconv", po::bool_switch(&fconv_flag), "Force textures (re)conversion")
      ("include-path,i", po::value< std::vector<std::string> >()->composing(), "Include path")
      ;

    po::options_description logging("Logging");
    logging.add_options()
      ("log-level,l", po::value<boost::log::trivial::severity_level>(&logSeverity)->default_value(logSeverityDefault),"log level to output")
      ;

    po::options_description input("Input");
    input.add_options()
      ("stdin,C", "stdin compatibility mode")
      ("input-file,f", po::value< std::vector<std::string> >(), "Input file")
      ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(config).add(input);

    po::options_description config_file_options;
    config_file_options.add(config);

    po::options_description visible("Allowed options");
    visible.add(generic).add(config).add(input).add(logging);

    po::positional_options_description p;
    p.add("input-file", -1);

    po::variables_map vm;  

    try {
      po::store(po::command_line_parser(argc, argv).options(visible).positional(p).run(), vm); // can throw 
      po::notify(vm); // throws on error, so do after help in case there are any problems
    } catch ( po::error& e ) {
      std::cout << e.what();
      std::cout << generic << "\n";
      std::cout << config << "\n";
      std::cout << input << "\n";
      std::cout << logging << "\n";
      exit(EXIT_FAILURE);
    } 

    /** --help option 
     */ 
    if ( vm.count("help")  ) { 
      std::cout << generic << "\n";
      std::cout << config << "\n";
      std::cout << input << "\n";
      std::cout << logging << "\n";
      exit(EXIT_SUCCESS);
    }

    // Set up logging 
    lava::ut::log::init_log();
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >= logSeverity );

    if ( vm.count("list-devices")) {
      listGPUs();
      exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
      std::cout << "Lava, version 0.0\n";
      exit(EXIT_SUCCESS);
    }

    // Handle config file
    std::ifstream ifs(config_file.c_str());
    if (!ifs) {
        LLOG_DBG << "No config file provided but that's totally fine.";
    } else {
        store(parse_config_file(ifs, config_file_options), vm);
        notify(vm);
    }

    auto& app_config = ConfigStore::instance();
    if(vtoff_flag) {
      app_config.set<bool>("vtoff", true);
    }

    if(fconv_flag) {
      app_config.set<bool>("fconv", true);
    }


    // Populate Renderer_IO_Registry with internal and external scene translators
    SceneReadersRegistry::getInstance().addReader(
      ReaderLSD::myExtensions, 
      ReaderLSD::myConstructor
    );


    auto pDeviceManager = DeviceManager::create();
    if (!pDeviceManager)
      exit(EXIT_FAILURE);

    pDeviceManager->setDefaultRenderingDevice(gpuID);

    Falcor::Device::Desc device_desc;
    device_desc.width = 1280;
    device_desc.height = 720;

    LLOG_DBG << "Creating rendering device id " << to_string(gpuID);
    auto pDevice = pDeviceManager->createRenderingDevice(gpuID, device_desc);
    LLOG_DBG << "Rendering device " << to_string(gpuID) << " created";

    Renderer::SharedPtr pRenderer = Renderer::create(pDevice);

    if(!pRenderer->init()) {
      exit(EXIT_FAILURE);
    }

    if (vm.count("input-file")) {
      // loading provided files
      std::vector<std::string> files = vm["input-file"].as< std::vector<std::string> >();
      BOOST_LOG_TRIVIAL(debug) << "Input scene files are: "<< boost::algorithm::join(files, " ") << "\n";
      for (std::vector<std::string>::const_iterator fi = files.begin(); fi != files.end(); ++fi) {
        std::ifstream in_file(*fi, std::ifstream::binary);
        
        if ( in_file ) {
          std::string file_extension = boost::filesystem::extension(*fi);
          BOOST_LOG_TRIVIAL(debug) << "ext " << file_extension;

          auto reader = SceneReadersRegistry::getInstance().getReaderByExt(file_extension);
          reader->init(pRenderer->aquireInterface(), echo_input);

          LLOG_DBG << "Reading "<< *fi << " scene file with " << reader->formatName() << " reader";
          if (!reader->readStream(in_file)) {
            // error loading scene from file
            LLOG_ERR << "Error loading scene from file: " << *fi;
          }
        } else {
          // error opening scene file
          std::cerr << "Unable to open file " << *fi << " ! aborting..." << std::endl;
        }
      }
    } else {
      // loading from stdin
      BOOST_LOG_TRIVIAL(debug) << "Reading scene from stdin ...\n";
      auto reader = SceneReadersRegistry::getInstance().getReaderByExt(".lsd"); // default format for reading stdin is ".lsd"
      reader->init(pRenderer->aquireInterface(), echo_input);

      if (!reader->readStream(std::cin)) {
        // error loading scene from stdin
        LLOG_ERR << "Error loading scene from stdin !";
      }
    }

    pDeviceManager = nullptr;

    exit(EXIT_SUCCESS);
}