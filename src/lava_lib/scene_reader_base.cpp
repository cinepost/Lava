#include "scene_reader_base.h"


namespace lava {

ReaderBase::ReaderBase() {

}

ReaderBase::~ReaderBase() {

}

bool ReaderBase::read(SharedPtr iface, std::istream &input, bool echo) {
	//if (!infile) {
	//	std::cerr << "Unable to read scene file: " << filename << std::endl;
//		return;
//	}

	std::string line;
	while (std::getline(input, line))	{
    	parseLine(line, echo);
    }

    return true;
}

}  // namespace lava