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
#include <cstdint>
#include <cstdio>
#include <stdexcept>

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
    using SubscriptionId = uint64_t;

    EventDispatcher() = default;
    ~EventDispatcher() = default;

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    template<typename EventType>
    SubscriptionId subscribe(std::function<void(const EventType&)> callback) {
        if (!callback) return 0;

        auto generic_wrapper = [callback = std::move(callback)](const IEvent& event) {
            const EventType* typed_event = dynamic_cast<const EventType*>(&event);
            if (typed_event) callback(*typed_event);
        };

        std::lock_guard<std::mutex> lock(mutex_);
        const SubscriptionId id = next_subscription_id_++;
        listeners_[std::type_index(typeid(EventType))].push_back(
            Listener{id, std::move(generic_wrapper)});
        return id;
    }

    template<typename EventType>
    bool unsubscribe(SubscriptionId id) {
        if (id == 0) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        const std::type_index type = typeid(EventType);
        auto it = listeners_.find(type);
        if (it == listeners_.end()) return false;

        auto& callbacks = it->second;
        const auto old_size = callbacks.size();
        callbacks.erase(
            std::remove_if(callbacks.begin(), callbacks.end(),
                           [id](const Listener& listener) {
                               return listener.id == id;
                           }),
            callbacks.end());

        if (callbacks.empty()) listeners_.erase(it);
        return callbacks.size() != old_size;
    }

    template<typename EventType>
    size_t unsubscribe_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = listeners_.find(std::type_index(typeid(EventType)));
        if (it == listeners_.end()) return 0;
        const size_t removed = it->second.size();
        listeners_.erase(it);
        return removed;
    }

    template<typename EventType>
    void dispatch(const EventType& event) {
        std::vector<std::function<void(const IEvent&)>> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = listeners_.find(std::type_index(typeid(EventType)));
            if (it == listeners_.end()) return;
            callbacks.reserve(it->second.size());
            for (const auto& listener : it->second) {
                callbacks.push_back(listener.callback);
            }
        }

        for (auto& callback : callbacks) {
            callback(event);
        }
    }

    void dispatch_string(const std::string& message) {
        StringEvent evt(message);
        dispatch<StringEvent>(evt);
    }

    size_t subscriber_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& pair : listeners_) {
            count += pair.second.size();
        }
        return count;
    }

private:
    struct Listener {
        SubscriptionId id;
        std::function<void(const IEvent&)> callback;
    };

    std::unordered_map<std::type_index, std::vector<Listener>> listeners_;
    mutable std::mutex mutex_;
    SubscriptionId next_subscription_id_ = 1;
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
