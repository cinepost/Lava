#ifndef SRC_LAVA_LIB_LOADER_BASE_H_
#define SRC_LAVA_LIB_LOADER_BASE_H_

#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace lava {

class LoaderBase: public std::enable_shared_from_this<LoaderBase> {
 public:
 	using SharedPtr = std::shared_ptr<LoaderBase>;

 	LoaderBase() {};
 	void read(const std::string& filename, bool echo=false); // read from file
 	void read(bool echo=false); // read from stdin

 private:
 	virtual void parseLine(const std::string& line) = 0;

};

}  // namespace lava

#endif  // SRC_LAVA_LIB_LOADER_BASE_H_
