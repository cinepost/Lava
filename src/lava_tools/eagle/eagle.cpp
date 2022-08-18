#include <zmq.hpp>
#include <string>
#include <iostream>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <boost/thread/thread.hpp>

#include "lava_utils_lib/logging.h"

static const int kMinWorkersNumber = 2;

zmq::context_t ctx_proxy(1);

void doWork(zmq::context_t& context) {
	try {
		zmq::socket_t socket(context, ZMQ_REP);
		socket.connect("inproc://workers");
		while(true) {
			zmq::message_t request;
			socket.recv(&request);

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

int main () {

#ifdef DEBUG
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::debug;
#else
    boost::log::trivial::severity_level logSeverity = boost::log::trivial::warning;
#endif
	
	// Setting up logger
  lava::ut::log::init_log();

	boost::thread_group threads;

	try {
		//zmq::context_t context(1);
		zmq::socket_t clients(ctx_proxy, ZMQ_ROUTER);
		clients.bind("tcp://*:5559");
 
		zmq::socket_t workers(ctx_proxy, ZMQ_DEALER);
		workers.bind("inproc://workers");
		//zmq_bind(workers, "inproc://workers");
 
		for(int i = 0; i < kMinWorkersNumber; i++)
			threads.create_thread(std::bind(&doWork, std::ref(ctx_proxy)));
 
		// /zmq_device (ZMQ_QUEUE, &clients, &workers);
	}
	catch(const zmq::error_t& ze)
	{
		LLOG_ERR << "Exception: " << ze.what();
	}
	threads.join_all(); // 6

	exit(EXIT_SUCCESS);
}