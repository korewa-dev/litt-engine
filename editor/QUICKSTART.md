# Litt Editor - Quick Start

## Running the Editor

### Option 1: Build and Run

```bash
# Build C++ core
cd native
.\build.bat

# Build editor
cd ..\editor
.\build.bat

# Run
.\cs\bin\Release\LittEditor.exe
```

### Option 2: Using .NET SDK

```bash
cd editor\cs
dotnet run
```

## Using the Chat System

The integrated chat works like DeepSeek Harness:

1. Type commands starting with `/` for editor commands
2. Type regular text for AI assistance
3. Chat history is preserved across sessions

Example:
```
> /help
> /status
> create a new ground plane
> select the camera and move it to (0, 5, -10)
```

## Scene Workflow

1. **View Scene**: The center panel shows your 3D scene
2. **Select Nodes**: Click nodes in hierarchy or scene view
3. **Edit Properties**: Use the inspector panel to modify
4. **Transform**: Use toolbar tools to move/rotate/scale
5. **Save**: Use `/save` or File > Save

## Camera Controls

- **Orbit**: Right-click drag
- **Pan**: Middle-click drag or Shift + right-click drag
- **Zoom**: Scroll wheel

## Node Operations

- **Select**: Click in scene or hierarchy
- **Move**: W key or Move tool, then drag gizmo
- **Rotate**: R key or Rotate tool
- **Scale**: S key or Scale tool
- **Delete**: Delete key or Delete tool
