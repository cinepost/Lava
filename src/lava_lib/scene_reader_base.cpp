#include <chrono>

#include "scene_reader_base.h"
#include "lava_utils_lib/logging.h"

namespace lava {

ReaderBase::ReaderBase(): mEcho(false), mIsInitialized(false) { }

ReaderBase::~ReaderBase() { }

void ReaderBase::init(std::unique_ptr<RendererIfaceBase> pIntreface, bool echo) {
    mEcho = echo;
    mpInterface = std::move(pIntreface); 
    mIsInitialized = true;
}

bool ReaderBase::readStream(std::istream &input) {
    bool result;
    if(!isInitialized())
        return false;

    auto t1 = std::chrono::high_resolution_clock::now();
    result = parseStream(input);
    auto t2 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>( t2 - t1 ).count();

    LLOG_DBG << "Scene parsed in: " << duration << " sec.";

    return result;
}

bool ReaderBase::read(std::istream &input) {
    if(!isInitialized())
        return false;

	bool parser_ok = true;
	std::string line_buff;

	std::string line;
	while (std::getline(input, line) && parser_ok)	{
    	line_buff += line;
    	std::string unparsed;
    	parser_ok = parseLine(line_buff, unparsed);
    	line_buff = std::move(unparsed);
    }

    return parser_ok;
}

}  // namespace lava