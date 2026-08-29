"""
Editor class for Litt Engine AI Editor API
"""
import json
from pathlib import Path
from typing import Optional, Dict, List, Tuple, Any


class Editor:
    """AI Editor for Litt Engine - allows any AI system to interact with the engine"""
    
    def __init__(self, config_path: Optional[str] = None):
        """Initialize editor with optional config file"""
        self.entities: Dict[int, Dict[str, Any]] = {}
        self.next_id: int = 1
        self.version: str = "1.0.0"
        self._event_handlers: Dict[str, List] = {}
        
        if config_path:
            self._load_config(config_path)
    
    def _load_config(self, config_path: str):
        """Load configuration from file"""
        path = Path(config_path)
        if path.exists():
            with open(path, 'r') as f:
                config = json.load(f)
                # Apply config (simplified for demo)
                print(f"Loaded config from {config_path}")
    
    def close(self):
        """Destroy editor and free resources"""
        self.entities.clear()
        self.next_id = 1
    
    def version(self) -> str:
        """Get API version string"""
        return self.version
    
    # =============================================================================
    # Scene Management
    # =============================================================================
    
    def create_entity(self, name: str, position: Tuple[float, float, float] = (0, 0, 0),
                      rotation: Tuple[float, float, float] = (0, 0, 0),
                      scale: Tuple[float, float, float] = (1, 1, 1)) -> Dict[str, Any]:
        """Create new entity"""
        entity = {
            "id": self.next_id,
            "name": name,
            "position": list(position),
            "rotation": list(rotation),
            "scale": list(scale),
            "components": {},
            "tags": [],
            "active": True
        }
        self.entities[self.next_id] = entity
        self.next_id += 1
        return {"success": True, "entity_id": entity["id"]}
    
    def delete_entity(self, entity_id: int) -> Dict[str, Any]:
        """Remove entity"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        del self.entities[entity_id]
        return {"success": True}
    
    def set_position(self, entity_id: int, position: Tuple[float, float, float]) -> Dict[str, Any]:
        """Set entity position"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        self.entities[entity_id]["position"] = list(position)
        return {"success": True}
    
    def set_rotation(self, entity_id: int, rotation: Tuple[float, float, float]) -> Dict[str, Any]:
        """Set entity rotation"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        self.entities[entity_id]["rotation"] = list(rotation)
        return {"success": True}
    
    def set_scale(self, entity_id: int, scale: Tuple[float, float, float]) -> Dict[str, Any]:
        """Set entity scale"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        self.entities[entity_id]["scale"] = list(scale)
        return {"success": True}
    
    def get_entity(self, entity_id: int) -> Optional[Dict[str, Any]]:
        """Get entity properties"""
        return self.entities.get(entity_id)
    
    def list_entities(self) -> List[Dict[str, Any]]:
        """List all entities"""
        return list(self.entities.values())
    
    # =============================================================================
    # Component Operations
    # =============================================================================
    
    def add_component(self, entity_id: int, component_type: str, 
                      config: Optional[Dict] = None) -> Dict[str, Any]:
        """Add component to entity"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        
        if component_type not in self.entities[entity_id]["components"]:
            self.entities[entity_id]["components"][component_type] = config or {}
        else:
            # Merge config
            self.entities[entity_id]["components"][component_type].update(config or {})
        
        return {"success": True}
    
    def remove_component(self, entity_id: int, component_type: str) -> Dict[str, Any]:
        """Remove component from entity"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        
        if component_type in self.entities[entity_id]["components"]:
            del self.entities[entity_id]["components"][component_type]
            return {"success": True}
        
        return {"success": False, "error": f"Component {component_type} not found"}
    
    def set_component_property(self, entity_id: int, component_type: str,
                               property_name: str, value: Any) -> Dict[str, Any]:
        """Set component property"""
        if entity_id not in self.entities:
            return {"success": False, "error": f"Entity {entity_id} not found"}
        
        if component_type not in self.entities[entity_id]["components"]:
            return {"success": False, "error": f"Component {component_type} not found"}
        
        self.entities[entity_id]["components"][component_type][property_name] = value
        return {"success": True}
    
    # =============================================================================
    # Asset Operations
    # =============================================================================
    
    def load_asset(self, path: str, asset_type: str) -> Dict[str, Any]:
        """Load asset from disk"""
        full_path = Path(path)
        if not full_path.exists():
            return {"success": False, "error": f"Asset not found: {path}"}
        
        # In real implementation, this would load into engine
        return {"success": True, "path": str(full_path), "type": asset_type}
    
    def export_scene(self, output_path: str) -> Dict[str, Any]:
        """Export scene to JSON"""
        scene = {
            "version": self.version,
            "entities": list(self.entities.values())
        }
        
        path = Path(output_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(path, 'w') as f:
            json.dump(scene, f, indent=2)
        
        return {"success": True, "path": str(path)}
    
    def import_scene(self, input_path: str) -> Dict[str, Any]:
        """Import scene from JSON"""
        path = Path(input_path)
        if not path.exists():
            return {"success": False, "error": f"Scene not found: {input_path}"}
        
        with open(path, 'r') as f:
            scene = json.load(f)
        
        # Clear current entities
        self.entities.clear()
        self.next_id = 1
        
        # Import entities
        for entity_data in scene.get("entities", []):
            entity_id = self.create_entity(
                entity_data.get("name", "unnamed"),
                tuple(entity_data.get("position", [0, 0, 0])),
                tuple(entity_data.get("rotation", [0, 0, 0])),
                tuple(entity_data.get("scale", [1, 1, 1]))
            )
            
            # Restore components
            for comp_type, comp_config in entity_data.get("components", {}).items():
                self.add_component(entity_id["entity_id"], comp_type, comp_config)
        
        return {"success": True}
    
    # =============================================================================
    # Renderer Operations
    # =============================================================================
    
    def render_frame(self, width: int, height: int, output_path: str) -> Dict[str, Any]:
        """Render single frame"""
        # In real implementation, this would call Vulkan/DX12 renderer
        return {
            "success": True,
            "width": width,
            "height": height,
            "output": output_path
        }
    
    def set_camera(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Configure camera"""
        return {"success": True}
    
    def add_light(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Add light source"""
        return {"success": True}
    
    # =============================================================================
    # Query Operations
    # =============================================================================
    
    def count_entities(self) -> int:
        """Get entity count"""
        return len(self.entities)
    
    def count_components(self, component_type: str) -> int:
        """Get component count"""
        count = 0
        for entity in self.entities.values():
            if component_type in entity.get("components", {}):
                count += 1
        return count
    
    def find_by_name(self, name: str) -> Optional[Dict[str, Any]]:
        """Find entity by name"""
        for entity in self.entities.values():
            if entity.get("name") == name:
                return entity
        return None
    
    def find_by_tag(self, tag: str) -> List[Dict[str, Any]]:
        """Find entities by tag"""
        return [e for e in self.entities.values() if tag in e.get("tags", [])]
    
    # =============================================================================
    # Script Operations
    # =============================================================================
    
    def execute_script(self, path: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Run script file"""
        # In real implementation, this would execute Lua/Python script
        return {"success": True, "path": path}
    
    def evaluate(self, expression: str) -> Dict[str, Any]:
        """Evaluate expression"""
        # In real implementation, this would evaluate expression
        return {"success": True, "result": expression}
    
    # =============================================================================
    # Event System
    # =============================================================================
    
    def subscribe(self, event_name: str, callback) -> int:
        """Subscribe to event"""
        if event_name not in self._event_handlers:
            self._event_handlers[event_name] = []
        
        handler_id = len(self._event_handlers[event_name])
        self._event_handlers[event_name].append(callback)
        return handler_id
    
    def unsubscribe(self, handler_id: int, event_name: Optional[str] = None) -> Dict[str, Any]:
        """Unsubscribe from event"""
        if event_name and event_name in self._event_handlers:
            if 0 <= handler_id < len(self._event_handlers[event_name]):
                self._event_handlers[event_name].pop(handler_id)
                return {"success": True}
        return {"success": False, "error": "Handler not found"}
    
    def emit(self, event_name: str, data: Optional[Dict] = None) -> Dict[str, Any]:
        """Emit event"""
        if event_name in self._event_handlers:
            for handler in self._event_handlers[event_name]:
                handler(data or {})
        return {"success": True}


if __name__ == "__main__":
    # Demo
    editor = Editor()
    print(f"Litt Editor v{editor.version()}")
    
    # Create entity
    result = editor.create_entity("player", position=(0, 0, 0))
    print(f"Created entity: {result}")
    
    # Add component
    result = editor.add_component(result["entity_id"], "mesh", {"model": "player.glb"})
    print(f"Added component: {result}")
    
    # List entities
    entities = editor.list_entities()
    print(f"Total entities: {len(entities)}")
    
    # Export scene
    result = editor.export_scene("demo_scene.json")
    print(f"Exported scene: {result}")
