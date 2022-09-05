#include <string>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <csignal>
#include <signal.h>
#include <memory>
#include <boost/thread/thread.hpp>

#include "lava_utils_lib/logging.h"

#include "window.h"

#define _HAS_CXX17 true

#include <zmq.hpp>

static const int kMinWorkersNumber = 2;
static const int kWindowWidth = 800;
static const int kWindowHeight = 600;

zmq::context_t ctx_proxy(1);
boost::thread_group gThreads;
std::atomic<bool> running = true;

static void atexitHandler()  {
	lava::ut::log::shutdown_log();
}

static void signalHandler( int signum ){
  running = false;
  gThreads.join_all();
  exit(signum);
}

void doWork(zmq::context_t& context) {
	try {
		zmq::socket_t socket(context, ZMQ_REP);
		socket.connect("inproc://workers");
		while(running) {
			
			zmq::message_t request;
			//socket.recv(&request, zmq::recv_flags::dontwait);

			std::string data((char*)request.data(), (char*)request.data() + request.size());
			LLOG_DBG << "Received: " << data << std::endl;
			
			boost::this_thread::sleep(boost::posix_time::seconds(1));
			
			zmq::message_t reply(data.length());
			memcpy(reply.data(), data.c_str(), data.length());
			socket.send(reply);
			
		}
	}
	catch(const zmq::error_t& ze)
	{
		LLOG_ERR << "Worker exception: " << ze.what();	
	}
}

void windowWorker(std::shared_ptr<Window> pWindow) {
	bool exit = false;
	SDL_Event event;
	pWindow->init();
	while(running && !exit) {
		while(pWindow->pollEvent(event)) {
			// Forward to Imgui
    	ImGui_ImplSDL2_ProcessEvent(&event);

			switch (event.type) {
				case SDL_QUIT :
					exit = true;
					break;
				case SDL_WINDOWEVENT:
					if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
						pWindow->resizeWindow(event.window.data1, event.window.data2);
					}
					break;
				case SDLK_h :
					pWindow->showHelp();
					break;
				default: 
					break;
			}
		}
		pWindow->draw();
		
		//boost::this_thread::sleep(boost::posix_time::seconds(1));
	}
	running = false;
	pWindow = nullptr;
}

int main () {
	std::shared_ptr<Window> pWindow = nullptr;

#ifdef DEBUG
	boost::log::trivial::severity_level logSeverity = boost::log::trivial::debug;
#else
	boost::log::trivial::severity_level logSeverity = boost::log::trivial::warning;
#endif
	
	// Setting up logger
  lava::ut::log::init_log();

  signal(SIGINT, signalHandler);
  std::atexit(atexitHandler);

  pWindow = std::shared_ptr<Window>(
  	new Window(0, 0, kWindowWidth, kWindowHeight, 4, GL_FLOAT, GL_RGBA, GL_RGBA32F)
  );
  
  if(!pWindow) {
  	LLOG_ERR << "Error creating main window!";
  	exit(EXIT_FAILURE);
  }

	try {
		// Launch window worker
		//gThreads.create_thread(std::bind(&windowWorker, pWindow));

		zmq::socket_t clients(ctx_proxy, ZMQ_ROUTER);
		clients.bind("tcp://*:5559");
 
 		zmq::socket_t workers(ctx_proxy, ZMQ_DEALER);
		workers.bind("inproc://workers");
		
		// Launch data recieving workers
		for(int i = 0; i < kMinWorkersNumber; i++) {
			std::cout << "Worker thread " << std::to_string(i) << "started" << std::endl;
			gThreads.create_thread(std::bind(&doWork, std::ref(ctx_proxy)));
		}

		zmq_device (ZMQ_QUEUE, &clients, &workers);
	}
	catch(const zmq::error_t& ze)
	{
		LLOG_ERR << "Exception: " << ze.what();
	}

/////////////////////////////////////////////////

	bool quit = false;
	SDL_Event event;
	pWindow->init();
	while(!quit) {
		while(pWindow->pollEvent(event)) {
			// Forward to Imgui
    	ImGui_ImplSDL2_ProcessEvent(&event);

			switch (event.type) {
				case SDL_QUIT:
					quit = true;
					break;
				case SDL_WINDOWEVENT:
					if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
						pWindow->resizeWindow(event.window.data1, event.window.data2);
					}
					break;
				case SDLK_h :
					pWindow->showHelp();
					break;
				default: 
					break;
			}
		}
		pWindow->draw();
	}
	running = false;

/////////////////////////////////////////////////

	gThreads.join_all();
	std::cout << "\nExiting Eagle. All good ;)\n";
	exit(EXIT_SUCCESS);
}