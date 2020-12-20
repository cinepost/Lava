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

#include "Falcor/Core/API/DeviceManager.h"
#include "Falcor/Core/API/SparseResourceManager.h"

#include "lava_lib/renderer.h"
#include "lava_lib/scene_readers_registry.h"
#include "lava_lib/reader_lsd/reader_lsd.h"

#include "lava_utils_lib/logging.h"

using namespace lava;

void printUsage() {
    std::cout << "Usage: lava [options] [lsd] [outputimage]\n" << std::endl;

    std::cout << "Reads an scene from standard input and renders the image described.\n" << std::endl;

    std::cout << "If the first argument after options ends in .lsd, .rib or ..." << std::endl;
    std::cout << "will read the scene description from that file.  If the argument does not" << std::endl;
    std::cout << "have an extension it will be used as the output image/device\n" << std::endl;

    std::cout << "    Control Options:" << std::endl;
    std::cout << "        -e echo stdin" << std::endl;
    std::cout << "        -r file  Read render graph definition from file" << std::endl;
    std::cout << "        -f file  Read scene file specified instead of reading from stdin" << std::endl;
    std::cout << "        -v val  Set verbose level (i.e. -v 2)" << std::endl;
    std::cout << "                    0-6  Output varying degrees of rendering statistics" << std::endl;
    std::cout << "        -l logfile  Write log to file" << std::endl;
    std::cout << "        -C      Enable stdin compatibility mode." << std::endl;
}

void signalHandler( int signum ){
    LLOG_DBG << "Interrupt signal (" << signum << ") received !";

    // cleanup and close up stuff here
    // terminate program
    exit(signum);
}

void listGPUs() {
  std::cout << "Available devices:\n____________________________________________\n";
  const auto& deviceMap = Falcor::DeviceManager::instance().listDevices();
  for( auto const& [gpu_id, name]: deviceMap ) {
    std::cout << "[" << to_string(gpu_id) << "] : " << name << "\n";
  }
  std::cout << std::endl;
}

typedef std::basic_ifstream<wchar_t, std::char_traits<wchar_t> > wifstream;

int main(int argc, char** argv){
    // Set up logging level quick 
    lava::ut::log::init_log();
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >=  boost::log::trivial::debug );
    
    int gpuID = -1; // automatic gpu selection
    int verbose_level = 6;
    bool echo_input = true;
    bool vtoff_flag = false; // virtual texturing enabled by default

    signal(SIGTERM, signalHandler);
    signal(SIGABRT, signalHandler);


    /// Program options
    int opt;
    std::string config_file;

    // Declare a group of options that will be allowed only on command line
    namespace po = boost::program_options; 
    po::options_description generic("Options"); 
    generic.add_options() 
        ("help,h", "Show helps") 
        ("version,v", "Shout version information")
        ("list-gpus,L", "List GPUs")
        ;

    // Declare a group of options that will be allowed both on command line and in config file
    po::options_description config("Configuration");
    config.add_options()
        ("gpus,g", po::value<int>(&gpuID)->default_value(0), "Use specific gpu")
        ("vtoff", po::bool_switch(&vtoff_flag), "Turn off vitrual texturing")
        ("include-path,I", po::value< std::vector<std::string> >()->composing(), "Include path")
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
    visible.add(generic).add(config).add(input);

    po::positional_options_description p;
    p.add("input-file", -1);
 
    po::variables_map vm; 
    po::store(po::command_line_parser(argc, argv).
      options(cmdline_options).positional(p).run(), vm); // can throw 
    po::notify(vm); // throws on error, so do after help in case there are any problems

    /** --help option 
     */ 
    if ( vm.count("help")  ) { 
      std::cout << generic << "\n";
      std::cout << config << "\n";
      std::cout << input << "\n";
      exit(EXIT_SUCCESS);
    }

    if ( vm.count("list-gpus")) {
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

    // Populate Renderer_IO_Registry with internal and external scene translators
    SceneReadersRegistry::getInstance().addReader(
      ReaderLSD::myExtensions, 
      ReaderLSD::myConstructor
    );


    Falcor::DeviceManager::instance().setDefaultRenderingDevice(gpuID);
    Renderer::UniquePtr pRenderer = Renderer::create(gpuID);

    if(vtoff_flag)
      SparseResourceManager::instance().setVirtualTexturingEnabled(false);

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

    LLOG_DBG << "Exiting lava. Bye :)";
    exit(EXIT_SUCCESS);
}