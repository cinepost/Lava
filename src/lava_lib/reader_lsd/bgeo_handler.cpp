#include "bgeo_handler.h"

#include "lava_utils_lib/logging.h"

namespace lava {

namespace bgeo {

using namespace rapidjson;


bool BgeoAsciiHandler::StartArray() {

    return true;
}

bool BgeoAsciiHandler::EndArray(SizeType elementCount) {

    return true;
}

bool BgeoAsciiHandler::Int(int i) {

    return true;
}

bool BgeoAsciiHandler::Uint(unsigned i) {

    return true;
}

bool BgeoAsciiHandler::Int64(int64_t i) {

    return true;
}

bool BgeoAsciiHandler::Uint64(uint64_t i) {

    return true;
}

bool BgeoAsciiHandler::Double(double d) {

    return true;
}

bool BgeoAsciiHandler::String(const Ch* str, SizeType length, bool copy) {

    return true;
}

bool BgeoAsciiHandler::StartObject() {

    return true;
}

bool BgeoAsciiHandler::Key(const Ch* str, SizeType length, bool copy) {

    return true;
}

bool BgeoAsciiHandler::EndObject(SizeType memberCount) {

    return true;
}

bool BgeoAsciiHandler::Bool(bool b) {

    return true;
}

bool BgeoAsciiHandler::Null() { 

    return true; 
};


}  // namespace ngeo

}  // namespace lava
