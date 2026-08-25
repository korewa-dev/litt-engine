// Litt Editor with integrated chat
#include "litt.h"
#include <cstdio>
#include <vector>
#include <string>
#include <deque>
#include <iostream>

using namespace litt;

struct Msg {
    enum Type { System, User, Agent, Error } type;
    std::string from, content;
};

class Chat {
public:
    std::deque<Msg> history;
    
    void add(Msg::Type t, const std::string& from, const std::string& content) {
        history.push_back({t, from, content});
        if (history.size() > 200) history.pop_front();
    }
    
    void add_system(const std::string& s) { add(Msg::System, "System", s); }
    void add_user(const std::string& s) { add(Msg::User, "You", s); }
    void add_agent(const std::string& s) { add(Msg::Agent, "AI", s); }
    void add_error(const std::string& s) { add(Msg::Error, "Error", s); }
    
    void print() const {
        for (const auto& m : history) {
            printf("[%s] %s: %s\n", 
                m.type == Msg::System ? "SYS" : m.type == Msg::User ? "YOU" : m.type == Msg::Agent ? "AI" : "ERR",
                m.from.c_str(), m.content.c_str());
        }
    }
    
    void process(const std::string& cmd) {
        add_user(cmd);
        if (cmd == "/help") {
            add_system("Commands: /help /status /load <f> /save /reset /clear");
        } else if (cmd == "/status") {
            add_system("FPS: 60 | Entities: 0 | Draw: 0");
        } else if (cmd == "/clear") {
            history.clear();
            add_system("Chat cleared");
        } else if (cmd.find("/load") == 0) {
            // Guard the substring: bare "/load" made substr(6) throw
            // std::out_of_range and kill the process.
            std::string arg = cmd.length() > 6 ? cmd.substr(6) : "";
            add_system(arg.empty() ? "Usage: /load <file>" : "Loading: " + arg);
        } else if (cmd == "/reset") {
            add_system("Scene reset");
        } else {
            add_agent("Processing: " + cmd);
        }
    }
};

int main() {
    printf("Litt Editor\n");
    printf("Type commands (e.g., /help) or exit with Ctrl+C\n\n");
    
    Chat chat;
    chat.add_system("Litt Editor initialized");
    chat.add_agent("Welcome! Type /help for commands.");
    chat.print();
    
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line[0] == '/') {
            chat.process(line.substr(1));
            chat.print();
        }
    }
    
    return 0;
}
