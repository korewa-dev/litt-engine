// Phase 5: Production Systems - Networking

#pragma once

#include "litt_math.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace litt {

// Network message
struct NetworkMessage {
    uint32_t id;
    uint32_t type;
    std::vector<uint8_t> data;
    double timestamp;
};

// Network peer
class NetworkPeer {
public:
    NetworkPeer() = default;
    virtual ~NetworkPeer() = default;
    
    // Send message
    virtual bool send(const NetworkMessage& message) = 0;
    
    // Receive message
    virtual bool receive(NetworkMessage& message) = 0;
    
    // Check if connected
    virtual bool is_connected() const = 0;
    
    // Get peer ID
    uint32_t get_id() const { return id_; }
    
    // Set peer ID
    void set_id(uint32_t id) { id_ = id; }

protected:
    uint32_t id_ = 0;
};

// Network server
class NetworkServer {
public:
    static NetworkServer& get_instance() {
        static NetworkServer instance;
        return instance;
    }
    
    // Start server
    bool start(uint16_t port);
    
    // Stop server
    void stop();
    
    // Send to client
    bool send_to(uint32_t client_id, const NetworkMessage& message);
    
    // Broadcast to all clients
    bool broadcast(const NetworkMessage& message);
    
    // Get client count
    size_t get_client_count() const { return clients_.size(); }
    
    // Register message handler
    void register_handler(uint32_t message_type, std::function<void(const NetworkMessage&)> handler);
    
    // Process messages
    void process_messages();

private:
    NetworkServer() = default;
    std::unordered_map<uint32_t, std::unique_ptr<NetworkPeer>> clients_;
    std::unordered_map<uint32_t, std::function<void(const NetworkMessage&)>> handlers_;
    bool running_ = false;
    uint16_t port_ = 0;
};

// Network client
class NetworkClient {
public:
    static NetworkClient& get_instance() {
        static NetworkClient instance;
        return instance;
    }
    
    // Connect to server
    bool connect(const std::string& address, uint16_t port);
    
    // Disconnect
    void disconnect();
    
    // Send message
    bool send(const NetworkMessage& message);
    
    // Receive message
    bool receive(NetworkMessage& message);
    
    // Check if connected
    bool is_connected() const { return connected_; }
    
    // Register message handler
    void register_handler(uint32_t message_type, std::function<void(const NetworkMessage&)> handler);
    
    // Process messages
    void process_messages();

private:
    NetworkClient() = default;
    std::unique_ptr<NetworkPeer> server_;
    std::unordered_map<uint32_t, std::function<void(const NetworkMessage&)>> handlers_;
    bool connected_ = false;
};

// Network manager
class NetworkManager {
public:
    static NetworkManager& get_instance() {
        static NetworkManager instance;
        return instance;
    }
    
    // Initialize networking
    bool initialize();
    
    // Shutdown networking
    void shutdown();
    
    // Create server
    NetworkServer* create_server();
    
    // Create client
    NetworkClient* create_client();
    
    // Get server
    NetworkServer* get_server() { return server_.get(); }
    
    // Get client
    NetworkClient* get_client() { return client_.get(); }

private:
    NetworkManager() = default;
    std::unique_ptr<NetworkServer> server_;
    std::unique_ptr<NetworkClient> client_;
};

} // namespace litt
