#include <memory>
#include <array>

#include <dlfcn.h>
#include <stdlib.h>
#include <boost/format.hpp>

#include "display.h"
#include "lava_utils_lib/logging.h"

namespace lava {

const Display::ChannelNamingDesc kChannelNameDesc[] = {
	// NamingScheme                             ChannelNames,
	{ Display::NamingScheme::RGBA,                        {"r", "g", "b", "a"}},
	{ Display::NamingScheme::XYZW,                        {"x", "y", "z", "w"}},
};

const std::string& Display::makeChannelName(Display::NamingScheme namingScheme, uint32_t channelIndex) {
	assert(kChannelNameDesc[(uint32_t)namingScheme].namingScheme == namingScheme);
	return kChannelNameDesc[(uint32_t)namingScheme].channelNames[channelIndex];
}

std::string Display::makeImageHashString(const std::string& image_name, uint width, uint height, const std::vector<Channel>& channels, uint &imageHandle, 
      const std::vector<UserParameter>& userParams, const MetaData* pMetaData) {

    std::string hashString = image_name;
    hashString += ":" + std::to_string(width) + ": " + std::to_string(height);

    for(const auto& channel: channels) hashString += ":" + channel.name + "_" + to_string(channel.format);    

    return hashString;
}

bool Display::setStringParameter(const std::string& name, const std::vector<std::string>& strings) {
	return false;
}

bool Display::setIntParameter(const std::string& name, const std::vector<int>& ints) {
  return false;
}

bool Display::setFloatParameter(const std::string& name, const std::vector<float>& floats) {
	return false;
}

/* static */
UserParameter Display::makeStringsParameter(const std::string& name, const std::vector<std::string>& strings) {
	UserParameter parameter;
	// Allocate and fill in the name.
	char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
	strcpy(pname, name.c_str());
	parameter.name = pname;

	// Allocate enough space for the string pointers, and the strings, in one big block,
	// makes it easy to deallocate later.
	int count = strings.size();
	int totallen = count * sizeof(char*);

	for ( uint i = 0; i < count; i++ ) totallen += (strings[i].size()+1) * sizeof(char);

	char** pstringptrs = reinterpret_cast<char**>(malloc(totallen));
	char* pstrings = reinterpret_cast<char*>(&pstringptrs[count]);

	for ( uint i = 0; i < count; i++ ) {
    // Copy each string to the end of the block.
    strcpy(pstrings, strings[i].c_str());
    pstringptrs[i] = pstrings;
    pstrings += strings[i].size()+1;
	}

	parameter.value = reinterpret_cast<RtPointer>(pstringptrs);
	parameter.vtype = 's';
	parameter.vcount = count;
	parameter.nbytes = totallen;

	return parameter;
}

UserParameter Display::makeIntsParameter(const std::string& name, const std::vector<int>& ints) {
  UserParameter parameter;
  // Allocate and fill in the name.
  char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
  strcpy(pname, name.c_str());
  parameter.name = pname;
  

  // Allocate an ints array.
  uint32_t count = ints.size();
  uint32_t totallen = count * sizeof(int);
  int* pints = reinterpret_cast<int*>(malloc(totallen));
  // Then just copy the whole lot in one go.
  memcpy(pints, ints.data(), totallen);
  parameter.value = reinterpret_cast<RtPointer>(pints);
  parameter.vtype = 'i';
  parameter.vcount = count;
  parameter.nbytes = totallen;

  return parameter;
}

UserParameter Display::makeFloatsParameter(const std::string& name, const std::vector<float>& floats) {
  UserParameter parameter;
  // Allocate and fill in the name.
  char* pname = reinterpret_cast<char*>(malloc(name.size()+1));
  strcpy(pname, name.c_str());
  parameter.name = pname;
  

  // Allocate an ints array.
  uint32_t count = floats.size();
  uint32_t totallen = count * sizeof(float);
  float* pfloats = reinterpret_cast<float*>(malloc(totallen));
  // Then just copy the whole lot in one go.
  memcpy(pfloats, floats.data(), totallen);
  parameter.value = reinterpret_cast<RtPointer>(pfloats);
  parameter.vtype = 'f';
  parameter.vcount = count;
  parameter.nbytes = totallen;

  return parameter;
}

void Display::makeStringsParameter(const std::string& name, const std::vector<std::string>& strings, UserParameter& parameter) {
	parameter = makeStringsParameter(name, strings);
}

void Display::makeIntsParameter(const std::string& name, const std::vector<int>& ints, UserParameter& parameter) {
	parameter = makeIntsParameter(name, ints);
}

void Display::makeFloatsParameter(const std::string& name, const std::vector<float>& floats, UserParameter& parameter) {
	parameter = makeFloatsParameter(name, floats);
}

}  // namespace lava