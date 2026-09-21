// Litt Engine - bounded serialization utilities
#pragma once

#include "litt_json.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace litt {

class Serializer {
public:
    virtual ~Serializer() = default;
    virtual bool serialize(const std::string& path) = 0;
    virtual bool deserialize(const std::string& path) = 0;
    virtual std::vector<uint8_t> serialize_to_buffer() = 0;
    virtual bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) = 0;
};

namespace serialization_detail {
static constexpr size_t kMaxDocumentBytes = 8u * 1024u * 1024u;
static constexpr size_t kMaxBinaryBytes = 64u * 1024u * 1024u;

inline bool read_file(const std::string& path, size_t max_bytes, std::vector<uint8_t>& out) {
    if (path.empty()) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > static_cast<uint64_t>(max_bytes)) return false;
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    return out.empty() || static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()), size));
}

inline bool write_file(const std::string& path, const uint8_t* data, size_t size) {
    if (path.empty() || (size && !data)) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    if (size) file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(file);
}

inline bool valid_json(const std::vector<uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > kMaxDocumentBytes) return false;
    std::string text(bytes.begin(), bytes.end());
    LvJson* doc = lvj_parse_strict(text.c_str());
    if (!doc) return false;
    lvj_free(doc);
    return true;
}
} // namespace serialization_detail

class JSONSerializer : public Serializer {
public:
    bool serialize(const std::string& path) override {
        const std::vector<uint8_t> bytes(json_data_.begin(), json_data_.end());
        if (!serialization_detail::valid_json(bytes)) return false;
        return serialization_detail::write_file(path, bytes.data(), bytes.size());
    }

    bool deserialize(const std::string& path) override {
        std::vector<uint8_t> bytes;
        if (!serialization_detail::read_file(path, serialization_detail::kMaxDocumentBytes, bytes)) return false;
        return deserialize_from_buffer(bytes);
    }

    std::vector<uint8_t> serialize_to_buffer() override {
        std::vector<uint8_t> bytes(json_data_.begin(), json_data_.end());
        return serialization_detail::valid_json(bytes) ? bytes : std::vector<uint8_t>{};
    }

    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override {
        if (!serialization_detail::valid_json(buffer)) return false;
        std::string next(buffer.begin(), buffer.end());
        json_data_.swap(next);
        return true;
    }

    void set_data(const std::string& json_data) { json_data_ = json_data; }
    const std::string& get_data() const { return json_data_; }

private:
    std::string json_data_;
};

class BinarySerializer : public Serializer {
public:
    bool serialize(const std::string& path) override {
        if (!good_ || buffer_.size() > serialization_detail::kMaxBinaryBytes) return false;
        return serialization_detail::write_file(path, buffer_.data(), buffer_.size());
    }

    bool deserialize(const std::string& path) override {
        std::vector<uint8_t> next;
        if (!serialization_detail::read_file(path, serialization_detail::kMaxBinaryBytes, next)) return false;
        return deserialize_from_buffer(next);
    }

    std::vector<uint8_t> serialize_to_buffer() override {
        return good_ ? buffer_ : std::vector<uint8_t>{};
    }

    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override {
        if (buffer.size() > serialization_detail::kMaxBinaryBytes) return false;
        buffer_ = buffer;
        read_pos_ = 0;
        good_ = true;
        return true;
    }

    bool write_bytes(const void* data, size_t size) {
        if ((!data && size) || size > serialization_detail::kMaxBinaryBytes ||
            buffer_.size() > serialization_detail::kMaxBinaryBytes - size) {
            good_ = false;
            return false;
        }
        const auto* p = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), p, p + size);
        return true;
    }

    bool write_string(const std::string& str) {
        if (str.size() > std::numeric_limits<uint32_t>::max()) {
            good_ = false;
            return false;
        }
        const uint32_t size = static_cast<uint32_t>(str.size());
        return write_uint(size) && write_bytes(str.data(), str.size());
    }

    bool write_float(float value) { return write_bytes(&value, sizeof(value)); }
    bool write_int(int value) { return write_bytes(&value, sizeof(value)); }
    bool write_uint(uint32_t value) { return write_bytes(&value, sizeof(value)); }
    bool write_bool(bool value) {
        const uint8_t encoded = value ? 1u : 0u;
        return write_bytes(&encoded, sizeof(encoded));
    }

    std::vector<uint8_t> read_bytes(size_t size) {
        if (!can_read(size)) return {};
        std::vector<uint8_t> out(buffer_.begin() + static_cast<std::ptrdiff_t>(read_pos_),
                                 buffer_.begin() + static_cast<std::ptrdiff_t>(read_pos_ + size));
        read_pos_ += size;
        return out;
    }

    std::string read_string() {
        const uint32_t size = read_uint();
        if (!good_ || !can_read(size)) return {};
        std::string out(reinterpret_cast<const char*>(buffer_.data() + read_pos_), size);
        read_pos_ += size;
        return out;
    }

    float read_float() { return read_scalar<float>(); }
    int read_int() { return read_scalar<int>(); }
    uint32_t read_uint() { return read_scalar<uint32_t>(); }
    bool read_bool() {
        const uint8_t value = read_scalar<uint8_t>();
        if (!good_ || value > 1u) {
            good_ = false;
            return false;
        }
        return value != 0;
    }

    void reset_read() { read_pos_ = 0; good_ = true; }
    void clear() { buffer_.clear(); read_pos_ = 0; good_ = true; }
    bool good() const { return good_; }
    size_t size() const { return buffer_.size(); }

private:
    bool can_read(size_t size) {
        if (!good_ || size > buffer_.size() || read_pos_ > buffer_.size() - size) {
            good_ = false;
            return false;
        }
        return true;
    }

    template <typename T>
    T read_scalar() {
        T value{};
        if (!can_read(sizeof(T))) return value;
        std::memcpy(&value, buffer_.data() + read_pos_, sizeof(T));
        read_pos_ += sizeof(T);
        return value;
    }

    std::vector<uint8_t> buffer_;
    size_t read_pos_ = 0;
    bool good_ = true;
};

class SceneSerializer : public Serializer {
public:
    bool serialize(const std::string& path) override {
        if (scene_data_.size() > serialization_detail::kMaxDocumentBytes) return false;
        return serialization_detail::write_file(
            path, reinterpret_cast<const uint8_t*>(scene_data_.data()), scene_data_.size());
    }

    bool deserialize(const std::string& path) override {
        std::vector<uint8_t> bytes;
        if (!serialization_detail::read_file(path, serialization_detail::kMaxDocumentBytes, bytes)) return false;
        return deserialize_from_buffer(bytes);
    }

    std::vector<uint8_t> serialize_to_buffer() override {
        if (scene_data_.size() > serialization_detail::kMaxDocumentBytes) return {};
        return std::vector<uint8_t>(scene_data_.begin(), scene_data_.end());
    }

    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override {
        if (buffer.size() > serialization_detail::kMaxDocumentBytes) return false;
        std::string next(buffer.begin(), buffer.end());
        scene_data_.swap(next);
        return true;
    }

    void set_scene_data(const std::string& data) { scene_data_ = data; }
    const std::string& get_scene_data() const { return scene_data_; }

private:
    std::string scene_data_;
};

class SerializationManager {
public:
    static constexpr size_t kMaxSerializers = 64u;

    static SerializationManager& get_instance() {
        static SerializationManager instance;
        return instance;
    }

    bool register_serializer(const std::string& name, std::unique_ptr<Serializer> serializer) {
        if (name.empty() || !serializer || serializers_.size() >= kMaxSerializers ||
            serializers_.count(name)) {
            return false;
        }
        serializers_.emplace(name, std::move(serializer));
        return true;
    }

    Serializer* get_serializer(const std::string& name) {
        const auto it = serializers_.find(name);
        return it == serializers_.end() ? nullptr : it->second.get();
    }

    bool remove_serializer(const std::string& name) {
        return serializers_.erase(name) == 1u;
    }

    bool serialize_to_file(const std::string& name, const std::string& path) {
        Serializer* serializer = get_serializer(name);
        return serializer && serializer->serialize(path);
    }

    bool deserialize_from_file(const std::string& name, const std::string& path) {
        Serializer* serializer = get_serializer(name);
        return serializer && serializer->deserialize(path);
    }

    std::vector<uint8_t> serialize_to_buffer(const std::string& name) {
        Serializer* serializer = get_serializer(name);
        return serializer ? serializer->serialize_to_buffer() : std::vector<uint8_t>{};
    }

    bool deserialize_from_buffer(const std::string& name, const std::vector<uint8_t>& buffer) {
        Serializer* serializer = get_serializer(name);
        return serializer && serializer->deserialize_from_buffer(buffer);
    }

    void clear() { serializers_.clear(); }

private:
    SerializationManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Serializer>> serializers_;
};

} // namespace litt
