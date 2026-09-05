// Litt Engine Editor - Main JavaScript
// Web-based editor for Litt Engine - works without traditional editor

class LittEditor {
    constructor() {
        this.entities = new Map();
        this.selectedEntity = null;
        this.currentTool = 'select';
        this.gridEnabled = true;
        this.gizmoEnabled = true;
        this.canvas = null;
        this.ctx = null;
        this.isRunning = false;
        
        this.init();
    }
    
    init() {
        this.canvas = document.getElementById('viewport-canvas');
        this.ctx = this.canvas.getContext('2d');
        this.resizeCanvas();
        
        // Event listeners
        window.addEventListener('resize', () => this.resizeCanvas());
        this.setupEventListeners();
        
        // Start render loop
        this.render();
        
        // Welcome message
        this.log('info', 'Litt Engine Editor initialized');
        this.log('info', 'Use AI Chat to build your game');
    }
    
    resizeCanvas() {
        const container = this.canvas.parentElement;
        this.canvas.width = container.clientWidth;
        this.canvas.height = container.clientHeight;
    }
    
    setupEventListeners() {
        // Viewport click
        this.canvas.addEventListener('click', (e) => this.handleViewportClick(e));
        
        // Tool buttons
        document.querySelectorAll('.tool-btn[data-tool]').forEach(btn => {
            btn.addEventListener('click', () => this.setTool(btn.dataset.tool));
        });
        
        // Grid toggle
        document.getElementById('btn-grid').addEventListener('click', () => {
            this.gridEnabled = !this.gridEnabled;
            this.log('info', `Grid ${this.gridEnabled ? 'enabled' : 'disabled'}`);
        });
        
        // Add entity
        document.getElementById('btn-add-entity').addEventListener('click', () => this.addEntityDialog());
        
        // Delete entity
        document.getElementById('btn-delete-entity').addEventListener('click', () => this.deleteSelected());
        
        // Export
        document.getElementById('btn-export').addEventListener('click', () => this.exportScene());
        
        // Run/Stop
        document.getElementById('btn-run').addEventListener('click', () => this.runGame());
        document.getElementById('btn-stop').addEventListener('click', () => this.stopGame());
        
        // Tab switching
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.addEventListener('click', () => this.switchTab(btn.dataset.tab));
        });
        
        // AI Chat
        const aiInput = document.getElementById('ai-input');
        const aiSend = document.getElementById('ai-send');
        
        aiSend.addEventListener('click', () => this.handleAICommand());
        aiInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.handleAICommand();
        });
        
        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => this.handleKeyboard(e));
    }
    
    // =============================================================================
    // Entity Management
    // =============================================================================
    
    createEntity(name, options = {}) {
        const id = this.entities.size + 1;
        const entity = {
            id,
            name,
            position: options.position || [0, 0, 0],
            rotation: options.rotation || [0, 0, 0],
            scale: options.scale || [1, 1, 1],
            components: {},
            visible: true,
            active: true
        };
        
        this.entities.set(id, entity);
        
        if (options.components) {
            for (const [type, config] of Object.entries(options.components)) {
                this.addComponent(id, type, config);
            }
        }
        
        this.updateHierarchy();
        this.log('success', `Created entity: ${name} (${id})`);
        
        return entity;
    }
    
    addComponent(entityId, type, config = {}) {
        const entity = this.entities.get(entityId);
        if (!entity) {
            this.log('error', `Entity ${entityId} not found`);
            return null;
        }
        
        entity.components[type] = config;
        this.updateInspector();
        this.log('info', `Added ${type} component to ${entity.name}`);
        
        return entity.components[type];
    }
    
    deleteEntity(entityId) {
        const entity = this.entities.get(entityId);
        if (!entity) return false;
        
        if (this.selectedEntity === entityId) {
            this.selectedEntity = null;
            this.updateInspector();
        }
        
        this.entities.delete(entityId);
        this.updateHierarchy();
        this.log('info', `Deleted entity: ${entity.name}`);
        
        return true;
    }
    
    deleteSelected() {
        if (this.selectedEntity) {
            this.deleteEntity(this.selectedEntity);
        }
    }
    
    selectEntity(entityId) {
        this.selectedEntity = entityId;
        this.updateHierarchy();
        this.updateInspector();
    }
    
    // =============================================================================
    // UI Updates
    // =============================================================================
    
    updateHierarchy() {
        const list = document.getElementById('hierarchy-list');
        list.innerHTML = '';
        
        if (this.entities.size === 0) {
            list.innerHTML = '<div class="empty-state"><div class="empty-text">No entities</div></div>';
            return;
        }
        
        this.entities.forEach((entity, id) => {
            const item = document.createElement('div');
            item.className = `entity-item ${id === this.selectedEntity ? 'selected' : ''}`;
            item.dataset.id = id;
            
            const icon = this.getEntityIcon(entity);
            const name = document.createElement('span');
            name.className = 'entity-name';
            name.textContent = entity.name;
            
            item.appendChild(icon);
            item.appendChild(name);
            
            item.addEventListener('click', () => this.selectEntity(id));
            list.appendChild(item);
        });
        
        this.updateCounts();
    }
    
    updateInspector() {
        const content = document.getElementById('inspector-content');
        
        if (!this.selectedEntity || !this.entities.has(this.selectedEntity)) {
            content.innerHTML = `
                <div class="empty-state">
                    <div class="empty-icon">👆</div>
                    <div class="empty-text">Select an entity to inspect</div>
                </div>
            `;
            return;
        }
        
        const entity = this.entities.get(this.selectedEntity);
        const components = Object.keys(entity.components).join(' ');
        
        content.innerHTML = `
            <div class="inspector-section">
                <div class="inspector-section-title">Transform</div>
                <div class="property-row">
                    <span class="property-label">Name</span>
                    <input class="property-input" value="${entity.name}" 
                           onchange="editor.updateName(${entity.id}, this.value)">
                </div>
                <div class="property-row">
                    <span class="property-label">Position</span>
                    <div class="vec3-input">
                        <span class="vec-label x">X</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.position[0]}" 
                               onchange="editor.updatePosition(${entity.id}, 'x', this.value)">
                        <span class="vec-label y">Y</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.position[1]}" 
                               onchange="editor.updatePosition(${entity.id}, 'y', this.value)">
                        <span class="vec-label z">Z</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.position[2]}" 
                               onchange="editor.updatePosition(${entity.id}, 'z', this.value)">
                    </div>
                </div>
                <div class="property-row">
                    <span class="property-label">Rotation</span>
                    <div class="vec3-input">
                        <span class="vec-label x">X</span>
                        <input class="property-input" type="number" step="1" 
                               value="${entity.rotation[0]}" 
                               onchange="editor.updateRotation(${entity.id}, 'x', this.value)">
                        <span class="vec-label y">Y</span>
                        <input class="property-input" type="number" step="1" 
                               value="${entity.rotation[1]}" 
                               onchange="editor.updateRotation(${entity.id}, 'y', this.value)">
                        <span class="vec-label z">Z</span>
                        <input class="property-input" type="number" step="1" 
                               value="${entity.rotation[2]}" 
                               onchange="editor.updateRotation(${entity.id}, 'z', this.value)">
                    </div>
                </div>
                <div class="property-row">
                    <span class="property-label">Scale</span>
                    <div class="vec3-input">
                        <span class="vec-label x">X</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.scale[0]}" 
                               onchange="editor.updateScale(${entity.id}, 'x', this.value)">
                        <span class="vec-label y">Y</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.scale[1]}" 
                               onchange="editor.updateScale(${entity.id}, 'y', this.value)">
                        <span class="vec-label z">Z</span>
                        <input class="property-input" type="number" step="0.1" 
                               value="${entity.scale[2]}" 
                               onchange="editor.updateScale(${entity.id}, 'z', this.value)">
                    </div>
                </div>
            </div>
            
            <div class="inspector-section">
                <div class="inspector-section-title">Components (${components || 'None'})</div>
                ${Object.entries(entity.components).map(([type, config]) => `
                    <div class="component-badge">
                        <span>${type}</span>
                        <span class="remove" onclick="editor.removeComponent(${entity.id}, '${type}')">×</span>
                    </div>
                `).join('')}
                <div class="property-row" style="margin-top: 8px;">
                    <button class="btn btn-secondary" style="width: 100%;" 
                            onclick="editor.showAddComponent(${entity.id})">
                        + Add Component
                    </button>
                </div>
            </div>
            
            <div class="inspector-section">
                <div class="inspector-section-title">Actions</div>
                <div class="property-row">
                    <button class="btn btn-secondary" style="width: 100%;" 
                            onclick="editor.duplicateEntity(${entity.id})">
                        Duplicate
                    </button>
                </div>
                <div class="property-row">
                    <button class="btn btn-danger" style="width: 100%; background: var(--danger);" 
                            onclick="editor.deleteEntity(${entity.id})">
                        Delete
                    </button>
                </div>
            </div>
        `;
    }
    
    updateCounts() {
        document.getElementById('entity-count').textContent = `Entities: ${this.entities.size}`;
        let compCount = 0;
        this.entities.forEach(e => compCount += Object.keys(e.components).length);
        document.getElementById('component-count').textContent = `Components: ${compCount}`;
    }
    
    getEntityIcon(entity) {
        const hasMesh = 'mesh' in entity.components;
        const hasPhysics = 'physics' in entity.components;
        
        if (hasMesh && hasPhysics) return document.createTextNode('🎮');
        if (hasMesh) return document.createTextNode('⬜');
        if (hasPhysics) return document.createTextNode('⚙️');
        return document.createTextNode('📦');
    }
    
    // =============================================================================
    // Transform Updates
    // =============================================================================
    
    updateName(id, name) {
        const entity = this.entities.get(id);
        if (entity) {
            entity.name = name;
            this.updateHierarchy();
        }
    }
    
    updatePosition(id, axis, value) {
        const entity = this.entities.get(id);
        if (entity) {
            const idx = axis === 'x' ? 0 : axis === 'y' ? 1 : 2;
            entity.position[idx] = parseFloat(value) || 0;
            this.log('info', `Updated ${entity.name} position`);
        }
    }
    
    updateRotation(id, axis, value) {
        const entity = this.entities.get(id);
        if (entity) {
            const idx = axis === 'x' ? 0 : axis === 'y' ? 1 : 2;
            entity.rotation[idx] = parseFloat(value) || 0;
        }
    }
    
    updateScale(id, axis, value) {
        const entity = this.entities.get(id);
        if (entity) {
            const idx = axis === 'x' ? 0 : axis === 'y' ? 1 : 2;
            entity.scale[idx] = parseFloat(value) || 1;
        }
    }
    
    // =============================================================================
    // Component Management
    // =============================================================================
    
    removeComponent(entityId, type) {
        const entity = this.entities.get(entityId);
        if (entity && type in entity.components) {
            delete entity.components[type];
            this.updateInspector();
            this.log('info', `Removed ${type} component`);
        }
    }
    
    showAddComponent(entityId) {
        const types = ['mesh', 'physics', 'light', 'camera', 'script', 'audio'];
        const type = prompt(`Add component to entity ${entityId}:\n${types.join(', ')}`);
        if (type && types.includes(type)) {
            this.addComponent(entityId, type);
        }
    }
    
    // =============================================================================
    // Tools
    // =============================================================================
    
    setTool(tool) {
        this.currentTool = tool;
        document.querySelectorAll('.tool-btn[data-tool]').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tool === tool);
        });
        this.log('info', `Tool: ${tool}`);
    }
    
    // =============================================================================
    // Viewport Rendering
    // =============================================================================
    
    render() {
        if (!this.ctx) return;
        
        const w = this.canvas.width;
        const h = this.canvas.height;
        
        // Clear
        this.ctx.fillStyle = '#1e1e1e';
        this.ctx.fillRect(0, 0, w, h);
        
        // Draw grid
        if (this.gridEnabled) {
            this.drawGrid(w, h);
        }
        
        // Draw entities
        this.entities.forEach((entity, id) => {
            this.drawEntity(entity, id === this.selectedEntity);
        });
        
        requestAnimationFrame(() => this.render());
    }
    
    drawGrid(w, h) {
        this.ctx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
        this.ctx.lineWidth = 1;
        
        const gridSize = 50;
        const offsetX = 0;
        const offsetY = h / 2;
        
        // Vertical lines
        for (let x = offsetX % gridSize; x < w; x += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, h);
            this.ctx.stroke();
        }
        
        // Horizontal lines
        for (let y = offsetY % gridSize; y < h; y += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(w, y);
            this.ctx.stroke();
        }
        
        // Axis lines
        this.ctx.strokeStyle = 'rgba(255, 100, 100, 0.5)';
        this.ctx.beginPath();
        this.ctx.moveTo(w/2, h/2);
        this.ctx.lineTo(w/2 + 100, h/2);
        this.ctx.stroke();
        
        this.ctx.strokeStyle = 'rgba(100, 255, 100, 0.5)';
        this.ctx.beginPath();
        this.ctx.moveTo(w/2, h/2);
        this.ctx.lineTo(w/2, h/2 - 100);
        this.ctx.stroke();
    }
    
    drawEntity(entity, isSelected) {
        const w = this.canvas.width;
        const h = this.canvas.height;
        
        // Convert 3D position to 2D (simple projection)
        const x = w/2 + entity.position[0] * 20;
        const y = h/2 - entity.position[1] * 20;
        const size = 30 * entity.scale[0];
        
        // Selection glow
        if (isSelected) {
            this.ctx.shadowColor = '#4a9eff';
            this.ctx.shadowBlur = 15;
        }
        
        // Draw entity box
        this.ctx.fillStyle = isSelected ? 'rgba(74, 158, 255, 0.3)' : 'rgba(255, 255, 255, 0.1)';
        this.ctx.strokeStyle = isSelected ? '#4a9eff' : 'rgba(255, 255, 255, 0.3)';
        this.ctx.lineWidth = isSelected ? 2 : 1;
        
        this.ctx.fillRect(x - size/2, y - size/2, size, size);
        this.ctx.strokeRect(x - size/2, y - size/2, size, size);
        
        // Reset shadow
        this.ctx.shadowColor = 'transparent';
        this.ctx.shadowBlur = 0;
        
        // Draw label
        this.ctx.fillStyle = 'rgba(255, 255, 255, 0.7)';
        this.ctx.font = '10px sans-serif';
        this.ctx.textAlign = 'center';
        this.ctx.fillText(entity.name, x, y - size/2 - 5);
        
        // Draw component indicators
        let iconY = y + size/2 + 12;
        if (entity.components.mesh) {
            this.ctx.fillText('🎨', x, iconY);
            iconY += 14;
        }
        if (entity.components.physics) {
            this.ctx.fillText('⚙️', x, iconY);
        }
    }
    
    handleViewportClick(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        // Find clicked entity
        let clicked = null;
        this.entities.forEach((entity, id) => {
            const canvasX = this.canvas.width/2 + entity.position[0] * 20;
            const canvasY = this.canvas.height/2 - entity.position[1] * 20;
            const size = 30 * entity.scale[0];
            
            if (Math.abs(x - canvasX) < size/2 && Math.abs(y - canvasY) < size/2) {
                clicked = id;
            }
        });
        
        if (clicked) {
            this.selectEntity(clicked);
        } else {
            this.selectedEntity = null;
            this.updateHierarchy();
            this.updateInspector();
        }
    }
    
    // =============================================================================
    // Game Loop
    // =============================================================================
    
    runGame() {
        this.isRunning = true;
        document.getElementById('btn-run').disabled = true;
        document.getElementById('btn-stop').disabled = false;
        this.log('success', 'Game started');
        
        // Update coordinates display
        this.updateCoordsDisplay();
    }
    
    stopGame() {
        this.isRunning = false;
        document.getElementById('btn-run').disabled = false;
        document.getElementById('btn-stop').disabled = true;
        this.log('info', 'Game stopped');
    }
    
    updateCoordsDisplay() {
        if (!this.selectedEntity) {
            document.getElementById('viewport-coords').textContent = 'X: 0 Y: 0 Z: 0';
            return;
        }
        
        const entity = this.entities.get(this.selectedEntity);
        document.getElementById('viewport-coords').textContent = 
            `X: ${entity.position[0].toFixed(2)} Y: ${entity.position[1].toFixed(2)} Z: ${entity.position[2].toFixed(2)}`;
    }
    
    // =============================================================================
    // Scene I/O
    // =============================================================================
    
    exportScene() {
        const scene = {
            version: '1.0.0',
            exported: new Date().toISOString(),
            entities: Array.from(this.entities.values())
        };
        
        const json = JSON.stringify(scene, null, 2);
        
        // Download
        const blob = new Blob([json], { type: 'application/json' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'litt_scene.json';
        a.click();
        URL.revokeObjectURL(url);
        
        this.log('success', 'Scene exported to litt_scene.json');
    }
    
    importScene(file) {
        const reader = new FileReader();
        reader.onload = (e) => {
            try {
                const scene = JSON.parse(e.target.result);
                this.loadScene(scene);
            } catch (err) {
                this.log('error', `Failed to import: ${err.message}`);
            }
        };
        reader.readAsText(file);
    }
    
    loadScene(scene) {
        this.entities.clear();
        
        if (scene.entities) {
            scene.entities.forEach(entityData => {
                this.entities.set(entityData.id, entityData);
            });
        }
        
        this.updateHierarchy();
        this.log('success', `Loaded scene with ${this.entities.size} entities`);
    }
    
    // =============================================================================
    // AI Integration
    // =============================================================================
    
    handleAICommand() {
        const input = document.getElementById('ai-input');
        const command = input.value.trim();
        if (!command) return;
        
        this.log('command', `> ${command}`);
        input.value = '';
        
        // Parse and execute command
        this.processCommand(command);
    }
    
    processCommand(command) {
        const lower = command.toLowerCase();
        
        // Create entity patterns
        if (lower.startsWith('create') || lower.startsWith('add')) {
            const match = command.match(/create\s+(\w+)\s+(?:with\s+(\w+))?/);
            if (match) {
                const name = match[1];
                const component = match[2] || 'mesh';
                const entity = this.createEntity(name);
                this.addComponent(entity.id, component);
                return;
            }
        }
        
        // Position entity
        const posMatch = command.match(/move\s+(\w+)\s+to\s+([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)/);
        if (posMatch) {
            const [, name, x, y, z] = posMatch;
            const entity = this.findEntityByName(name);
            if (entity) {
                entity.position = [parseFloat(x), parseFloat(y), parseFloat(z)];
                this.updateHierarchy();
                this.log('success', `Moved ${name}`);
            }
            return;
        }
        
        // Delete entity
        if (lower.startsWith('delete')) {
            const match = command.match(/delete\s+(\w+)/);
            if (match) {
                const entity = this.findEntityByName(match[1]);
                if (entity) {
                    this.deleteEntity(entity.id);
                }
            }
            return;
        }
        
        // Unknown command
        this.log('warning', `Unknown command: ${command}`);
        this.log('info', 'Try: "create player with mesh"');
    }
    
    findEntityByName(name) {
        for (const entity of this.entities.values()) {
            if (entity.name.toLowerCase() === name.toLowerCase()) {
                return entity;
            }
        }
        return null;
    }
    
    // =============================================================================
    // Dialogs
    // =============================================================================
    
    addEntityDialog() {
        const name = prompt('Enter entity name:');
        if (!name) return;
        
        const entity = this.createEntity(name);
        this.selectEntity(entity.id);
    }
    
    // =============================================================================
    // Utilities
    // =============================================================================
    
    log(type, message) {
        const output = document.getElementById('console-output');
        const line = document.createElement('div');
        line.className = `console-line ${type}`;
        
        const time = new Date().toLocaleTimeString();
        line.textContent = `[${time}] ${message}`;
        
        output.appendChild(line);
        output.scrollTop = output.scrollHeight;
    }
    
    switchTab(tabName) {
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });
        
        document.getElementById('console-tab').classList.toggle('hidden', tabName !== 'console');
        document.getElementById('output-tab').classList.toggle('hidden', tabName !== 'output');
        document.getElementById('ai-chat-tab').classList.toggle('hidden', tabName !== 'ai-chat');
    }
    
    handleKeyboard(e) {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
        
        switch(e.key.toLowerCase()) {
            case 'v': this.setTool('select'); break;
            case 'w': this.setTool('move'); break;
            case 'e': this.setTool('rotate'); break;
            case 'r': this.setTool('scale'); break;
            case 'delete':
            case 'backspace':
                this.deleteSelected();
                break;
            case 'g':
                this.gridEnabled = !this.gridEnabled;
                this.log('info', `Grid ${this.gridEnabled ? 'enabled' : 'disabled'}`);
                break;
        }
    }
}

// Initialize
const editor = new LittEditor();
