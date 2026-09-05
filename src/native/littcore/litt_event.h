// LittEvent - Event and messaging system for Litt Engine
// Decoupled communication between subsystems

#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <mutex>
#include <memory>
#include <algorithm>
#include <queue>
#include <string>

namespace litt {

// =============================================================================
// Event Types
// =============================================================================
enum class EventKey {
    LogEvent,
    UpdateEvent,
    RenderEvent,
    InputEvent,
    PhysicsEvent,
    CollisionEvent,
    SceneChangeEvent,
    AssetLoaded,
    AssetError
};

// =============================================================================
// Event System
// =============================================================================

// Base event interface
struct IEvent {
    virtual ~IEvent() = default;
    virtual IEvent* clone() const = 0;
};

// String event (simple event with message)
struct StringEvent : IEvent {
    std::string message;
    
    StringEvent() = default;
    explicit StringEvent(const std::string& msg) : message(msg) {}
    
    IEvent* clone() const override {
        return new StringEvent(*this);
    }
};

// Dispatcher with type-erased callbacks
class EventDispatcher {
public:
    EventDispatcher() = default;
    ~EventDispatcher() = default;

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // Subscribe to an event type
    template<typename EventType>
    void subscribe(std::function<void(const EventType&)> callback) {
        std::type_index type = typeid(EventType);
        
        // Wrap typed callback in generic wrapper
        auto generic_wrapper = [callback](const IEvent& event) {
            const EventType& typed_event = dynamic_cast<const EventType&>(event);
            callback(typed_event);
        };

        std::lock_guard<std::mutex> lock(mutex_);
        listeners_[type].push_back(generic_wrapper);
    }

    // Unsubscribe
    template<typename EventType>
    void unsubscribe(std::function<void(const EventType&)> callback) {
        std::type_index type = typeid(EventType);
        auto it = listeners_.find(type);
        if (it != listeners_.end()) {
            // Remove matching callbacks
            auto& callbacks = it->second;
            callbacks.erase(
                std::remove_if(callbacks.begin(), callbacks.end(), [&](const std::function<void(const IEvent&)>& cb) {
                    // We can't easily compare function objects, so this is a simple approach
                    return false;
                }),
                callbacks.end()
            );
        }
    }

    // Dispatch an event
    template<typename EventType>
    void dispatch(const EventType& event) {
        std::type_index type = typeid(EventType);
        auto it = listeners_.find(type);
        if (it != listeners_.end()) {
            for (auto& cb : it->second) {
                cb(event);
            }
        }
    }

    // Dispatch with string event
    void dispatch_string(const std::string& message) {
        StringEvent evt(message);
        dispatch<StringEvent>(evt);
    }

    size_t subscriber_count() const {
        size_t count = 0;
        for (const auto& pair : listeners_) {
            count += pair.second.size();
        }
        return count;
    }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const IEvent&)>>> listeners_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Event Queue - For deferred event processing
// =============================================================================
template<typename T>
class EventQueue {
public:
    void push(const T& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(event);
    }

    T pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            throw std::runtime_error("Event queue is empty");
        }
        T event = queue_.front();
        queue_.pop();
        return event;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
};

// =============================================================================
// Logging Utility
// =============================================================================
class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static void log(Level level, const std::string& message) {
        const char* prefix = "";
        switch (level) {
            case Level::DEBUG: prefix = "[DEBUG]"; break;
            case Level::INFO: prefix = "[INFO]"; break;
            case Level::WARNING: prefix = "[WARN]"; break;
            case Level::ERROR: prefix = "[ERROR]"; break;
        }
        fprintf(stdout, "%s %s\n", prefix, message.c_str());
    }

    static void debug(const std::string& message) {
        log(Level::DEBUG, message);
    }

    static void info(const std::string& message) {
        log(Level::INFO, message);
    }

    static void warning(const std::string& message) {
        log(Level::WARNING, message);
    }

    static void error(const std::string& message) {
        log(Level::ERROR, message);
    }
};

} // namespace litt
