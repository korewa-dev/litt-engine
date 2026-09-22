// Litt Editor Core - portable, headless editor state and command history
#pragma once

#include "littcore/litt_scene.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace litt { namespace editor {

enum class ChatRole { System, User, Assistant, Error, Agent };

struct ChatMessage {
    ChatRole role = ChatRole::System;
    std::string content;
    std::string author;
};

class EditorSession {
public:
    static constexpr size_t kMaxHistory = 100u;

    Scene& scene() { return scene_; }
    const Scene& scene() const { return scene_; }

    uint32_t selected_id() const { return selected_id_; }
    SceneNode* selected_node() { return scene_.getNode(selected_id_); }

    bool select(uint32_t id) {
        if (!scene_.getNode(id)) return false;
        selected_id_ = id;
        return true;
    }

    bool create_node(const std::string& name, uint32_t parent_id, uint32_t* out_id = nullptr) {
        if (name.empty() || !scene_.getNode(parent_id)) return false;
        uint32_t created_id = UINT32_MAX;
        const bool ok = transact([&] {
            SceneNode& node = scene_.createNode(name);
            created_id = node.id;
            if (!scene_.setParent(node.id, parent_id)) {
                scene_.removeNode(node.id);
                return false;
            }
            selected_id_ = node.id;
            return true;
        });
        if (ok && out_id) *out_id = created_id;
        return ok;
    }

    bool rename_node(uint32_t id, const std::string& name) {
        if (name.empty() || name.size() > Scene::MAX_NODE_NAME_BYTES) return false;
        return transact([&] {
            SceneNode* node = scene_.getNode(id);
            if (!node) return false;
            node->name = name;
            return true;
        });
    }

    bool reparent_node(uint32_t id, uint32_t parent_id) {
        return transact([&] { return scene_.setParent(id, parent_id); });
    }

    bool delete_node(uint32_t id) {
        if (!scene_.getNode(id) || (scene_.root && scene_.root->id == id)) return false;
        return transact([&] {
            scene_.removeNode(id);
            if (selected_id_ == id) selected_id_ = scene_.root ? scene_.root->id : UINT32_MAX;
            return scene_.getNode(id) == nullptr;
        });
    }

    bool undo() {
        if (undo_.empty()) return false;
        Snapshot edit = std::move(undo_.back());
        undo_.pop_back();
        if (!restore(edit.before, edit.before_selected)) return false;
        redo_.push_back(std::move(edit));
        trim(redo_);
        return true;
    }

    bool redo() {
        if (redo_.empty()) return false;
        Snapshot edit = std::move(redo_.back());
        redo_.pop_back();
        if (!restore(edit.after, edit.after_selected)) return false;
        undo_.push_back(std::move(edit));
        trim(undo_);
        return true;
    }

    size_t undo_count() const { return undo_.size(); }
    size_t redo_count() const { return redo_.size(); }

    void clear_history() {
        undo_.clear();
        redo_.clear();
    }

private:
    struct Snapshot {
        std::string before;
        std::string after;
        uint32_t before_selected = UINT32_MAX;
        uint32_t after_selected = UINT32_MAX;
    };

    template <typename Fn>
    bool transact(Fn&& fn) {
        const std::string before = scene_.serializeToJson();
        const uint32_t before_selected = selected_id_;
        if (before.empty() || !fn()) return false;
        const std::string after = scene_.serializeToJson();
        if (after.empty()) {
            (void)scene_.deserializeFromJson(before);
            selected_id_ = before_selected;
            return false;
        }
        Snapshot edit{before, after, before_selected, selected_id_};
        undo_.push_back(std::move(edit));
        trim(undo_);
        redo_.clear();
        return true;
    }

    bool restore(const std::string& json, uint32_t selected) {
        if (!scene_.deserializeFromJson(json)) return false;
        if (scene_.getNode(selected)) selected_id_ = selected;
        else selected_id_ = scene_.root ? scene_.root->id : UINT32_MAX;
        return true;
    }

    static void trim(std::deque<Snapshot>& history) {
        while (history.size() > kMaxHistory) history.pop_front();
    }

    Scene scene_;
    uint32_t selected_id_ = 0u;
    std::deque<Snapshot> undo_;
    std::deque<Snapshot> redo_;
};

class ChatSystem {
public:
    static constexpr size_t kMaxMessages = 200u;

    void add(ChatRole role, const std::string& author, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back({role, content, author});
        if (history_.size() > kMaxMessages) history_.erase(history_.begin());
    }

    void add_system(const std::string& content) { add(ChatRole::System, "System", content); }
    void add_user(const std::string& content) { add(ChatRole::User, "User", content); }
    void add_agent(const std::string& content) { add(ChatRole::Agent, "LittAgent", content); }
    void add_error(const std::string& content) { add(ChatRole::Error, "Error", content); }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.clear();
    }

    std::vector<ChatMessage> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_.size();
    }

    bool process_command(const std::string& command, EditorSession* session = nullptr) {
        if (command.empty()) return false;
        add_user(command);

        if (command == "/help") {
            add_system("Commands: /help /status /clear /undo /redo");
            return true;
        }
        if (command == "/clear") {
            clear();
            add_system("Chat cleared");
            return true;
        }
        if (command == "/status") {
            const size_t nodes = session ? session->scene().nodes.size() : 0u;
            add_system("Editor ready | nodes=" + std::to_string(nodes));
            return true;
        }
        if (command == "/undo") {
            const bool ok = session && session->undo();
            (ok ? add_system("Undo complete") : add_error("Nothing to undo"));
            return ok;
        }
        if (command == "/redo") {
            const bool ok = session && session->redo();
            (ok ? add_system("Redo complete") : add_error("Nothing to redo"));
            return ok;
        }

        add_agent("No built-in editor command matched: " + command);
        return false;
    }

private:
    mutable std::mutex mutex_;
    std::vector<ChatMessage> history_;
};

} } // namespace litt::editor
