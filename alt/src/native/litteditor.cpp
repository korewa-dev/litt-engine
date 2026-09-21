// Litt Editor CLI - portable front end for the tested editor core
#include "litteditor.h"
#include <cstdio>
#include <iostream>
#include <string>

using namespace litt::editor;

static const char* role_name(ChatRole role) {
    switch (role) {
        case ChatRole::System: return "SYS";
        case ChatRole::User: return "YOU";
        case ChatRole::Assistant: return "ASSIST";
        case ChatRole::Error: return "ERR";
        case ChatRole::Agent: return "AGENT";
    }
    return "?";
}

static void print_new(const ChatSystem& chat, size_t& cursor) {
    const auto messages = chat.snapshot();
    while (cursor < messages.size()) {
        const auto& message = messages[cursor++];
        std::printf("[%s] %s: %s\n", role_name(message.role),
                    message.author.c_str(), message.content.c_str());
    }
}

int main() {
    EditorSession session;
    ChatSystem chat;
    chat.add_system("Litt Editor core initialized");
    chat.add_agent("Type /help for commands. Ctrl+D/Ctrl+Z exits.");

    std::printf("Litt Editor CLI\n");
    size_t cursor = 0;
    print_new(chat, cursor);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line[0] == '/') {
            chat.process_command(line, &session);
        } else {
            chat.add_user(line);
            chat.add_agent("Editor core received text input");
        }
        print_new(chat, cursor);
    }
    return 0;
}
