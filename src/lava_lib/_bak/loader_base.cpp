#include <vector>
#include <fstream>

#include "loader_base.h"


namespace lava {

void LoaderBase::read(const std::string& filename, bool echo) {
	std::ifstream infile(filename);
	if (!infile) {
		std::cerr << "Unable to read scene file: " << filename << std::endl;
		return;
	}

	std::string line;
	while (std::getline(infile, line))	{
    	if (echo) std::cout << line << std::endl;
    	parseLine(line);
    }
}

void LoaderBase::read(bool echo) {
	std::string line;

	while(std::getline(std::cin, line)) {
		if (echo) std::cout << line << std::endl;
    	parseLine(line);
    }
}

}  // namespace lava