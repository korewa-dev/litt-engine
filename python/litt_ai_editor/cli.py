#!/usr/bin/env python3
"""
AI Editor CLI - Command line interface for Litt Engine AI Editor
Usage:
    litt editor create --name player --position 0 0 0
    litt editor add-component --entity 1 --type mesh --config file.json
    litt editor export --output scene.json
    litt editor list
"""
import argparse
import json
import sys
from pathlib import Path

# Try to import Python bindings, fall back to C extension
try:
    from litt_ai_editor import Editor
    HAS_PYTHON = True
except ImportError:
    HAS_PYTHON = False
    print("Warning: Python bindings not found. Install with: pip install -e .", file=sys.stderr)

class CLIEditor:
    def __init__(self):
        self.editor = Editor() if HAS_PYTHON else None
    
    def create_entity(self, name, position, rotation=None, scale=None):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        
        result = self.editor.create_entity(
            name=name,
            position=tuple(position) if position else (0, 0, 0),
            rotation=tuple(rotation) if rotation else (0, 0, 0),
            scale=tuple(scale) if scale else (1, 1, 1)
        )
        return result
    
    def delete_entity(self, entity_id):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        return self.editor.delete_entity(entity_id)
    
    def add_component(self, entity_id, component_type, config=None):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        
        result = self.editor.add_component(entity_id, component_type, config or {})
        return result
    
    def remove_component(self, entity_id, component_type):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        return self.editor.remove_component(entity_id, component_type)
    
    def export_scene(self, output_path):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        return self.editor.export_scene(output_path)
    
    def import_scene(self, input_path):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        return self.editor.import_scene(input_path)
    
    def list_entities(self):
        if not self.editor:
            return []
        return self.editor.list_entities()
    
    def get_entity(self, entity_id):
        if not self.editor:
            return None
        return self.editor.get_entity(entity_id)
    
    def render_frame(self, width, height, output_path):
        if not self.editor:
            return {"success": False, "error": "Editor not initialized"}
        return self.editor.render_frame(width, height, output_path)


def main():
    parser = argparse.ArgumentParser(
        prog='litt editor',
        description='Litt Engine AI Editor CLI'
    )
    subparsers = parser.add_subparsers(dest='command', help='Commands')
    
    # Create entity
    create_parser = subparsers.add_parser('create', help='Create new entity')
    create_parser.add_argument('--name', required=True, help='Entity name')
    create_parser.add_argument('--position', nargs=3, type=float, default=[0, 0, 0],
                               help='Position (x y z)')
    create_parser.add_argument('--rotation', nargs=3, type=float, default=[0, 0, 0],
                               help='Rotation (x y z) in degrees')
    create_parser.add_argument('--scale', nargs=3, type=float, default=[1, 1, 1],
                               help='Scale (x y z)')
    create_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Delete entity
    delete_parser = subparsers.add_parser('delete', help='Delete entity')
    delete_parser.add_argument('--id', type=int, required=True, help='Entity ID')
    delete_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Add component
    component_parser = subparsers.add_parser('add-component', help='Add component')
    component_parser.add_argument('--entity', type=int, required=True, help='Entity ID')
    component_parser.add_argument('--type', required=True, 
                                  choices=['mesh', 'physics', 'light', 'camera', 'script', 'audio'],
                                  help='Component type')
    component_parser.add_argument('--config', type=str, help='Config file path')
    component_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Remove component
    remove_parser = subparsers.add_parser('remove-component', help='Remove component')
    remove_parser.add_argument('--entity', type=int, required=True, help='Entity ID')
    remove_parser.add_argument('--type', required=True, 
                               choices=['mesh', 'physics', 'light', 'camera', 'script', 'audio'],
                               help='Component type')
    remove_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # List entities
    list_parser = subparsers.add_parser('list', help='List all entities')
    list_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Export scene
    export_parser = subparsers.add_parser('export', help='Export scene')
    export_parser.add_argument('--output', '-o', required=True, help='Output path')
    export_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Import scene
    import_parser = subparsers.add_parser('import', help='Import scene')
    import_parser.add_argument('--input', '-i', required=True, help='Input path')
    import_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Render frame
    render_parser = subparsers.add_parser('render', help='Render single frame')
    render_parser.add_argument('--width', '-w', type=int, default=1920, help='Width')
    render_parser.add_argument('--height', '-h', type=int, default=1080, help='Height')
    render_parser.add_argument('--output', '-o', required=True, help='Output path')
    render_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    # Version
    version_parser = subparsers.add_parser('version', help='Show version')
    version_parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    cli = CLIEditor()
    
    # Execute command
    if args.command == 'version':
        if HAS_PYTHON:
            version = cli.editor.version()
        else:
            version = "1.0.0 (Python bindings not available)"
        
        if args.json:
            print(json.dumps({"version": version}))
        else:
            print(f"Litt Editor v{version}")
    
    elif args.command == 'create':
        result = cli.create_entity(args.name, args.position, args.rotation, args.scale)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Created entity '{args.name}' with ID {result['entity_id']}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'delete':
        result = cli.delete_entity(args.id)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Deleted entity {args.id}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'add-component':
        config = None
        if args.config:
            with open(args.config, 'r') as f:
                config = json.load(f)
        
        result = cli.add_component(args.entity, args.type, config)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Added {args.type} component to entity {args.entity}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'remove-component':
        result = cli.remove_component(args.entity, args.type)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Removed {args.type} component from entity {args.entity}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'list':
        entities = cli.list_entities()
        if args.json:
            print(json.dumps(entities, indent=2))
        else:
            print(f"Entities ({len(entities)}):")
            for ent in entities:
                print(f"  [{ent['id']}] {ent['name']} at {ent['position']}")
    
    elif args.command == 'export':
        result = cli.export_scene(args.output)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Scene exported to {args.output}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'import':
        result = cli.import_scene(args.input)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Scene imported from {args.input}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)
    
    elif args.command == 'render':
        result = cli.render_frame(args.width, args.height, args.output)
        if args.json:
            print(json.dumps(result, indent=2))
        else:
            if result.get('success'):
                print(f"Frame rendered to {args.output}")
            else:
                print(f"Error: {result.get('error', 'Unknown error')}", file=sys.stderr)
                sys.exit(1)


if __name__ == '__main__':
    main()
