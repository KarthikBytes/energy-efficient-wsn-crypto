<script type="importmap">
{
    "imports": {
        "three": "https://cdn.jsdelivr.net/npm/three@0.162.0/build/three.module.js",
        "three/addons/": "https://cdn.jsdelivr.net/npm/three@0.162.0/examples/jsm/"
    }
}
</script>

<script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// WebSocket Connection
let ws = null;
let reconnectAttempts = 0;
const MAX_RECONNECTS = 5;
const RECONNECT_DELAY = 2000;

// Simulation State
const simulationState = {
    nodes: [],
    connections: [],
    packets: [],
    packetPool: [],
    maxPackets: 100,
    stats: {
        nodes: 0,
        packetsTx: 0,
        packetsRx: 0,
        pdr: 0,
        energy: 0,
        throughput: 0,
        encrypted: 0,
        decrypted: 0
    },
    events: [],
    isConnected: false,
    cryptoEnabled: true,
    simulationRunning: false,
    networkLayout: 'GRID',
    simulationSpeed: 1.0
};

// Three.js Setup
const scene = new THREE.Scene();
scene.fog = new THREE.FogExp2(0x0a0a14, 0.002);

const camera = new THREE.PerspectiveCamera(75, 1, 0.1, 1000);
camera.position.set(50, 50, 50);

const renderer = new THREE.WebGLRenderer({
    canvas: document.getElementById('network-canvas'),
    antialias: true,
    alpha: true
});
renderer.setPixelRatio(window.devicePixelRatio);

const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.dampingFactor = 0.05;
controls.autoRotate = false;
controls.rotateSpeed = 0.5;

// Node Materials
const materials = {
    normal: new THREE.MeshStandardMaterial({
        color: 0x667eea,
        emissive: 0x667eea,
        emissiveIntensity: 0.3,
        transparent: true,
        opacity: 0.9
    }),
    gateway: new THREE.MeshStandardMaterial({
        color: 0x43e97b,
        emissive: 0x43e97b,
        emissiveIntensity: 0.5,
        transparent: true,
        opacity: 0.9
    }),
    compromised: new THREE.MeshStandardMaterial({
        color: 0xf857a6,
        emissive: 0xf857a6,
        emissiveIntensity: 0.4,
        transparent: true,
        opacity: 0.9
    })
};

// Lighting
const ambientLight = new THREE.AmbientLight(0xffffff, 0.3);
scene.add(ambientLight);

const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
directionalLight.position.set(50, 50, 50);
scene.add(directionalLight);

// Network Layouts
const networkLayouts = {
    GRID: 'grid',
    SPHERE: 'sphere',
    RANDOM: 'random'
};

// Initialize Visualization
function initVisualization() {
    updateCanvasSize();
    window.addEventListener('resize', updateCanvasSize);
    
    // Create starfield
    createStarfield();
    
    // Initialize packet pool
    initPacketPool();
    
    // Update stats display
    updateStatsDisplay();
    
    // Load previous data
    loadPreviousData();
    
    // Initialize controls
    initControls();
    
    // Start animation loop
    animate();
    
    // Try to connect automatically
    setTimeout(() => {
        connectWebSocket();
    }, 1000);
    
    // Set up keyboard shortcuts
    setupKeyboardShortcuts();
    
    console.log('✅ Visualization initialized');
}

function updateCanvasSize() {
    const canvasContainer = document.getElementById('visualization');
    const width = canvasContainer.clientWidth;
    const height = canvasContainer.clientHeight;
    
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
}

// Starfield Background
function createStarfield() {
    const vertices = [];
    for (let i = 0; i < 1000; i++) {
        const x = (Math.random() - 0.5) * 200;
        const y = (Math.random() - 0.5) * 200;
        const z = (Math.random() - 0.5) * 200;
        vertices.push(x, y, z);
    }
    
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(vertices, 3));
    
    const material = new THREE.PointsMaterial({
        color: 0xffffff,
        size: 0.1,
        transparent: true,
        opacity: 0.5
    });
    
    const stars = new THREE.Points(geometry, material);
    scene.add(stars);
    return stars;
}

// Packet Pool Management
function initPacketPool() {
    for (let i = 0; i < simulationState.maxPackets; i++) {
        const mesh = createPacketMesh(false);
        mesh.visible = false;
        scene.add(mesh);
        simulationState.packetPool.push(mesh);
    }
}

function getPacketMesh(isEncrypted) {
    if (simulationState.packetPool.length > 0) {
        const mesh = simulationState.packetPool.pop();
        mesh.material.color.setHex(isEncrypted ? 0x4facfe : 0x9d50bb);
        mesh.visible = true;
        return mesh;
    }
    return createPacketMesh(isEncrypted);
}

function returnPacketMesh(mesh) {
    mesh.visible = false;
    simulationState.packetPool.push(mesh);
}

function createPacketMesh(isEncrypted) {
    const geometry = new THREE.SphereGeometry(0.5, 8, 8);
    const material = new THREE.MeshBasicMaterial({
        color: isEncrypted ? 0x4facfe : 0x9d50bb,
        transparent: true,
        opacity: 0.8
    });
    return new THREE.Mesh(geometry, material);
}

// WebSocket Connection
function connectWebSocket() {
    const wsPort = 8080;
    const wsUrl = `ws://localhost:${wsPort}`;
    
    try {
        ws = new WebSocket(wsUrl);
        
        ws.onopen = () => {
            console.log('✅ Connected to NS-3 WebSocket server');
            simulationState.isConnected = true;
            reconnectAttempts = 0;
            updateConnectionStatus(true);
            addEventToLog('Connected to NS-3 simulation server', 'success');
            showNotification('Connected to NS-3 server', 'success');
        };
        
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                processSimulationEvent(data);
            } catch (error) {
                console.error('Error parsing WebSocket message:', error);
                addEventToLog('❌ Error parsing simulation data', 'error');
            }
        };
        
        ws.onclose = (event) => {
            console.log('❌ Disconnected from NS-3:', event.code, event.reason);
            simulationState.isConnected = false;
            updateConnectionStatus(false);
            
            if (reconnectAttempts < MAX_RECONNECTS) {
                reconnectAttempts++;
                const delay = RECONNECT_DELAY * Math.pow(1.5, reconnectAttempts - 1);
                setTimeout(() => {
                    console.log(`🔄 Reconnecting (attempt ${reconnectAttempts}/${MAX_RECONNECTS})...`);
                    addEventToLog(`Attempting to reconnect... (${reconnectAttempts}/${MAX_RECONNECTS})`, 'warning');
                    connectWebSocket();
                }, delay);
            } else {
                addEventToLog('Failed to connect to NS-3 server', 'error');
                showNotification('Failed to connect to NS-3 server', 'error');
            }
        };
        
        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            addEventToLog('WebSocket connection error', 'error');
        };
        
    } catch (error) {
        console.error('Failed to create WebSocket:', error);
        addEventToLog('Failed to create WebSocket connection', 'error');
    }
}

// Process Events from NS-3
function processSimulationEvent(event) {
    if (!event || !event.event) {
        console.warn('Invalid event received:', event);
        return;
    }
    
    const timestamp = new Date().toLocaleTimeString();
    console.log(`[${timestamp}] Event:`, event.event, event);
    
    switch(event.event) {
        case 'simulation_start':
            simulationState.simulationRunning = true;
            addEventToLog('Simulation started', 'success');
            break;
            
        case 'network_create':
            const nodeCount = event.from || 100;
            simulationState.stats.nodes = nodeCount;
            createNetworkNodes(nodeCount);
            addEventToLog(`Created network with ${nodeCount} nodes`, 'info');
            break;
            
        case 'packet_tx':
            simulationState.stats.packetsTx++;
            simulatePacket(event.from, event.to, event.packetId, false);
            addEventToLog(`📤 Packet ${event.packetId} from Node ${event.from} to ${event.to}`, 'info');
            break;
            
        case 'encrypt':
            simulationState.stats.encrypted++;
            simulatePacket(event.from, -1, event.packetId, true);
            addEventToLog(`🔒 Packet ${event.packetId} encrypted at Node ${event.from}`, 'crypto');
            break;
            
        case 'packet_rx':
            simulationState.stats.packetsRx++;
            addEventToLog(`📥 Packet ${event.packetId} received at Node ${event.to}`, 'success');
            break;
            
        case 'decrypt':
            simulationState.stats.decrypted++;
            addEventToLog(`🔓 Packet ${event.packetId} decrypted successfully`, 'crypto');
            break;
            
        case 'stats_update':
            if (event.nodes) simulationState.stats.nodes = event.nodes;
            if (event.packetsTx) simulationState.stats.packetsTx = event.packetsTx;
            if (event.packetsRx) simulationState.stats.packetsRx = event.packetsRx;
            if (event.pdr) simulationState.stats.pdr = event.pdr;
            if (event.energy) simulationState.stats.energy = (event.energy / 1000);
            if (event.throughput) simulationState.stats.throughput = event.throughput;
            break;
            
        case 'node_compromised':
            if (event.nodeId >= 0 && event.nodeId < simulationState.nodes.length) {
                simulationState.nodes[event.nodeId].type = 'compromised';
                simulationState.nodes[event.nodeId].mesh.material = materials.compromised;
                addEventToLog(`⚔️ Node ${event.nodeId} compromised!`, 'error');
            }
            break;
            
        case 'node_recovered':
            if (event.nodeId >= 0 && event.nodeId < simulationState.nodes.length) {
                simulationState.nodes[event.nodeId].type = 'normal';
                simulationState.nodes[event.nodeId].mesh.material = materials.normal;
                addEventToLog(`✅ Node ${event.nodeId} recovered`, 'success');
            }
            break;
            
        case 'simulation_complete':
            simulationState.simulationRunning = false;
            addEventToLog('Simulation completed successfully', 'success');
            showResults();
            saveSimulationData();
            break;
            
        case 'error':
            addEventToLog(`❌ Error: ${event.message || 'Unknown error'}`, 'error');
            break;
            
        default:
            console.log('Unhandled event:', event.event);
    }
    
    // Update display
    updateStatsDisplay();
    
    // Save event
    simulationState.events.push({
        timestamp: new Date().toISOString(),
        event: event.event,
        data: event
    });
}

// Create Network Nodes
function createNetworkNodes(nodeCount) {
    // Clear existing nodes
    simulationState.nodes.forEach(node => {
        scene.remove(node.mesh);
        node.mesh.traverse(child => {
            if (child.geometry) child.geometry.dispose();
            if (child.material) child.material.dispose();
        });
    });
    
    simulationState.connections.forEach(conn => {
        scene.remove(conn);
        conn.geometry.dispose();
        conn.material.dispose();
    });
    
    simulationState.nodes = [];
    simulationState.connections = [];
    
    // Create nodes based on layout
    switch(simulationState.networkLayout) {
        case networkLayouts.SPHERE:
            createSphericalNetwork(nodeCount);
            break;
        case networkLayouts.RANDOM:
            createRandomNetwork(nodeCount);
            break;
        default: // GRID
            createGridNetwork(nodeCount);
    }
    
    // Create connections
    createNetworkConnections();
    
    console.log(`✅ Created ${nodeCount} nodes with ${simulationState.networkLayout} layout`);
}

function createGridNetwork(nodeCount) {
    const gridSize = Math.ceil(Math.cbrt(nodeCount));
    const spacing = 10;
    
    for (let i = 0; i < nodeCount; i++) {
        const x = (i % gridSize) * spacing - (gridSize * spacing) / 2;
        const y = (Math.floor(i / gridSize) % gridSize) * spacing - (gridSize * spacing) / 2;
        const z = Math.floor(i / (gridSize * gridSize)) * spacing - (gridSize * spacing) / 2;
        
        const node = createNode(i, x, y, z);
        scene.add(node.mesh);
        simulationState.nodes.push(node);
    }
}

function createSphericalNetwork(nodeCount) {
    const radius = 25;
    
    for (let i = 0; i < nodeCount; i++) {
        const phi = Math.acos(-1 + (2 * i) / nodeCount);
        const theta = Math.sqrt(nodeCount * Math.PI) * phi;
        
        const x = radius * Math.sin(phi) * Math.cos(theta);
        const y = radius * Math.sin(phi) * Math.sin(theta);
        const z = radius * Math.cos(phi);
        
        const node = createNode(i, x, y, z);
        scene.add(node.mesh);
        simulationState.nodes.push(node);
    }
}

function createRandomNetwork(nodeCount) {
    const bounds = 30;
    
    for (let i = 0; i < nodeCount; i++) {
        const x = (Math.random() - 0.5) * bounds * 2;
        const y = (Math.random() - 0.5) * bounds * 2;
        const z = (Math.random() - 0.5) * bounds * 2;
        
        const node = createNode(i, x, y, z);
        scene.add(node.mesh);
        simulationState.nodes.push(node);
    }
}

function createNode(id, x, y, z) {
    // Determine node type
    let type = 'normal';
    if (id === 0) type = 'gateway'; // First node is gateway
    if (Math.random() < 0.05) type = 'compromised'; // 5% chance compromised
    
    const geometry = new THREE.SphereGeometry(1.5, 16, 16);
    const material = materials[type].clone();
    const mesh = new THREE.Mesh(geometry, material);
    
    mesh.position.set(x, y, z);
    
    // Add glow effect
    const glowGeometry = new THREE.SphereGeometry(2.0, 16, 16);
    const glowMaterial = new THREE.MeshBasicMaterial({
        color: material.emissive,
        transparent: true,
        opacity: 0.2,
        side: THREE.BackSide
    });
    const glow = new THREE.Mesh(glowGeometry, glowMaterial);
    mesh.add(glow);
    
    return {
        id: id,
        position: new THREE.Vector3(x, y, z),
        type: type,
        mesh: mesh,
        energy: 100,
        isActive: true
    };
}

function createNetworkConnections() {
    const maxDistance = 20;
    
    for (let i = 0; i < simulationState.nodes.length; i++) {
        for (let j = i + 1; j < simulationState.nodes.length; j++) {
            const nodeA = simulationState.nodes[i];
            const nodeB = simulationState.nodes[j];
            const distance = nodeA.position.distanceTo(nodeB.position);
            
            // Connect nodes that are close and have some randomness
            if (distance < maxDistance && Math.random() < 0.3) {
                const connection = createConnectionLine(nodeA.position, nodeB.position);
                scene.add(connection);
                simulationState.connections.push(connection);
            }
        }
    }
}

function createConnectionLine(pos1, pos2) {
    const geometry = new THREE.BufferGeometry().setFromPoints([pos1, pos2]);
    const material = new THREE.LineBasicMaterial({
        color: 0x667eea,
        transparent: true,
        opacity: 0.2,
        linewidth: 1
    });
    return new THREE.Line(geometry, material);
}

// Packet Simulation
function simulatePacket(fromNodeId, toNodeId, packetId, isEncrypted) {
    if (fromNodeId < 0 || fromNodeId >= simulationState.nodes.length) return;
    
    const fromNode = simulationState.nodes[fromNodeId];
    const toNode = toNodeId >= 0 && toNodeId < simulationState.nodes.length
        ? simulationState.nodes[toNodeId]
        : null;
    
    const targetPos = toNode ? toNode.position.clone() :
        fromNode.position.clone().add(new THREE.Vector3(
            (Math.random() - 0.5) * 30,
            (Math.random() - 0.5) * 30,
            (Math.random() - 0.5) * 30
        ));
    
    const packet = {
        id: packetId,
        from: fromNodeId,
        to: toNodeId,
        position: fromNode.position.clone(),
        target: targetPos,
        progress: 0,
        encrypted: isEncrypted,
        mesh: getPacketMesh(isEncrypted),
        startTime: Date.now(),
        trail: []
    };
    
    packet.mesh.position.copy(fromNode.position);
    packet.mesh.userData = { packetId };
    
    simulationState.packets.push(packet);
    
    // Create initial trail effect
    createPacketTrail(fromNode.position, isEncrypted);
    
    // Remove packet after animation
    setTimeout(() => {
        removePacket(packetId);
    }, 3000 / simulationState.simulationSpeed);
}

function createPacketTrail(position, isEncrypted) {
    const geometry = new THREE.SphereGeometry(0.3, 4, 4);
    const material = new THREE.MeshBasicMaterial({
        color: isEncrypted ? 0x4facfe : 0x9d50bb,
        transparent: true,
        opacity: 0.3
    });
    const trail = new THREE.Mesh(geometry, material);
    trail.position.copy(position);
    scene.add(trail);
    
    // Fade out and remove
    const fadeInterval = setInterval(() => {
        trail.material.opacity -= 0.05;
        if (trail.material.opacity <= 0) {
            scene.remove(trail);
            trail.geometry.dispose();
            trail.material.dispose();
            clearInterval(fadeInterval);
        }
    }, 100);
}

function removePacket(packetId) {
    const packetIndex = simulationState.packets.findIndex(p => p.id === packetId);
    if (packetIndex === -1) return;
    
    const packet = simulationState.packets[packetIndex];
    returnPacketMesh(packet.mesh);
    
    // Clean up trail
    packet.trail.forEach(trail => {
        if (trail.parent) scene.remove(trail);
        trail.geometry.dispose();
        trail.material.dispose();
    });
    
    simulationState.packets.splice(packetIndex, 1);
}

// Event Log
function addEventToLog(message, type = 'info') {
    const eventList = document.getElementById('event-list');
    const eventItem = document.createElement('div');
    eventItem.className = `event-item ${type}`;
    
    const time = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
    eventItem.innerHTML = `
        <span>${message}</span>
        <span class="event-time">${time}</span>
    `;
    
    eventList.appendChild(eventItem);
    eventList.scrollTop = eventList.scrollHeight;
    
    // Keep only last 100 events
    if (eventList.children.length > 100) {
        eventList.removeChild(eventList.firstChild);
    }
}

function showNotification(message, type = 'info') {
    // Create notification element
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    
    // Add to body
    document.body.appendChild(notification);
    
    // Remove after 3 seconds
    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => {
            if (notification.parentNode) {
                notification.parentNode.removeChild(notification);
            }
        }, 300);
    }, 3000);
}

// Update Stats Display
function updateStatsDisplay() {
    const stats = simulationState.stats;
    
    // Calculate PDR
    const pdr = stats.packetsTx > 0 ? (stats.packetsRx / stats.packetsTx * 100) : 0;
    const health = Math.min(100, pdr);
    
    // Calculate throughput based on recent activity
    if (simulationState.packets.length > 0) {
        const activePackets = simulationState.packets.length;
        stats.throughput = (activePackets * 0.1 * simulationState.simulationSpeed).toFixed(2);
    }
    
    // Update DOM elements
    document.getElementById('node-count').textContent = stats.nodes;
    document.getElementById('packets-tx').textContent = stats.packetsTx;
    document.getElementById('packets-rx').textContent = stats.packetsRx;
    document.getElementById('pdr-value').textContent = `${pdr.toFixed(1)}%`;
    document.getElementById('energy-value').textContent = `${stats.energy.toFixed(2)}J`;
    document.getElementById('throughput-value').textContent = stats.throughput;
    document.getElementById('health-percent').textContent = `${health.toFixed(0)}%`;
    document.getElementById('health-bar').style.width = `${health}%`;
    
    // Update health bar color based on health
    const healthBar = document.getElementById('health-bar');
    if (health > 70) {
        healthBar.style.background = 'linear-gradient(90deg, #43e97b, #4facfe)';
    } else if (health > 30) {
        healthBar.style.background = 'linear-gradient(90deg, #f093fb, #f5576c)';
    } else {
        healthBar.style.background = 'linear-gradient(90deg, #f093fb, #f5576c)';
    }
}

// Connection Status
function updateConnectionStatus(connected) {
    const indicator = document.getElementById('status-indicator');
    const statusText = document.getElementById('status-text');
    const connectBtn = document.getElementById('connect-btn');
    
    if (connected) {
        indicator.className = 'status-indicator status-connected';
        statusText.textContent = 'Connected to NS-3';
        connectBtn.textContent = '🔗 Connected';
        connectBtn.classList.add('active');
    } else {
        indicator.className = 'status-indicator status-disconnected';
        statusText.textContent = 'Disconnected from NS-3';
        connectBtn.textContent = '🔗 Connect to NS-3';
        connectBtn.classList.remove('active');
    }
}

// Show Results
function showResults() {
    addEventToLog('========================================', 'info');
    addEventToLog('📊 SIMULATION RESULTS', 'success');
    addEventToLog(`Total Nodes: ${simulationState.stats.nodes}`, 'info');
    addEventToLog(`Packets Transmitted: ${simulationState.stats.packetsTx}`, 'info');
    addEventToLog(`Packets Received: ${simulationState.stats.packetsRx}`, 'info');
    addEventToLog(`PDR: ${(simulationState.stats.packetsTx > 0 ?
        (simulationState.stats.packetsRx / simulationState.stats.packetsTx * 100).toFixed(2) : 0)}%`, 'info');
    addEventToLog(`Packets Encrypted: ${simulationState.stats.encrypted}`, 'crypto');
    addEventToLog(`Packets Decrypted: ${simulationState.stats.decrypted}`, 'crypto');
    addEventToLog(`Energy Consumption: ${simulationState.stats.energy.toFixed(2)}J`, 'info');
    addEventToLog('========================================', 'info');
}

// Data Persistence
function saveSimulationData() {
    const data = {
        timestamp: new Date().toISOString(),
        stats: simulationState.stats,
        events: simulationState.events.slice(-100)
    };
    localStorage.setItem('ns3_simulation_data', JSON.stringify(data));
    console.log('💾 Simulation data saved');
}

function loadPreviousData() {
    const saved = localStorage.getItem('ns3_simulation_data');
    if (saved) {
        try {
            const data = JSON.parse(saved);
            addEventToLog(`📁 Loaded previous simulation from ${new Date(data.timestamp).toLocaleString()}`, 'info');
        } catch (error) {
            console.error('Error loading previous data:', error);
        }
    }
}

// Animation Loop
function animate() {
    requestAnimationFrame(animate);
    
    const delta = 0.016 * simulationState.simulationSpeed; // Approximate 60fps
    
    // Update packet positions
    simulationState.packets.forEach(packet => {
        packet.progress += 0.02 * simulationState.simulationSpeed;
        if (packet.progress <= 1) {
            const newPos = packet.position.clone().lerp(packet.target, packet.progress);
            packet.mesh.position.copy(newPos);
            
            // Pulsing effect
            const scale = 1 + 0.3 * Math.sin(Date.now() * 0.005 + packet.id);
            packet.mesh.scale.setScalar(scale);
            
            // Fade out near end
            if (packet.progress > 0.8) {
                packet.mesh.material.opacity = 0.8 * (1 - packet.progress);
            }
            
            // Create trail at intervals
            if (Math.random() < 0.3 * simulationState.simulationSpeed) {
                createPacketTrail(newPos, packet.encrypted);
            }
        }
    });
    
    // Update node animations
    simulationState.nodes.forEach((node, index) => {
        const scale = 1 + 0.1 * Math.sin(Date.now() * 0.001 + index);
        node.mesh.scale.setScalar(scale);
        
        // Gateway nodes pulse stronger
        if (node.type === 'gateway') {
            node.mesh.children[0].scale.setScalar(1.5 + 0.5 * Math.sin(Date.now() * 0.002));
        }
        
        // Compromised nodes flash red
        if (node.type === 'compromised') {
            node.mesh.material.emissiveIntensity = 0.3 + 0.2 * Math.sin(Date.now() * 0.003);
        }
    });
    
    // Update connection animations
    simulationState.connections.forEach((conn, index) => {
        const material = conn.material;
        material.opacity = 0.1 + 0.1 * Math.sin(Date.now() * 0.001 + index);
    });
    
    // Update controls
    controls.update();
    
    // Render scene
    renderer.render(scene, camera);
}

// Initialize Controls
function initControls() {
    // Connect button
    document.getElementById('connect-btn').addEventListener('click', () => {
        connectWebSocket();
    });
    
    // Start simulation button
    document.getElementById('start-btn').addEventListener('click', () => {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                command: 'start_simulation',
                crypto_enabled: simulationState.cryptoEnabled
            }));
            addEventToLog('📡 Sent start command to NS-3', 'success');
            showNotification('Simulation started', 'success');
        } else {
            addEventToLog('❌ Not connected to NS-3', 'error');
            showNotification('Please connect to NS-3 first', 'warning');
        }
    });
    
    // Crypto toggle button
    document.getElementById('crypto-btn').addEventListener('click', () => {
        simulationState.cryptoEnabled = !simulationState.cryptoEnabled;
        const btn = document.getElementById('crypto-btn');
        btn.textContent = simulationState.cryptoEnabled ? '🔐 ASCON Enabled' : '🔓 Crypto Disabled';
        btn.classList.toggle('active', simulationState.cryptoEnabled);
        
        if (simulationState.cryptoEnabled) {
            addEventToLog('🔐 ASCON-128 Cryptography enabled', 'crypto');
            showNotification('ASCON encryption enabled', 'success');
        } else {
            addEventToLog('🔓 Cryptography disabled', 'warning');
            showNotification('Encryption disabled', 'warning');
        }
    });
    
    // Attack simulation button
    document.getElementById('attack-btn').addEventListener('click', () => {
        if (simulationState.nodes.length > 0) {
            const randomNode = Math.floor(Math.random() * simulationState.nodes.length);
            
            // Send attack command to NS-3 if connected
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'simulate_attack',
                    node_id: randomNode
                }));
            }
            
            // Update visualization
            simulationState.nodes[randomNode].type = 'compromised';
            simulationState.nodes[randomNode].mesh.material = materials.compromised;
            
            addEventToLog(`⚔️ Node ${randomNode} compromised!`, 'error');
            showNotification(`Node ${randomNode} attacked!`, 'error');
        }
    });
    
    // Reset view button
    document.getElementById('reset-btn').addEventListener('click', () => {
        camera.position.set(50, 50, 50);
        controls.reset();
        addEventToLog('🔄 Camera reset', 'info');
    });
    
    // Speed control
    const speedSlider = document.getElementById('speed-slider');
    const speedValue = document.getElementById('speed-value');
    
    speedSlider.addEventListener('input', (e) => {
        simulationState.simulationSpeed = parseFloat(e.target.value);
        speedValue.textContent = `${simulationState.simulationSpeed}x`;
    });
    
    // Network layout selector
    const layoutSelect = document.createElement('select');
    layoutSelect.className = 'layout-select';
    layoutSelect.innerHTML = `
        <option value="GRID">Grid Layout</option>
        <option value="SPHERE">Sphere Layout</option>
        <option value="RANDOM">Random Layout</option>
    `;
    layoutSelect.value = simulationState.networkLayout;
    
    layoutSelect.addEventListener('change', (e) => {
        simulationState.networkLayout = e.target.value;
        if (simulationState.nodes.length > 0) {
            createNetworkNodes(simulationState.nodes.length);
            addEventToLog(`Network layout changed to ${e.target.value}`, 'info');
        }
    });
    
    // Add layout selector to controls panel
    const controlsPanel = document.getElementById('controls');
    const speedControl = document.createElement('div');
    speedControl.className = 'speed-control';
    speedControl.innerHTML = `
        <label>Simulation Speed:</label>
        <input type="range" id="speed-slider" min="0.1" max="5" step="0.1" value="1">
        <span id="speed-value">1x</span>
    `;
    
    const layoutControl = document.createElement('div');
    layoutControl.className = 'layout-control';
    layoutControl.innerHTML = `
        <label>Network Layout:</label>
    `;
    layoutControl.appendChild(layoutSelect);
    
    controlsPanel.appendChild(speedControl);
    controlsPanel.appendChild(layoutControl);
}

// Keyboard Shortcuts
function setupKeyboardShortcuts() {
    document.addEventListener('keydown', (e) => {
        // Don't trigger if user is typing in an input
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
        
        switch(e.key) {
            case ' ':
                // Space to start/stop simulation
                e.preventDefault();
                document.getElementById('start-btn').click();
                break;
            case 'c':
            case 'C':
                e.preventDefault();
                document.getElementById('connect-btn').click();
                break;
            case 'e':
            case 'E':
                e.preventDefault();
                document.getElementById('crypto-btn').click();
                break;
            case 'a':
            case 'A':
                e.preventDefault();
                document.getElementById('attack-btn').click();
                break;
            case 'r':
            case 'R':
                e.preventDefault();
                document.getElementById('reset-btn').click();
                break;
            case 'Escape':
                e.preventDefault();
                camera.position.set(50, 50, 50);
                controls.reset();
                break;
            case '+':
            case '=':
                e.preventDefault();
                const speedSlider = document.getElementById('speed-slider');
                if (speedSlider) {
                    let speed = parseFloat(speedSlider.value) + 0.1;
                    speed = Math.min(5, speed);
                    speedSlider.value = speed;
                    speedSlider.dispatchEvent(new Event('input'));
                }
                break;
            case '-':
            case '_':
                e.preventDefault();
                const speedSlider2 = document.getElementById('speed-slider');
                if (speedSlider2) {
                    let speed = parseFloat(speedSlider2.value) - 0.1;
                    speed = Math.max(0.1, speed);
                    speedSlider2.value = speed;
                    speedSlider2.dispatchEvent(new Event('input'));
                }
                break;
        }
    });
}

// Export simulation data
function exportSimulationData() {
    const data = {
        timestamp: new Date().toISOString(),
        simulationState: simulationState,
        stats: simulationState.stats,
        events: simulationState.events
    };
    
    const dataStr = JSON.stringify(data, null, 2);
    const dataBlob = new Blob([dataStr], {type: 'application/json'});
    const url = URL.createObjectURL(dataBlob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = `ns3-simulation-${new Date().toISOString().slice(0, 19)}.json`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    addEventToLog('💾 Simulation data exported', 'success');
    showNotification('Simulation data exported', 'success');
}

// Import simulation data
function importSimulationData(file) {
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            const data = JSON.parse(e.target.result);
            // Implement data import logic here
            addEventToLog('📁 Simulation data imported', 'success');
            showNotification('Simulation data imported', 'success');
        } catch (error) {
            addEventToLog('❌ Error importing data', 'error');
            showNotification('Error importing data', 'error');
        }
    };
    reader.readAsText(file);
}

// Add export/import buttons
function addDataButtons() {
    const exportBtn = document.createElement('button');
    exportBtn.className = 'control-btn';
    exportBtn.textContent = '💾 Export Data';
    exportBtn.addEventListener('click', exportSimulationData);
    
    const importBtn = document.createElement('button');
    importBtn.className = 'control-btn';
    importBtn.textContent = '📁 Import Data';
    
    const fileInput = document.createElement('input');
    fileInput.type = 'file';
    fileInput.accept = '.json';
    fileInput.style.display = 'none';
    fileInput.addEventListener('change', (e) => {
        if (e.target.files[0]) {
            importSimulationData(e.target.files[0]);
        }
    });
    
    importBtn.addEventListener('click', () => {
        fileInput.click();
    });
    
    const controlsPanel = document.getElementById('controls');
    controlsPanel.appendChild(exportBtn);
    controlsPanel.appendChild(importBtn);
    controlsPanel.appendChild(fileInput);
}

// Initialize everything when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    // Add notification CSS
    const style = document.createElement('style');
    style.textContent = `
        .notification {
            position: fixed;
            bottom: 20px;
            right: 20px;
            padding: 12px 20px;
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(20px);
            border-radius: 12px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            color: white;
            font-family: 'Outfit', sans-serif;
            font-size: 14px;
            z-index: 1000;
            transform: translateX(100%);
            opacity: 0;
            animation: slideIn 0.3s forwards;
        }
        
        .notification.success {
            border-left: 4px solid #43e97b;
        }
        
        .notification.error {
            border-left: 4px solid #f857a6;
        }
        
        .notification.warning {
            border-left: 4px solid #9d50bb;
        }
        
        .notification.info {
            border-left: 4px solid #667eea;
        }
        
        .notification.crypto {
            border-left: 4px solid #4facfe;
        }
        
        .notification.fade-out {
            animation: slideOut 0.3s forwards;
        }
        
        @keyframes slideIn {
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
        
        @keyframes slideOut {
            to {
                transform: translateX(100%);
                opacity: 0;
            }
        }
        
        .layout-select {
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.1);
            border-radius: 8px;
            color: white;
            padding: 8px 12px;
            font-family: 'Outfit', sans-serif;
            font-size: 12px;
            outline: none;
            cursor: pointer;
            width: 100%;
            margin-top: 8px;
        }
        
        .layout-select option {
            background: #0a0a14;
            color: white;
        }
        
        .layout-control {
            margin-top: 12px;
            padding: 12px;
            background: rgba(255, 255, 255, 0.02);
            border-radius: 12px;
        }
        
        .layout-control label {
            font-size: 12px;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 1px;
            display: block;
            margin-bottom: 8px;
        }
    `;
    document.head.appendChild(style);
    
    // Add data buttons
    addDataButtons();
    
    // Initialize visualization
    initVisualization();
});

// Error handling for Three.js
window.addEventListener('error', (event) => {
    console.error('Global error:', event.error);
    addEventToLog(`❌ Error: ${event.message}`, 'error');
});

// Handle page unload
window.addEventListener('beforeunload', () => {
    saveSimulationData();
    
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    }
    
    // Clean up Three.js resources
    scene.traverse(object => {
        if (object.geometry) object.geometry.dispose();
        if (object.material) {
            if (object.material.map) object.material.map.dispose();
            if (Array.isArray(object.material)) {
                object.material.forEach(material => material.dispose());
            } else {
                object.material.dispose();
            }
        }
    });
});
</script>
