// LittEditor - Unity/Godot-like editor for Litt Engine
// Pure C++17, Vulkan-based rendering, integrated chat system

#pragma once
#include <windows.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <deque>
#include <chrono>
#include <mutex>
#include <thread>

#include "littcore/litt_math.h"
#include "littcore/litt_ecs.h"
#include "littcore/litt_scene.h"
#include "littcore/litt_physics.h"
#include "littcore/litt_input.h"
#include "littcore/litt_profiler.h"
#include "littcore/litt_world.h"

namespace litt {
namespace editor {

// =============================================================================
// Chat System (Harness-like)
// =============================================================================
enum class ChatRole {
    System,
    User,
    Assistant,
    Error,
    Agent
};

struct ChatMessage {
    ChatRole role;
    std::string content;
    std::string author;
    uint64_t timestamp;
    
    std::string GetTimeString() const {
        auto t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::from_time_t(timestamp / 1000));
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        return std::string(buf);
    }
};

class ChatSystem {
public:
    std::vector<ChatMessage> history;
    std::mutex mutex;
    
    void AddMessage(ChatRole role, const std::string& author, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex);
        ChatMessage msg;
        msg.role = role;
        msg.author = author;
        msg.content = content;
        msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        history.push_back(msg);
    }
    
    void AddSystem(const std::string& content) {
        AddMessage(ChatRole::System, "System", content);
    }
    
    void AddUser(const std::string& content) {
        AddMessage(ChatRole::User, "User", content);
    }
    
    void AddAssistant(const std::string& content) {
        AddMessage(ChatRole::Assistant, "Assistant", content);
    }
    
    void AddAgent(const std::string& content) {
        AddMessage(ChatRole::Agent, "LittAgent", content);
    }
    
    void AddError(const std::string& content) {
        AddMessage(ChatRole::Error, "Error", content);
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex);
        history.clear();
    }
    
    size_t Size() {
        std::lock_guard<std::mutex> lock(mutex);
        return history.size();
    }
    
    const std::vector<ChatMessage>& GetHistory() {
        return history;
    }
    
    // Command processing
    void ProcessCommand(const std::string& cmd) {
        AddUser(cmd);
        
        if (cmd.empty()) return;
        
        if (cmd == "/help") {
            AddSystem("Commands: /help /status /load <scene> /save /reset /clear /undo /redo /select <name> /delete <name> /add <type> /grid /gizmo");
        } else if (cmd == "/status") {
            AddSystem("FPS: 60 | Draw Calls: 0 | Triangles: 0");
        } else if (cmd == "/clear") {
            Clear();
            AddSystem("Chat cleared");
        } else if (cmd == "/undo") {
            AddSystem("Undo not implemented yet");
        } else if (cmd == "/redo") {
            AddSystem("Redo not implemented yet");
        } else {
            AddAgent("Processing: " + cmd);
            // Would integrate with AI agent backend
        }
    }
};

// =============================================================================
// Editor Camera
// =============================================================================
struct EditorCamera {
    Vec3 position = {0, 5, -10};
    Vec3 target = {0, 0, 0};
    Vec3 up = {0, 1, 0};
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 10.0f;
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    bool orbitMode = true;
    
    Mat4 GetViewMatrix() const {
        return Mat4::look_at(position, target, up);
    }
    
    Mat4 GetProjectionMatrix() const {
        return Mat4::perspective(fov, 16.0f/9.0f, nearPlane, farPlane);
    }
    
    void Orbit(float dyaw, float dpitch) {
        yaw += dyaw;
        pitch += dpitch;
        pitch = std::max(-89.0f, std::min(89.0f, pitch));
        
        float radYaw = yaw * 3.14159f / 180.0f;
        float radPitch = pitch * 3.14159f / 180.0f;
        
        position.x = target.x + distance * cosf(radPitch) * sinf(radYaw);
        position.y = target.y + distance * sinf(radPitch);
        position.z = target.z + distance * cosf(radPitch) * cosf(radYaw);
    }
    
    void Pan(float dx, float dy) {
        Vec3 right = Vec3::forward().cross(up).normalized();
        Vec3 worldUp = up;
        
        target += right * dx;
        target += worldUp * dy;
        position += right * dx;
        position += worldUp * dy;
    }
    
    void Zoom(float delta) {
        distance *= 1.0f - delta * 0.1f;
        distance = std::max(0.5f, std::min(1000.0f, distance));
    }
};

// =============================================================================
// Transform Gizmo
// =============================================================================
enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};

struct Gizmo {
    GizmoMode mode = GizmoMode::Translate;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    bool visible = true;
    bool selected = false;
    
    // Axis colors
    static const Vec3 XColor;
    static const Vec3 YColor;
    static const Vec3 ZColor;
};

const Vec3 Gizmo::XColor = {1.0f, 0.2f, 0.2f};
const Vec3 Gizmo::YColor = {0.2f, 1.0f, 0.2f};
const Vec3 Gizmo::ZColor = {0.2f, 0.2f, 1.0f};

// =============================================================================
// Scene Tree Node
// =============================================================================
struct SceneTreeNode {
    uint32_t id;
    std::string name;
    SceneNode* node = nullptr;
    std::vector<std::unique_ptr<SceneTreeNode>> children;
    bool expanded = true;
    bool selected = false;
    
    SceneTreeNode(uint32_t id, const std::string& name) 
        : id(id), name(name) {}
    
    SceneTreeNode* FindChild(const std::string& name) {
        for (auto& child : children) {
            if (child->name == name) return child.get();
            auto found = child->FindChild(name);
            if (found) return found;
        }
        return nullptr;
    }
};

// =============================================================================
// Inspector Property
// =============================================================================
struct InspectableProperty {
    std::string name;
    std::string type;
    std::string value;
    
    // For Vec3
    float x = 0, y = 0, z = 0;
    // For float
    float floatValue = 0;
    // For int
    int intValue = 0;
    // For bool
    bool boolValue = false;
    // For color
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

// =============================================================================
// Editor State
// =============================================================================
enum class EditorTool {
    Select,
    Move,
    Rotate,
    Scale,
    Create,
    Delete,
    Paint
};

struct EditorState {
    // Scene
    SceneManager sceneManager;
    Scene* activeScene = nullptr;
    
    // Selection (no INVALID_ENTITY constant exists in litt_ecs.h; 0xFFFFFFFF is the "none" sentinel)
    uint32_t selectedNodeId = 0xFFFFFFFFu;
    SceneNode* selectedNode = nullptr;
    
    // Camera
    EditorCamera camera;
    
    // Gizmo
    Gizmo gizmo;
    
    // Tool
    EditorTool currentTool = EditorTool::Select;
    
    // Display options
    bool gridVisible = true;
    bool gizmoVisible = true;
    bool snapToGrid = false;
    float gridSpacing = 1.0f;
    int gridDivisions = 20;
    
    // History
    std::deque<std::function<void()>> undoStack;
    std::deque<std::function<void()>> redoStack;
    static constexpr size_t MaxHistory = 100;
    
    // Stats
    uint32_t frameCount = 0;
    float fps = 0.0f;
    float frameTime = 0.0f;
    
    // Operations
    void PushUndo(std::function<void()> undo, std::function<void()> redo) {
        undoStack.push_back([undo, redo]() {
            undo();
            redoStack.push_back([undo, redo]() { redo(); });
            if (undoStack.size() > MaxHistory) undoStack.pop_front();
        });
    }
    
    bool Undo() {
        if (undoStack.empty()) return false;
        auto op = undoStack.back();
        undoStack.pop_back();
        op();
        return true;
    }
    
    bool Redo() {
        if (redoStack.empty()) return false;
        auto op = redoStack.back();
        redoStack.pop_back();
        op();
        return true;
    }
};

// =============================================================================
// Vulkan Renderer for Editor
// =============================================================================
struct EditorRenderData {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    
    uint32_t width = 1920;
    uint32_t height = 1080;
};

class EditorRenderer {
public:
    EditorRenderData data;
    
    bool Initialize(uint32_t width, uint32_t height) {
        this->width = width;
        this->height = height;
        
        // Create Vulkan instance
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Litt Editor";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Litt";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;
        
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // Enable validation layers in debug
#ifdef DEBUG
        // Enable validation layers
#endif
        
        VkResult result = vkCreateInstance(&createInfo, nullptr, &data.instance);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create Vulkan instance: %d\n", result);
            return false;
        }
        
        // ... (full initialization would follow)
        
        return true;
    }
    
    void Shutdown() {
        if (data.device) {
            vkDeviceWaitIdle(data.device);
            // Cleanup resources
            vkDestroyDevice(data.device, nullptr);
        }
        if (data.instance) {
            vkDestroyInstance(data.instance, nullptr);
        }
    }
    
    void BeginFrame() {
        // Begin render pass
    }
    
    void EndFrame() {
        // Present frame
    }
    
    void RenderScene(const Scene& scene, const EditorCamera& camera) {
        // Render scene nodes
    }
    
    void RenderGizmo(const Gizmo& gizmo, const EditorCamera& camera) {
        // Render transform gizmo
    }
    
    void RenderGrid(const EditorCamera& camera) {
        // Render ground grid
    }
};

// =============================================================================
// Main Editor Window
// =============================================================================
class EditorWindow {
public:
    EditorWindow() = default;
    
    bool Create(HINSTANCE hInst, int nCmdShow) {
        // Register window class
        WNDCLASSEX wc = {sizeof(WNDCLASSEX)};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "LittEditorWindow";
        
        if (!RegisterClassEx(&wc)) {
            fprintf(stderr, "Failed to register window class\n");
            return false;
        }
        
        // Create main window
        hwnd = CreateWindowEx(
            0, "LittEditorWindow", "Litt Engine Editor",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            1400, 900,
            nullptr, nullptr, hInst, nullptr);
        
        if (!hwnd) {
            fprintf(stderr, "Failed to create window\n");
            return false;
        }
        
        // Initialize Vulkan renderer
        renderer.Initialize(1280, 720);
        
        // Show window
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
        
        return true;
    }
    
    void Run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Accessors
    ChatSystem& GetChat() { return chat; }
    EditorState& GetState() { return state; }
    EditorRenderer& GetRenderer() { return renderer; }
    
private:
    HWND hwnd = nullptr;
    EditorState state;
    EditorRenderer renderer;
    ChatSystem chat;
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // Main window procedure
        switch (msg) {
            case WM_CLOSE:
                DestroyWindow(hwnd);
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }
};

// =============================================================================
// Entry Point
// =============================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = {sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES};
    InitCommonControlsEx(&icex);
    
    EditorWindow editor;
    if (!editor.Create(hInst, nCmdShow)) {
        return 1;
    }
    
    // Add welcome message
    editor.GetChat().AddSystem("Litt Editor initialized");
    editor.GetChat().AddAgent("Welcome to Litt Editor! Type /help for commands.");
    
    // Create default scene
    auto& sceneMgr = editor.GetState().sceneManager;
    auto* scene = sceneMgr.CreateScene("Default");
    auto* root = scene->CreateNode("Root");
    
    // Add ground
    auto* ground = root->CreateChild("Ground");
    ground->AddComponent<Transform>({0, -1, 0, 0, 0, 0, 100, 1, 100});
    ground->AddComponent<Material>({0.3f, 0.3f, 0.3f});
    
    // Add camera
    auto* cam = root->CreateChild("Camera");
    cam->AddComponent<Transform>({0, 5, -10, 0, 0, 0, 1, 1, 1});
    cam->AddComponent<Camera>({60.0f, 16.0f/9.0f, 0.1f, 1000.0f});
    
    // Add light
    auto* light = root->CreateChild("Light");
    light->AddComponent<Transform>({5, 10, 5, 0, 0, 0, 1, 1, 1});
    light->AddComponent<Light>({0, 1, 0, 1.0f, 1.0f, 1.0f});
    
    editor.Run();
    return 0;
}

} // namespace editor
} // namespace litt
