#include "litteditor.h"
#include <cassert>
#include <string>

using namespace litt::editor;

int main() {
    EditorSession editor;
    assert(editor.scene().root != nullptr);
    const uint32_t root = editor.scene().root->id;

    uint32_t node = UINT32_MAX;
    assert(editor.create_node("Player", root, &node));
    assert(editor.scene().getNode(node) != nullptr);
    assert(editor.selected_id() == node);

    assert(editor.rename_node(node, "Hero"));
    assert(editor.scene().getNode(node)->name == "Hero");
    assert(editor.undo());
    assert(editor.scene().getNode(node) != nullptr);
    assert(editor.scene().getNode(node)->name == "Player");
    assert(editor.selected_id() == node);
    assert(editor.redo());
    assert(editor.scene().getNode(node)->name == "Hero");

    assert(editor.delete_node(node));
    assert(editor.scene().getNode(node) == nullptr);
    assert(editor.undo());
    assert(editor.scene().getNode(node) != nullptr);
    assert(editor.scene().getNode(node)->name == "Hero");
    assert(editor.redo());
    assert(editor.scene().getNode(node) == nullptr);

    uint32_t parent = UINT32_MAX;
    uint32_t child = UINT32_MAX;
    assert(editor.create_node("Parent", root, &parent));
    assert(editor.create_node("Child", root, &child));
    assert(editor.reparent_node(child, parent));
    assert(editor.scene().getNode(child)->parent == editor.scene().getNode(parent));
    assert(!editor.reparent_node(parent, child));
    assert(editor.scene().getNode(parent)->parent == editor.scene().root);

    editor.clear_history();
    for (size_t i = 0; i < EditorSession::kMaxHistory + 25u; ++i) {
        assert(editor.rename_node(parent, "Parent_" + std::to_string(i)));
    }
    assert(editor.undo_count() == EditorSession::kMaxHistory);
    assert(editor.undo());
    assert(editor.redo());

    ChatSystem chat;
    assert(chat.process_command("/status", &editor));
    assert(chat.size() == 2u);
    assert(!chat.process_command("/undo", nullptr));
    const auto snapshot = chat.snapshot();
    assert(!snapshot.empty());
    assert(snapshot.back().role == ChatRole::Error);

    chat.clear();
    assert(chat.process_command("/help", &editor));
    assert(chat.size() == 2u);

    return 0;
}
