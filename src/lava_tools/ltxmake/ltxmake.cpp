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

#ifdef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#else
#include "boost/filesystem.hpp"
namespace fs = boost::filesystem;
#endif


#include "Falcor/Utils/Image/LTX_Bitmap.h"
#include "lava_utils_lib/logging.h"

static uint8_t kMajorVersion = 1;
static uint8_t kMinorVersion = 0;


using namespace lava;

static void atexitHandler()  {
  lava::ut::log::shutdown_log();
}

int main(int argc, char** argv){
    std::atexit(atexitHandler);

    /// Program options
    std::string configFile;

    // Declare a group of options that will be allowed only on command line
    namespace po = boost::program_options; 
    po::options_description generic("Generic ptions"); 
    generic.add_options() 
        ("help,h", "Show helps") 
        ("version,v", "Shout version information")
        ("force,f", "Show helps")
        ("config-file", po::value<std::string>(&configFile), "Configuration file") 
        ;

    std::vector<std::string> inputFilenames;
    std::vector<std::string> outputFilenames;
    po::options_description input("Input");
    input.add_options()
        ("input-files,i", po::value< std::vector<std::string> >(&inputFilenames), "Input files")
        ("output-files,o", po::value< std::vector<std::string> >(&outputFilenames), "Output files")
        ;

    std::string compressorTypeName = "blosclz";
    int compressionLevel = 9;
    po::options_description tlc_compression("Container compression");
    tlc_compression.add_options()
        ("compressor,z", po::value<std::string>(&compressorTypeName)->default_value(compressorTypeName), "Compressor type")
        ("compression-level", po::value<int>(&compressionLevel)->default_value(compressionLevel), "Compression level")
        ;

    std::string logFilename = "";
#ifdef DEBUG
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::debug;
#else
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::warning;
#endif


    po::options_description logg("Logging");
    logg.add_options()
        ("log-level,l", po::value<boost::log::trivial::severity_level>(&logSeverity)->default_value(logSeverity), "Logging level")
        ("log-file", po::value<std::string>(&logFilename), "Output log to file")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(input).add(tlc_compression).add(logg);

    po::variables_map vm; 
 
    // Handle config file
    std::ifstream ifs(configFile.c_str());
    if (ifs) {
      try {
        po::store(po::parse_config_file(ifs, cmdline_options), vm);
      } catch ( po::error& e ) {
        LLOG_ERR << e.what();
      }
    }

    try {
      po::store(po::command_line_parser(argc, argv).options(cmdline_options).run(), vm); // can throw 
      po::notify(vm); // throws on error, so do after help in case there are any problems
    } catch ( po::error& e ) {
      LLOG_FTL << e.what();
      exit(EXIT_FAILURE);
    }

    // Setting up logger
    lava::ut::log::init_log();
    if(logFilename != "") lava::ut::log::init_file_log(logFilename);
    
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >= logSeverity );

    /** --help option 
     */ 
    if ( vm.count("help")  ) { 
      std::cout << generic << "\n";
      std::cout << input << "\n";
      std::cout << tlc_compression << "\n";
      std::cout << logg << "\n";
      exit(EXIT_SUCCESS);
    }

    if (vm.count("version")) {
      std::cout << "Ltxmake, version 0.0\n";
      exit(EXIT_SUCCESS);
    }

    bool forceConversion = false;
    if ( vm.count("force")  ) { 
      forceConversion = true;
    }

    if (vm.count("output-files")) {
      // Check for output filenames extension. Should always be .ltx
      for( const std::string& output_filename: outputFilenames ) {
        fs::path path(output_filename);
        if (path.extension().string() != ".ltx") {
          LLOG_FTL << "Wrong output filename: \"" << output_filename
          << "\" Should be .ltx got " << path.extension().string();
          exit(EXIT_FAILURE);
        }
      }
    }

    if (vm.count("input-files")) {
      // Check for input and output filenames count. Should match
      if ((inputFilenames.size() != outputFilenames.size()) && (outputFilenames.size() > 0)) {
        LLOG_FTL << "Wrong output filenames count !!!";
        exit(EXIT_FAILURE);
      }

      // Check if output filenames provided at all
      bool useAutoNaming = false;
      if ((outputFilenames.size() < 1) || (inputFilenames.size() != outputFilenames.size())) {
        useAutoNaming = true;
        LLOG_INF << "No output filenames provided.\n"
        << "Output files would be named automatically and placed alongside input files.";
      }

      // check for container compression level boundaries (0-9)
      if( Falcor::LTX_Bitmap::getTLCFromString(compressorTypeName) != Falcor::LTX_Header::TopLevelCompression::NONE ) {
        if(compressionLevel < 0) {
          LLOG_WRN << "Compression level set to 0 (got " << compressionLevel << ")";
          compressionLevel = 0;
        } else if (compressionLevel > 9) {
          LLOG_WRN << "Compression level set to 9 (got " << compressionLevel << ")";
          compressionLevel = 9;
        }
      }

      // Process input texture files
      size_t numFiles = inputFilenames.size();
      LLOG_INF << "Processing total " << numFiles << " textures...\n";

      for( size_t i = 0; i < inputFilenames.size(); i++) {

        const std::string& input_filename = inputFilenames[i];
        std::string output_filename;
        
        if (useAutoNaming) {
          output_filename = input_filename + ".ltx";
        } else {
          output_filename = outputFilenames[i];
        }

        bool ltxMagicMatch = false;
        if(fs::exists(input_filename) && Falcor::LTX_Bitmap::checkFileMagic(input_filename, true)) ltxMagicMatch = true;

        if(forceConversion || !fs::exists(input_filename) || !ltxMagicMatch ) {
            LLOG_INF << "Converting texture " << input_filename << " to LTX format ...";

            Falcor::LTX_Bitmap::TLCParms tlcParms;
            tlcParms.compressorName = compressorTypeName;
            tlcParms.compressionLevel = compressionLevel;
            if (!Falcor::LTX_Bitmap::convertToKtxFile(nullptr, input_filename, output_filename, tlcParms, true)) {
                LLOG_ERR << "Error converting texture " <<  input_filename << " !!!";
            } else {
                LLOG_DBG << "Conversion done for " << input_filename;
            }
        }

      }
    }

    lava::ut::log::shutdown_log();
    std::cout << "Exiting ltxmake. Bye :)\n";
    exit(EXIT_SUCCESS);
}