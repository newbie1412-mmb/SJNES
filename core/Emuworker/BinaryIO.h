#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <type_traits>

class BinaryWriter {
public:
    std::vector<uint8_t> buffer;

    template<typename T>
    BinaryWriter& operator<<(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be POD to serialize");
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), p, p + sizeof(T));
        return *this;
    }

    void writeRaw(const void* data, size_t size) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), p, p + size);
    }
};

class BinaryReader {
public:
    const uint8_t* data;
    size_t size;
    size_t offset = 0;
    bool ok = true;

    BinaryReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    template<typename T>
    BinaryReader& operator>>(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be POD to deserialize");
        if (offset + sizeof(T) > size) { ok = false; return *this; }
        std::memcpy(&value, data + offset, sizeof(T));
        offset += sizeof(T);
        return *this;
    }

    bool readRaw(void* out, size_t len) {
        if (offset + len > size) { ok = false; return false; }
        std::memcpy(out, data + offset, len);
        offset += len;
        return true;
    }
};