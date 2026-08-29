"""
Demonstration: How Any AI Can Use Litt Engine to Build an Editor

This script shows how an AI system can programmatically build a complete
game scene using the Litt Engine AI Editor API.
"""

from pathlib import Path
import json
from litt_ai_editor import Editor


def build_rpg_game(editor: Editor) -> dict:
    """
    Build a complete RPG game scene demonstrating AI capabilities.
    
    Returns a summary of what was created.
    """
    summary = {
        "entities_created": 0,
        "components_added": 0,
        "scene_file": "rpg_level.json"
    }
    
    # =============================================================================
    # 1. Create World Terrain
    # =============================================================================
    
    # Ground plane
    ground = editor.create_entity("ground", position=(0, -1, 0))
    editor.add_component(ground, "mesh", {
        "model": "assets/terrain/ground.glb",
        "material": "grass"
    })
    editor.add_component(ground, "physics", {
        "type": "static",
        "friction": 0.8
    })
    summary["entities_created"] += 1
    summary["components_added"] += 2
    
    # Hills and mountains
    for i in range(5):
        hill = editor.create_entity(f"hill_{i}", position=(i*20-40, i*2, 0))
        editor.add_component(hill, "mesh", {
            "model": "assets/terrain/hill.glb",
            "scale": [2, 1, 2]
        })
        editor.add_component(hill, "physics", {"type": "static"})
        summary["entities_created"] += 1
        summary["components_added"] += 2
    
    # =============================================================================
    # 2. Create Player Character
    # =============================================================================
    
    player = editor.create_entity("player", position=(0, 1, 0))
    editor.add_component(player, "mesh", {
        "model": "assets/characters/hero.glb",
        "material": "hero_armor"
    })
    editor.add_component(player, "physics", {
        "type": "dynamic",
        "mass": 70,
        "friction": 0.7,
        "restitution": 0.1,
        "can_sleep": True
    })
    editor.add_component(player, "script", {
        "path": "scripts/player_controller.lua",
        "params": {
            "max_health": 100,
            "speed": 5.0,
            "jump_force": 10.0,
            "attack_damage": 25
        }
    })
    editor.add_component(player, "animation", {
        "skeleton": "assets/characters/hero_skel.glb",
        "default_state": "idle"
    })
    summary["entities_created"] += 1
    summary["components_added"] += 4
    
    # =============================================================================
    # 3. Create Enemies
    # =============================================================================
    
    enemy_types = [
        ("goblin", "assets/characters/goblin.glb", 30),
        ("skeleton", "assets/characters/skeleton.glb", 50),
        ("orc", "assets/characters/orc.glb", 100)
    ]
    
    for i, (name, model, health) in enumerate(enemy_types):
        enemy = editor.create_entity(name, position=(10 + i*8, 1, 0))
        editor.add_component(enemy, "mesh", {"model": model})
        editor.add_component(enemy, "physics", {
            "type": "dynamic",
            "mass": 50
        })
        editor.add_component(enemy, "script", {
            "path": "scripts/enemy_ai.lua",
            "params": {
                "health": health,
                "aggression": 0.7,
                "detection_range": 15
            }
        })
        summary["entities_created"] += 1
        summary["components_added"] += 3
    
    # =============================================================================
    # 4. Create Collectibles
    # =============================================================================
    
    # Coins in a circle
    import math
    for i in range(12):
        angle = (i / 12) * 2 * math.pi
        x = 8 * math.cos(angle)
        z = 8 * math.sin(angle)
        
        coin = editor.create_entity(f"coin_{i}", position=(x, 1, z))
        editor.add_component(coin, "mesh", {"model": "assets/items/coin.glb"})
        editor.add_component(coin, "script", {
            "path": "scripts/collectible.lua",
            "params": {"type": "coin", "value": 10, "sound": "coin_pickup.ogg"}
        })
        summary["entities_created"] += 1
        summary["components_added"] += 2
    
    # Health potions
    for i in range(3):
        potion = editor.create_entity(f"potion_{i}", position=(i*15, 1, -10))
        editor.add_component(potion, "mesh", {"model": "assets/items/potion.glb"})
        editor.add_component(potion, "script", {
            "path": "scripts/collectible.lua",
            "params": {"type": "potion", "value": 25, "heal": True}
        })
        summary["entities_created"] += 1
        summary["components_added"] += 2
    
    # =============================================================================
    # 5. Create Environment
    # =============================================================================
    
    # Trees
    for i in range(8):
        tree = editor.create_entity(f"tree_{i}", position=(i*12-40, 0, -15))
        editor.add_component(tree, "mesh", {"model": "assets/environment/tree.glb"})
        editor.add_component(tree, "physics", {"type": "static"})
        summary["entities_created"] += 1
        summary["components_added"] += 2
    
    # Rocks
    for i in range(5):
        rock = editor.create_entity(f"rock_{i}", position=(i*10, 0, 15))
        editor.add_component(rock, "mesh", {"model": "assets/environment/rock.glb"})
        editor.add_component(rock, "physics", {"type": "static"})
        summary["entities_created"] += 1
        summary["components_added"] += 2
    
    # =============================================================================
    # 6. Create Lighting
    # =============================================================================
    
    sun = editor.create_entity("sun", position=(50, 100, 50))
    editor.add_component(sun, "light", {
        "type": "directional",
        "color": [1.0, 0.95, 0.8],
        "intensity": 1.5,
        "cast_shadows": True
    })
    summary["entities_created"] += 1
    summary["components_added"] += 1
    
    # Ambient light
    ambient = editor.create_entity("ambient", position=(0, 50, 0))
    editor.add_component(ambient, "light", {
        "type": "ambient",
        "color": [0.4, 0.4, 0.5],
        "intensity": 0.3
    })
    summary["entities_created"] += 1
    summary["components_added"] += 1
    
    # =============================================================================
    # 7. Create Camera
    # =============================================================================
    
    camera = editor.create_entity("main_camera", position=(0, 10, -20))
    editor.add_component(camera, "camera", {
        "fov": 60,
        "near": 0.1,
        "far": 500,
        "aspect": 16/9
    })
    editor.add_component(camera, "script", {
        "path": "scripts/camera_follow.lua",
        "params": {"target": "player", "distance": 15, "height": 5}
    })
    summary["entities_created"] += 1
    summary["components_added"] += 2
    
    # =============================================================================
    # 8. Create Spawn Points
    # =============================================================================
    
    spawn_player = editor.create_entity("spawn_player", position=(0, 1, 0))
    editor.add_component(spawn_player, "spawn_point", {
        "name": "player_start",
        "team": "player"
    })
    summary["entities_created"] += 1
    summary["components_added"] += 1
    
    spawn_enemy = editor.create_entity("spawn_enemy", position=(20, 1, 0))
    editor.add_component(spawn_enemy, "spawn_point", {
        "name": "enemy_spawn",
        "team": "enemy"
    })
    summary["entities_created"] += 1
    summary["components_added"] += 1
    
    # =============================================================================
    # Export Scene
    # =============================================================================
    
    output_path = "output/rpg_level.json"
    result = editor.export_scene(output_path)
    summary["scene_file"] = output_path
    
    # Print summary
    print("=" * 60)
    print("🎮 RPG Game Scene Built by AI")
    print("=" * 60)
    print(f"Entities created: {summary['entities_created']}")
    print(f"Components added: {summary['components_added']}")
    print(f"Scene exported to: {output_path}")
    print("=" * 60)
    
    return summary


def build_platformer_level(editor: Editor) -> dict:
    """
    Build a 2D platformer level.
    """
    print("\n" + "=" * 60)
    print("🎮 2D Platformer Level Built by AI")
    print("=" * 60)
    
    entities = []
    components = 0
    
    # Ground platforms
    for i in range(10):
        platform = editor.create_entity(f"platform_{i}", position=(i*10, 0, 0))
        editor.add_component(platform, "mesh", {"model": "assets/platform.glb"})
        editor.add_component(platform, "physics", {"type": "static"})
        entities.append(platform)
        components += 2
    
    # Platforms at different heights
    heights = [3, 6, 4, 8, 5]
    for i, h in enumerate(heights):
        plat = editor.create_entity(f"high_platform_{i}", position=(15 + i*12, h, 0))
        editor.add_component(plat, "mesh", {"model": "assets/platform.glb"})
        editor.add_component(plat, "physics", {"type": "static"})
        entities.append(plat)
        components += 2
    
    # Player
    player = editor.create_entity("player", position=(0, 5, 0))
    editor.add_component(player, "mesh", {"model": "assets/character.glb"})
    editor.add_component(player, "physics", {"type": "dynamic", "mass": 50})
    editor.add_component(player, "script", {"path": "scripts/platformer_controller.lua"})
    entities.append(player)
    components += 3
    
    # Coins
    for i in range(15):
        coin = editor.create_entity(f"coin_{i}", position=(i*8+3, 2, 0))
        editor.add_component(coin, "mesh", {"model": "assets/coin.glb"})
        editor.add_component(coin, "script", {
            "path": "scripts/collectible.lua",
            "params": {"type": "coin", "value": 10}
        })
        entities.append(coin)
        components += 2
    
    # Enemy
    enemy = editor.create_entity("enemy", position=(30, 3, 0))
    editor.add_component(enemy, "mesh", {"model": "assets/enemy.glb"})
    editor.add_component(enemy, "physics", {"type": "dynamic"})
    editor.add_component(enemy, "script", {
        "path": "scripts/enemy_ai.lua",
        "params": {"patrol_range": 10}
    })
    entities.append(enemy)
    components += 3
    
    # Camera
    camera = editor.create_entity("camera", position=(0, 10, -10))
    editor.add_component(camera, "camera", {"fov": 60, "near": 0.1, "far": 100})
    editor.add_component(camera, "script", {
        "path": "scripts/camera_follow.lua",
        "params": {"target": "player"}
    })
    entities.append(camera)
    components += 2
    
    # Light
    light = editor.create_entity("light", position=(0, 50, 0))
    editor.add_component(light, "light", {"type": "directional", "intensity": 1.0})
    entities.append(light)
    components += 1
    
    # Export
    output = "output/platformer_level.json"
    editor.export_scene(output)
    
    print(f"Entities: {len(entities)}")
    print(f"Components: {components}")
    print(f"Exported: {output}")
    print("=" * 60)
    
    return {"entities": len(entities), "components": components, "file": output}


def main():
    """Main demonstration function."""
    print("\n" + "=" * 60)
    print("🤖 Litt Engine AI Editor Demonstration")
    print("=" * 60)
    print("\nThis demonstrates how ANY AI system can use Litt Engine")
    print("to build complete game editors and scenes.\n")
    
    # Create editor instance
    editor = Editor()
    
    # Build RPG game
    rpg_summary = build_rpg_game(editor)
    
    # Create new editor for platformer
    editor2 = Editor()
    platformer_summary = build_platformer_level(editor2)
    
    # Show what AI can do
    print("\n" + "=" * 60)
    print("🤖 What AI Can Now Do with Litt Engine:")
    print("=" * 60)
    print("""
    1. BUILD GAME SCENES
       - Create entities programmatically
       - Add components (mesh, physics, script)
       - Set positions, rotations, scales
    
    2. USE NATURAL LANGUAGE
       - "Create a player character"
       - "Add physics with mass 70"
       - "Export the scene to JSON"
    
    3. AUTOMATE WORKFLOWS
       - Generate entire levels from templates
       - Create multiple variants automatically
       - Test and iterate rapidly
    
    4. WORK WITH ANY LANGUAGE
       - Python API (shown here)
       - JSON-RPC protocol (any language)
       - Web editor (browser automation)
    
    5. INTEGRATE WITH AI SYSTEMS
       - LangChain, AutoGPT, etc.
       - Custom AI agents
       - Game design automation
    """)
    
    print("=" * 60)
    print("✅ Demonstration Complete")
    print("=" * 60)
    print(f"\nGenerated files:")
    print(f"  - {rpg_summary['scene_file']}")
    print(f"  - {platformer_summary['file']}")
    print()


if __name__ == "__main__":
    main()
