#ifndef SRC_LAVA_LIB_READER_LSD_ATTRIBS_CONTAINER_H_
#define SRC_LAVA_LIB_READER_LSD_ATTRIBS_CONTAINER_H_

#include <map>
#include <memory>
#include <variant>
#include <string>

#include "../scene_reader_base.h"
#include "session_lsd.h"
#include "grammar_lsd.h"


namespace lava {

namespace lsd {

class AttribsContainer;

class Attrib {
    public:
        using Type = lsd::ast::Type;
        using Value = boost::variant<bool, int, double, Vector2, Vector3, Vector4, std::string>;

    public:
        // Copy constructor 
        Attrib(const Attrib &a2): mType(a2.mType), mValue(a2.mValue) { }

        // Move constructor
        Attrib(Attrib&& a) noexcept : mType(std::move(a.mType)), mValue(std::move(a.mValue)) { }

        Attrib& operator=(Attrib& a) {
            mType = a.mType;
            mValue = a.mValue;
            return *this;
        }

        Attrib& operator=(Attrib&& a) {
            mType = std::move(a.mType);
            mValue = std::move(a.mValue);
            return *this;
        }

        static bool create(Type type, Value& value, Attrib& attrib);

        Type  type() { return mType; };
        Value value() { return mValue; };

        template<typename T>
        T get();

    private:
        Attrib(): mType(Type::UNKNOWN) {};
        Attrib(Type type, Value value) { mType = type; mValue = value; };
        
        Type mType;
        Value mValue;

    friend class AttribsContainer;
};

class AttribsContainer {
    public:
        using AttribsMap = std::map<std::string, Attrib>;

    public:
        AttribsContainer() {};
        virtual ~AttribsContainer() {};

        bool declare(Attrib::Type type, const std::string& key, Attrib::Value& value);
        bool attributeExist(const std::string& key);

    private:
        AttribsMap mAttribsMap;
};

}  // namespace lsd

}  // namespace lava

#endif  // SRC_LAVA_LIB_READER_LSD_ATTRIBS_H_
