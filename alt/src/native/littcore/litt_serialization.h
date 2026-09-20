// Phase 5: Production Systems - Serialization

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

namespace litt {

// Serializer base class
class Serializer {
public:
    virtual ~Serializer() = default;
    
    // Serialize to file
    virtual bool serialize(const std::string& path) = 0;
    
    // Deserialize from file
    virtual bool deserialize(const std::string& path) = 0;
    
    // Serialize to buffer
    virtual std::vector<uint8_t> serialize_to_buffer() = 0;
    
    // Deserialize from buffer
    virtual bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) = 0;
};

// JSON serializer
class JSONSerializer : public Serializer {
public:
    bool serialize(const std::string& path) override;
    bool deserialize(const std::string& path) override;
    std::vector<uint8_t> serialize_to_buffer() override;
    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override;
    
    // Set JSON data
    void set_data(const std::string& json_data) { json_data_ = json_data; }
    const std::string& get_data() const { return json_data_; }

private:
    std::string json_data_;
};

// Binary serializer
class BinarySerializer : public Serializer {
public:
    bool serialize(const std::string& path) override;
    bool deserialize(const std::string& path) override;
    std::vector<uint8_t> serialize_to_buffer() override;
    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override;
    
    // Write data
    void write_bytes(const void* data, size_t size);
    void write_string(const std::string& str);
    void write_float(float value);
    void write_int(int value);
    void write_uint(uint32_t value);
    void write_bool(bool value);
    
    // Read data
    std::vector<uint8_t> read_bytes(size_t size);
    std::string read_string();
    float read_float();
    int read_int();
    uint32_t read_uint();
    bool read_bool();
    
    // Reset read position
    void reset_read() { read_pos_ = 0; }

private:
    std::vector<uint8_t> buffer_;
    size_t read_pos_ = 0;
};

// Scene serializer
class SceneSerializer : public Serializer {
public:
    bool serialize(const std::string& path) override;
    bool deserialize(const std::string& path) override;
    std::vector<uint8_t> serialize_to_buffer() override;
    bool deserialize_from_buffer(const std::vector<uint8_t>& buffer) override;
    
    // Set scene data
    void set_scene_data(const std::string& data) { scene_data_ = data; }
    const std::string& get_scene_data() const { return scene_data_; }

private:
    std::string scene_data_;
};

// Serialization manager
class SerializationManager {
public:
    static SerializationManager& get_instance() {
        static SerializationManager instance;
        return instance;
    }
    
    // Register serializer
    void register_serializer(const std::string& name, std::unique_ptr<Serializer> serializer);
    
    // Get serializer
    Serializer* get_serializer(const std::string& name);
    
    // Serialize to file
    bool serialize_to_file(const std::string& name, const std::string& path);
    
    // Deserialize from file
    bool deserialize_from_file(const std::string& name, const std::string& path);
    
    // Serialize to buffer
    std::vector<uint8_t> serialize_to_buffer(const std::string& name);
    
    // Deserialize from buffer
    bool deserialize_from_buffer(const std::string& name, const std::vector<uint8_t>& buffer);

private:
    SerializationManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Serializer>> serializers_;
};


inline bool JSONSerializer::serialize(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(json_data_.data(), static_cast<std::streamsize>(json_data_.size()));
    return out.good();
}

inline bool JSONSerializer::deserialize(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) return false;
    json_data_ = std::move(data);
    return true;
}

inline std::vector<uint8_t> JSONSerializer::serialize_to_buffer() {
    return std::vector<uint8_t>(json_data_.begin(), json_data_.end());
}

inline bool JSONSerializer::deserialize_from_buffer(const std::vector<uint8_t>& buffer) {
    json_data_.assign(buffer.begin(), buffer.end());
    return true;
}

inline bool BinarySerializer::serialize(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!buffer_.empty()) {
        out.write(reinterpret_cast<const char*>(buffer_.data()),
                  static_cast<std::streamsize>(buffer_.size()));
    }
    return out.good();
}

inline bool BinarySerializer::deserialize(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) return false;
    buffer_ = std::move(data);
    read_pos_ = 0;
    return true;
}

inline std::vector<uint8_t> BinarySerializer::serialize_to_buffer() {
    return buffer_;
}

inline bool BinarySerializer::deserialize_from_buffer(const std::vector<uint8_t>& buffer) {
    buffer_ = buffer;
    read_pos_ = 0;
    return true;
}

inline void BinarySerializer::write_bytes(const void* data, size_t size) {
    if (size == 0) return;
    if (!data || size > std::numeric_limits<size_t>::max() - buffer_.size()) return;
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer_.insert(buffer_.end(), bytes, bytes + size);
}

inline void BinarySerializer::write_string(const std::string& str) {
    if (str.size() > std::numeric_limits<uint32_t>::max()) return;
    write_uint(static_cast<uint32_t>(str.size()));
    write_bytes(str.data(), str.size());
}

inline void BinarySerializer::write_float(float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "32-bit float required");
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_uint(bits);
}

inline void BinarySerializer::write_int(int value) {
    static_assert(sizeof(int) == sizeof(uint32_t), "32-bit int required");
    write_uint(static_cast<uint32_t>(value));
}

inline void BinarySerializer::write_uint(uint32_t value) {
    buffer_.push_back(static_cast<uint8_t>(value & 0xffu));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xffu));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xffu));
}

inline void BinarySerializer::write_bool(bool value) {
    buffer_.push_back(value ? 1u : 0u);
}

inline std::vector<uint8_t> BinarySerializer::read_bytes(size_t size) {
    if (size > buffer_.size() - std::min(read_pos_, buffer_.size())) return {};
    const size_t end = read_pos_ + size;
    std::vector<uint8_t> out(buffer_.begin() + static_cast<std::ptrdiff_t>(read_pos_),
                             buffer_.begin() + static_cast<std::ptrdiff_t>(end));
    read_pos_ = end;
    return out;
}

inline uint32_t BinarySerializer::read_uint() {
    if (read_pos_ > buffer_.size() || buffer_.size() - read_pos_ < 4) return 0;
    const uint32_t value =
        static_cast<uint32_t>(buffer_[read_pos_]) |
        (static_cast<uint32_t>(buffer_[read_pos_ + 1]) << 8) |
        (static_cast<uint32_t>(buffer_[read_pos_ + 2]) << 16) |
        (static_cast<uint32_t>(buffer_[read_pos_ + 3]) << 24);
    read_pos_ += 4;
    return value;
}

inline int BinarySerializer::read_int() {
    return static_cast<int32_t>(read_uint());
}

inline float BinarySerializer::read_float() {
    const uint32_t bits = read_uint();
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline bool BinarySerializer::read_bool() {
    if (read_pos_ >= buffer_.size()) return false;
    return buffer_[read_pos_++] != 0;
}

inline std::string BinarySerializer::read_string() {
    if (read_pos_ > buffer_.size() || buffer_.size() - read_pos_ < 4) return {};
    const size_t length_pos = read_pos_;
    const uint32_t length = read_uint();
    if (length > buffer_.size() - read_pos_) {
        read_pos_ = length_pos;
        return {};
    }
    std::string out(reinterpret_cast<const char*>(buffer_.data() + read_pos_), length);
    read_pos_ += length;
    return out;
}

inline bool SceneSerializer::serialize(const std::string& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(scene_data_.data(), static_cast<std::streamsize>(scene_data_.size()));
    return out.good();
}

inline bool SceneSerializer::deserialize(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) return false;
    scene_data_ = std::move(data);
    return true;
}

inline std::vector<uint8_t> SceneSerializer::serialize_to_buffer() {
    return std::vector<uint8_t>(scene_data_.begin(), scene_data_.end());
}

inline bool SceneSerializer::deserialize_from_buffer(const std::vector<uint8_t>& buffer) {
    scene_data_.assign(buffer.begin(), buffer.end());
    return true;
}

inline void SerializationManager::register_serializer(
    const std::string& name, std::unique_ptr<Serializer> serializer) {
    if (name.empty()) return;
    if (!serializer) {
        serializers_.erase(name);
        return;
    }
    serializers_[name] = std::move(serializer);
}

inline Serializer* SerializationManager::get_serializer(const std::string& name) {
    const auto it = serializers_.find(name);
    return it == serializers_.end() ? nullptr : it->second.get();
}

inline bool SerializationManager::serialize_to_file(
    const std::string& name, const std::string& path) {
    Serializer* serializer = get_serializer(name);
    return serializer && serializer->serialize(path);
}

inline bool SerializationManager::deserialize_from_file(
    const std::string& name, const std::string& path) {
    Serializer* serializer = get_serializer(name);
    return serializer && serializer->deserialize(path);
}

inline std::vector<uint8_t> SerializationManager::serialize_to_buffer(const std::string& name) {
    Serializer* serializer = get_serializer(name);
    return serializer ? serializer->serialize_to_buffer() : std::vector<uint8_t>{};
}

inline bool SerializationManager::deserialize_from_buffer(
    const std::string& name, const std::vector<uint8_t>& buffer) {
    Serializer* serializer = get_serializer(name);
    return serializer && serializer->deserialize_from_buffer(buffer);
}

} // namespace litt
