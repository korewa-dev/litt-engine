#!/usr/bin/env python3
"""
Example: Building an AI Editor with Litt Engine

This example demonstrates how an AI can use the Litt Editor API to:
1. Create entities programmatically
2. Add components and configure them
3. Export scenes for game use
"""

from pathlib import Path
import json
import sys

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent / "python"))

try:
    from litt_ai_editor import Editor
except ImportError:
    print("Warning: Python bindings not available. Using mock for demo.")
    
    class MockEditor:
        def __init__(self):
            self.entities = {}
            self.next_id = 1
        
        def create_entity(self, name, position=(0,0,0), rotation=(0,0,0), scale=(1,1,1)):
            entity = {
                "id": self.next_id,
                "name": name,
                "position": position,
                "rotation": rotation,
                "scale": scale,
                "components": {}
            }
            self.entities[self.next_id] = entity
            self.next_id += 1
            return {"success": True, "entity_id": entity["id"]}
        
        def add_component(self, entity_id, component_type, config=None):
            if entity_id not in self.entities:
                return {"success": False, "error": f"Entity {entity_id} not found"}
            
            if component_type not in self.entities[entity_id]["components"]:
                self.entities[entity_id]["components"][component_type] = config or {}
            else:
                self.entities[entity_id]["components"][component_type].update(config or {})
            
            return {"success": True}
        
        def export_scene(self, output_path):
            scene = {
                "version": "1.0",
                "entities": list(self.entities.values())
            }
            with open(output_path, 'w') as f:
                json.dump(scene, f, indent=2)
            return {"success": True, "path": output_path}
        
        def list_entities(self):
            return list(self.entities.values())
    
    Editor = MockEditor


def build_player_character():
    """Build a complete player character with all components"""
    editor = Editor()
    
    print("Building player character...")
    
    # Create player entity
    player_result = editor.create_entity(
        "player",
        position=(0, 0, 0),
        rotation=(0, 0, 0),
        scale=(1, 1, 1)
    )
    player_id = player_result['entity_id']
    print(f"  Created player entity: {player_id}")
    
    # Add mesh component
    editor.add_component(player_id, "mesh", {
        "model": "assets/player.glb",
        "material": "default",
        "visible": True
    })
    print("  Added mesh component")
    
    # Add physics component
    editor.add_component(player_id, "physics", {
        "type": "dynamic",
        "mass": 70.0,
        "friction": 0.7,
        "restitution": 0.3
    })
    print("  Added physics component")
    
    # Add script component
    editor.add_component(player_id, "script", {
        "path": "scripts/player.lua",
        "params": {
            "max_health": 100,
            "speed": 5.0,
            "jump_force": 10.0
        }
    })
    print("  Added script component")
    
    # Export scene
    output_path = "output/player_scene.json"
    result = editor.export_scene(output_path)
    print(f"  Exported scene to: {output_path}")
    
    return editor


def build_level():
    """Build a complete game level"""
    editor = Editor()
    
    print("Building game level...")
    
    # Ground plane
    ground = editor.create_entity("ground", position=(0, -1, 0), scale=(100, 1, 100))
    editor.add_component(ground['entity_id'], "mesh", {"model": "assets/plane.glb"})
    editor.add_component(ground['entity_id'], "physics", {"type": "static"})
    print(f"  Created ground: {ground['entity_id']}")
    
    # Walls
    wall_positions = [
        (50, 2, 0, 0, 90, 0),   # North
        (-50, 2, 0, 0, 90, 0),  # South
        (0, 2, 50, 0, 0, 0),    # East
        (0, 2, -50, 0, 0, 0),   # West
    ]
    
    for i, (x, y, z, rx, ry, rz) in enumerate(wall_positions):
        wall = editor.create_entity(f"wall_{i}", position=(x, y, z), rotation=(rx, ry, rz))
        editor.add_component(wall['entity_id'], "mesh", {"model": "assets/wall.glb"})
        editor.add_component(wall['entity_id'], "physics", {"type": "static"})
        print(f"  Created wall_{i}: {wall['entity_id']}")
    
    # Collectibles
    for i in range(10):
        angle = (i / 10) * 2 * 3.14159
        x = 20 * 0.5 * (i % 2 == 0) - 10
        z = 20 * 0.5 * (i % 2 != 0) - 10
        coin = editor.create_entity(f"coin_{i}", position=(x, 1, z))
        editor.add_component(coin['entity_id'], "mesh", {"model": "assets/coin.glb"})
        editor.add_component(coin['entity_id'], "physics", {"type": "static"})
        print(f"  Created coin_{i}: {coin['entity_id']}")
    
    # Export
    output_path = "output/level_scene.json"
    editor.export_scene(output_path)
    print(f"  Exported level to: {output_path}")
    
    return editor


def build_camera_system():
    """Build a camera system for the editor"""
    editor = Editor()
    
    print("Building camera system...")
    
    # Main camera
    camera = editor.create_entity("camera_main", position=(0, 5, -10), rotation=(15, 0, 0))
    editor.add_component(camera['entity_id'], "camera", {
        "fov": 60,
        "near": 0.1,
        "far": 1000,
        "aspect": 16/9,
        "target": "player"
    })
    print(f"  Created main camera: {camera['entity_id']}")
    
    # Follow script
    editor.add_component(camera['entity_id'], "script", {
        "path": "scripts/camera_follow.lua",
        "params": {
            "distance": 10,
            "height": 5,
            "smoothness": 0.1
        }
    })
    print("  Added follow script")
    
    # Output
    editor.export_scene("output/camera_scene.json")
    print("  Exported camera scene")
    
    return editor


def main():
    """Run all examples"""
    output_dir = Path("output")
    output_dir.mkdir(exist_ok=True)
    
    print("=" * 60)
    print("Litt Engine AI Editor Examples")
    print("=" * 60)
    print()
    
    # Example 1: Player character
    print("[Example 1] Building player character...")
    build_player_character()
    print()
    
    # Example 2: Game level
    print("[Example 2] Building game level...")
    build_level()
    print()
    
    # Example 3: Camera system
    print("[Example 3] Building camera system...")
    build_camera_system()
    print()
    
    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)
    print()
    print("Output files:")
    for f in output_dir.glob("*.json"):
        print(f"  - {f}")


if __name__ == "__main__":
    main()
