#include "scene_reader_base.h"


namespace lava {

ReaderBase::ReaderBase() {

}

ReaderBase::~ReaderBase() {

}

bool ReaderBase::read(SharedPtr iface, std::istream &input, bool echo) {

	bool parser_ok = true;
	std::string line_buff;

	std::string line;
	while (std::getline(input, line) && parser_ok)	{
    	line_buff += line;
    	std::string unparsed;
    	parser_ok = parseLine(line_buff, echo, unparsed);
    	line_buff = std::move(unparsed);
    }

    return parser_ok;
}

}  // namespace lava