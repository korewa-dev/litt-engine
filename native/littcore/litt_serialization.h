// Phase 5: Production Systems - Serialization

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

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

} // namespace litt
