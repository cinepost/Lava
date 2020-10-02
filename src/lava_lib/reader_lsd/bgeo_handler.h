#ifndef SRC_LAVA_LIB_READER_LSD_BGEO_HANDLER_H_
#define SRC_LAVA_LIB_READER_LSD_BGEO_HANDLER_H_

#include <vector>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <variant>
#include <map>

#include "rapidjson/reader.h"
#include "rapidjson/memorystream.h"

#include "bgeo.h"

namespace lava {

namespace bgeo {

using namespace rapidjson;

/*
struct Value;
struct Object;
struct Array;

struct Object: std::map<std::string, Value> {};
struct Array: std::vector<Value> {};
struct Value: std::variant<bool, int, unsigned, int64_t, uint64_t, double, std::string, Object, Array> {};

class Bgeo: Array {

};
*/

enum class Keyword { FILEVERSION, HASINDEX, POINTCOUNT, VERTEXCOUNT, PRIMITIVECOUNT, TOPOLOGY, POINTREF, INDICES, ATTRIBUTES, VERTEXATTRIBUTES, 
    SCOPE, TYPE, NAME, OPTIONS, VALUE, SIZE, STORAGE, DEFAULTS, VALUES, TUPLES, POINTATTRIBUTES, STARTVERTEX, NPRIMITIVES, NVERTICES_RLE
};

class BgeoAsciiHandler: public rapidjson::BaseReaderHandler<UTF8<>, BgeoAsciiHandler>{
 public:
    using Ch = MemoryStream::Ch;

    bool Null();
    bool Bool(bool b);
    bool Int(int i);
    bool Uint(unsigned i);
    bool Int64(int64_t i);
    bool Uint64(uint64_t i);
    bool Double(double d);
    bool String(const Ch* str, SizeType length, bool copy);
    bool StartObject();
    bool Key(const Ch* str, SizeType length, bool copy);
    bool EndObject(SizeType memberCount);
    bool StartArray();
    bool EndArray(SizeType elementCount);

 private:
    Bgeo bgeo;
};


}  // namespace bgeo

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_BGEO_HANDLER_H_