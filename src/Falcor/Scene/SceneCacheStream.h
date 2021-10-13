#ifndef SRC_FALCOR_SCENE_SCENE_CACHE_STREAM_H_
#define SRC_FALCOR_SCENE_SCENE_CACHE_STREAM_H_

#include "SceneCache.h"

namespace Falcor {

/** Wrapper around std::ostream to ease serialization of basic types.
*/
class SceneCache::OutputStream
{
public:
    OutputStream(std::ostream& stream) : mStream(stream) {}

    void write(const void* data, size_t len)
    {
        mStream.write(reinterpret_cast<const char*>(data), len);
    }

    template<typename T>
    void write(const T& value)
    {
        write(&value, sizeof(T));
    }

    template<typename T>
    void write(const std::vector<T>& vec)
    {
        uint64_t len = vec.size();
        write(len);
        if constexpr (std::is_trivial<T>::value && !std::is_same<T, bool>::value)
        {
            write(vec.data(), len * sizeof(T));
        }
        else
        {
            for (const auto& item : vec) write<T>(item);
        }
    }

    template<typename T>
    void write(const std::optional<T>& opt)
    {
        bool hasValue = opt.has_value();
        write(hasValue);
        if (hasValue) write(opt.value());
    }

private:
    std::ostream& mStream;
};

template<>
void SceneCache::OutputStream::write(const std::string& value) {
    uint64_t len = value.size();
    write(len);
    write(value.data(), len);
}

/** Wrapper around std::istream to ease serialization of basic types.
*/
class SceneCache::InputStream
{
public:
    InputStream(std::istream& stream) : mStream(stream) {}

    void read(void* data, size_t len)
    {
        mStream.read(reinterpret_cast<char*>(data), len);
    }

    template<typename T>
    void read(T& value)
    {
        read(&value, sizeof(T));
    }

    template<typename T>
    T read()
    {
        T value;
        read(value);
        return value;
    }

    template<typename T>
    void read(std::vector<T>& vec);

    template<typename T>
    void read(std::optional<T>& opt);

private:
    std::istream& mStream;
};

template<>
void SceneCache::InputStream::read(std::string& value)
{
    uint64_t len = read<uint64_t>();
    value.resize(len);
    read(value.data(), len);
}

template<typename T>
void SceneCache::InputStream::read(std::vector<T>& vec)
{
    uint64_t len = read<uint64_t>();
    vec.resize(len);
    if constexpr (std::is_trivial<T>::value && !std::is_same<T, bool>::value)
    {
        read(vec.data(), len * sizeof(T));
    }
    else
    {
        for (auto& item : vec) read<T>(item);
    }
}

template<typename T>
void SceneCache::InputStream::read(std::optional<T>& opt)
{
    bool hasValue = read<bool>();
    if (hasValue) opt = read<T>();
}

} // namespace Falcor

#endif  // SRC_FALCOR_SCENE_SCENE_CACHE_STREAM_H_