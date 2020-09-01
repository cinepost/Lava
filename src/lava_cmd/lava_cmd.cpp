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

#include "lava_lib/renderer.h"
#include "lava_lib/loaders/loader_lsd.h"

#include "lava_utils_lib/logging.h"

using namespace lava;

void printUsage() {
    std::cout << "Usage: lava [options] [ifd] [outputimage]\n" << std::endl;

    std::cout << "Reads an scene from standard input and renders the image described.\n" << std::endl;

    std::cout << "If the first argument after options ends in .ifd, .rib or .lvs" << std::endl;
    std::cout << "will read the scene description from that file.  If the argument does not" << std::endl;
    std::cout << "have an extension it will be used as the output image/device\n" << std::endl;

    std::cout << "    Control Options:" << std::endl;
    std::cout << "        -e echo stdin" << std::endl;
    std::cout << "        -r file  Read render graph definition from file" << std::endl;
    std::cout << "        -f file  Read scene file specified instead of reading from stdin" << std::endl;
    std::cout << "        -v val  Set verbose level (i.e. -v 2)" << std::endl;
    std::cout << "                    0-6  Output varying degrees of rendering statistics" << std::endl;
    std::cout << "        -l logfile  Write log to file" << std::endl;
    std::cout << "        -c      Enable stdin compatibility mode." << std::endl;
}

void signalHandler( int signum ){
    LOG_DBG << "Interrupt signal (" << signum << ") received !";

    // cleanup and close up stuff here
    // terminate program
    exit(signum);
}

int main(int argc, char* argv[]){
    // Set up logging level quick 
    lava::ut::log::init_log();
    boost::log::core::get()->set_filter(  boost::log::trivial::severity >=  boost::log::trivial::debug );
    
    int option = 0;
    int verbose_level = 6;
    bool echo = false;
    bool read_stdin = true;
    bool run_interactive = true;
    std::string filename;

    //std::istream *in; // input stream pointer
    //std::ifstream ifn; // input file

    signal(SIGTERM, signalHandler);
    signal(SIGABRT, signalHandler);

    /// Specifying the expected rendering options
    const struct option long_opts[] = {
        {"listgpus", no_argument, nullptr, 'l'},
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "hciev:f:l:e:", long_opts, nullptr);

        if (opt == -1)
            break;


        switch (opt) {
            case 'c' : 
                read_stdin = true;
                break;
            case 'v' : 
                verbose_level = atoi(optarg);
                break;
            case 'e' :
                echo = true;
                break;
            case 'f' : 
                read_stdin = false;
                filename = optarg;
                break; 
            case 'l':  
                //Renderer::listGPUs();
                exit(EXIT_SUCCESS);
                break; 
            case 'h' :
            default:
                printUsage(); 
                exit(EXIT_FAILURE);
        }
    }

    std::cout << "filename: " << filename << std::endl;

    Renderer::UniquePtr renderer = Renderer::create();
    //renderer->init(800, 600, 16);

    LoaderLSD loader;

    if (read_stdin) {
        // read from stdin
        loader.read(echo);
    } else {
        // read from file
        loader.read(filename, echo);
    }
    //FstIfd ifd_reader(in, renderer);
    //if(!ifd_reader.process()){
    //    std::cerr << "Abort. Error processing IFD !!!" << std::endl;
    //    delete renderer;
    //    exit(EXIT_FAILURE);
    //}

    exit(EXIT_SUCCESS);
}