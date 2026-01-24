// server-enhanced.js - Enhanced NS-3 WebSocket Bridge Server with Node Resilience
const WebSocket = require("ws");
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');
const http = require('http');

console.log("🚀 Starting Enhanced NS-3 WebSocket Bridge Server with Node Resilience");
console.log("=".repeat(70));

// Configuration
const config = {
  ports: [8080, 8081, 8082, 8083, 8084],
  simulationCommand: 'cd .. && ./ns3 run memostp-enhanced-with-node-death 2>&1',
  simulationTimeout: 300000, // 5 minutes
  dataLogFile: 'simulation_data.json',
  keepServerRunning: true,
  maxClients: 20,
  healthCheckPort: 8085,
  dashboardPort: 8086
};

// Try multiple ports
let wss = null;
let selectedPort = null;

for (const port of config.ports) {
  try {
    wss = new WebSocket.Server({ port });
    selectedPort = port;
    console.log(`✅ WebSocket server running on ws://localhost:${port}`);
    break;
  } catch (error) {
    if (error.code === 'EADDRINUSE') {
      console.log(`Port ${port} busy, trying next...`);
      continue;
    }
    throw error;
  }
}

if (!wss) {
  console.error("❌ Could not start server on any port! Exiting.");
  process.exit(1);
}

// Store connected clients, simulation state, and node data
const clients = new Map(); // Map client id -> {ws, metadata}
const nodeStates = new Map(); // Map nodeId -> {energy, alive, packets, etc.}
let isSimulationRunning = false;
let simulationStartTime = null;
let eventCount = 0;
let ns3Process = null;
let statusInterval = null;

let simulationData = {
  startTime: null,
  endTime: null,
  totalEvents: 0,
  events: [],
  stats: {
    packets: { tx: 0, rx: 0, encrypted: 0, decrypted: 0, dropped: 0 },
    nodes: { total: 0, alive: 0, dead: 0 },
    energy: { total: 0, perNode: 0, efficiency: 0 },
    pdr: 0,
    throughput: 0,
    delay: 0,
    networkLifetime: 0,
    resilience: { routeChanges: 0, recoveries: 0, successRate: 0 }
  },
  nodeHistory: []
};

// Generate unique client ID
function generateClientId() {
  return 'client_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
}

// Initialize node states
function initNodeStates(nodeCount) {
  nodeStates.clear();
  for (let i = 0; i < nodeCount; i++) {
    nodeStates.set(i, {
      id: i,
      alive: true,
      energy: 100,
      initialEnergy: 100,
      packetsTx: 0,
      packetsRx: 0,
      position: { x: 0, y: 0 },
      deathTime: null,
      color: getNodeColor(i)
    });
  }
  simulationData.stats.nodes.total = nodeCount;
  simulationData.stats.nodes.alive = nodeCount;
  simulationData.stats.nodes.dead = 0;
  
  console.log(`📊 Initialized ${nodeCount} node states`);
}

// Get node color based on ID
function getNodeColor(nodeId) {
  const colors = [
    '#FF6B6B', '#4ECDC4', '#FFD166', '#06D6A0', '#118AB2',
    '#EF476F', '#FFD166', '#06D6A0', '#073B4C', '#7209B7',
    '#3A86FF', '#FB5607', '#8338EC', '#FF006E', '#FFBE0B',
    '#3A86FF', '#FB5607', '#8338EC', '#FF006E', '#FFBE0B',
    '#FF6B6B', '#4ECDC4', '#FFD166', '#06D6A0', '#118AB2'
  ];
  return colors[nodeId % colors.length];
}

// Initialize data logging
function initDataLogging() {
  simulationData.startTime = Date.now();
  simulationData.events = [];
  simulationData.nodeHistory = [];

  // Clear old log file
  if (fs.existsSync(config.dataLogFile)) {
    fs.unlinkSync(config.dataLogFile);
  }

  console.log("📊 Enhanced data logging initialized");
}

// Save event to log file
function saveEventToLog(event) {
  simulationData.events.push(event);
  
  // Update node states based on events
  updateNodeStatesFromEvent(event);

  // Save to file periodically or when simulation ends
  if (eventCount % 100 === 0 || event.event === 'simulation_complete') {
    fs.writeFileSync(config.dataLogFile, JSON.stringify(simulationData, null, 2));
  }
}

// Update node states from events
function updateNodeStatesFromEvent(event) {
  switch (event.event) {
    case 'node_energy_initialized':
      if (nodeStates.has(event.from)) {
        nodeStates.get(event.from).initialEnergy = event.value;
        nodeStates.get(event.from).energy = event.value;
      }
      break;
      
    case 'node_energy_update':
      if (nodeStates.has(event.from)) {
        nodeStates.get(event.from).energy = event.value;
      }
      break;
      
    case 'node_died':
      if (nodeStates.has(event.from)) {
        const node = nodeStates.get(event.from);
        node.alive = false;
        node.deathTime = event.time || Date.now();
        node.color = '#666666'; // Gray for dead nodes
        
        simulationData.stats.nodes.alive--;
        simulationData.stats.nodes.dead++;
        
        // Record node death in history
        simulationData.nodeHistory.push({
          time: event.time,
          nodeId: event.from,
          event: 'death',
          reason: event.info
        });
      }
      break;
      
    case 'packet_tx':
      if (nodeStates.has(event.from)) {
        nodeStates.get(event.from).packetsTx++;
      }
      break;
      
    case 'packet_rx':
      if (nodeStates.has(event.to)) {
        nodeStates.get(event.to).packetsRx++;
      }
      break;
      
    case 'network_health':
      simulationData.stats.nodes.alive = event.from || 0;
      simulationData.stats.nodes.dead = simulationData.stats.nodes.total - event.from;
      break;
  }
}

// Broadcast to specific client
function sendToClient(clientId, data) {
  const client = clients.get(clientId);
  if (client && client.ws.readyState === WebSocket.OPEN) {
    client.ws.send(JSON.stringify(data));
  }
}

// Broadcast to all clients
function broadcast(data, excludeClientId = null) {
  const message = JSON.stringify(data);
  clients.forEach((client, clientId) => {
    if (clientId !== excludeClientId && client.ws.readyState === WebSocket.OPEN) {
      client.ws.send(message);
    }
  });
}

// Broadcast to specific client type
function broadcastToType(clientType, data) {
  const message = JSON.stringify(data);
  clients.forEach((client, clientId) => {
    if (client.metadata.type === clientType && client.ws.readyState === WebSocket.OPEN) {
      client.ws.send(message);
    }
  });
}

// Update simulation stats
function updateStats(event) {
  switch(event.event) {
    case 'network_create':
      simulationData.stats.nodes.total = event.from || 0;
      simulationData.stats.nodes.alive = event.from || 0;
      initNodeStates(event.from || 0);
      break;
      
    case 'packet_tx':
      simulationData.stats.packets.tx++;
      break;
      
    case 'packet_rx':
      simulationData.stats.packets.rx++;
      break;
      
    case 'encrypt':
      simulationData.stats.packets.encrypted++;
      break;
      
    case 'decrypt':
      simulationData.stats.packets.decrypted++;
      break;
      
    case 'decrypt_failed':
    case 'encryption_failed':
      simulationData.stats.packets.dropped++;
      break;
      
    case 'stats_packets':
      simulationData.stats.packets.tx = event.from || 0;
      simulationData.stats.packets.rx = event.to || 0;
      break;
      
    case 'stats_pdr':
      simulationData.stats.pdr = event.from || 0;
      break;
      
    case 'stats_energy':
      simulationData.stats.energy.total = (event.from || 0) / 1000;
      break;
      
    case 'stats_throughput':
      simulationData.stats.throughput = (event.from || 0) / 1000;
      break;
      
    case 'stats_delay':
      simulationData.stats.delay = (event.from || 0) / 1000;
      break;
      
    case 'stats_alive_nodes':
      simulationData.stats.nodes.alive = event.from || 0;
      simulationData.stats.nodes.dead = (event.to || 0) - (event.from || 0);
      break;
      
    case 'stats_network_lifetime':
      simulationData.stats.networkLifetime = event.from || 0;
      break;
      
    case 'stats_dead_nodes':
      simulationData.stats.nodes.dead = event.from || 0;
      break;
      
    case 'route_recovery':
      if (event.info === 'success') {
        simulationData.stats.resilience.recoveries++;
      }
      simulationData.stats.resilience.routeChanges++;
      break;
  }

  // Calculate derived metrics
  if (simulationData.stats.packets.tx > 0) {
    simulationData.stats.pdr = (simulationData.stats.packets.rx / simulationData.stats.packets.tx * 100).toFixed(2);
  }
  
  if (simulationData.stats.nodes.total > 0) {
    simulationData.stats.energy.perNode = simulationData.stats.energy.total / simulationData.stats.nodes.total;
  }
  
  if (simulationData.stats.resilience.routeChanges > 0) {
    simulationData.stats.resilience.successRate = 
      (simulationData.stats.resilience.recoveries / simulationData.stats.resilience.routeChanges * 100).toFixed(2);
  }
  
  if (simulationData.stats.energy.total > 0) {
    simulationData.stats.energy.efficiency = 
      (simulationData.stats.packets.rx / simulationData.stats.energy.total).toFixed(2);
  }
}

// Handle WebSocket connections
wss.on("connection", (ws, req) => {
  const clientId = generateClientId();
  const clientIp = req.socket.remoteAddress;

  console.log(`📱 New client connected: ${clientId} from ${clientIp}`);

  // Store client with metadata
  clients.set(clientId, {
    ws: ws,
    metadata: {
      id: clientId,
      ip: clientIp,
      connectedAt: Date.now(),
      type: 'visualization',
      subscribedEvents: ['all']
    }
  });

  // Send welcome message with complete state
  const welcomeData = {
    type: "system",
    event: "client_connected",
    message: "Connected to Enhanced NS-3 Simulation Server",
    clientId: clientId,
    serverPort: selectedPort,
    serverTime: Date.now(),
    simulationStatus: isSimulationRunning ? "running" : "idle",
    connectedClients: clients.size,
    simulationStats: simulationData.stats,
    nodeStates: Array.from(nodeStates.values()),
    simulationData: {
      startTime: simulationData.startTime,
      eventCount: eventCount
    }
  };

  ws.send(JSON.stringify(welcomeData));

  // Send current node states if available
  if (nodeStates.size > 0) {
    ws.send(JSON.stringify({
      type: "node_states",
      nodes: Array.from(nodeStates.values()),
      timestamp: Date.now()
    }));
  }

  // Handle client messages
  ws.on("message", (message) => {
    try {
      const data = JSON.parse(message.toString());
      handleClientMessage(clientId, data);
    } catch (error) {
      console.error(`Error processing message from ${clientId}:`, error.message);
      sendToClient(clientId, {
        type: "error",
        message: "Invalid message format",
        timestamp: Date.now()
      });
    }
  });

  // Handle client disconnect
  ws.on("close", () => {
    console.log(`📱 Client disconnected: ${clientId}`);
    clients.delete(clientId);

    // Notify other clients
    broadcast({
      type: "system",
      event: "client_disconnected",
      clientId: clientId,
      connectedClients: clients.size,
      timestamp: Date.now()
    }, clientId);

    // If no clients left and not keeping server running
    if (clients.size === 0 && !config.keepServerRunning && !isSimulationRunning) {
      console.log("⚠️  All clients disconnected. Server will shut down in 30 seconds...");
      setTimeout(() => {
        console.log("👋 Server shutting down");
        process.exit(0);
      }, 30000);
    }
  });

  // Handle errors
  ws.on("error", (error) => {
    console.error(`WebSocket error for client ${clientId}:`, error.message);
  });
});

// Handle client commands
function handleClientMessage(clientId, data) {
  const client = clients.get(clientId);

  console.log(`📨 Received from ${clientId}: ${data.command || data.type || 'message'}`);

  switch(data.command || data.type) {
    case 'start_simulation':
      if (!isSimulationRunning) {
        startSimulation(clientId);
      } else {
        sendToClient(clientId, {
          type: "system",
          event: "simulation_already_running",
          message: "Simulation is already running",
          timestamp: Date.now()
        });
      }
      break;

    case 'stop_simulation':
      stopSimulation(clientId);
      break;

    case 'get_status':
      sendToClient(clientId, {
        type: "status",
        simulationRunning: isSimulationRunning,
        simulationStartTime: simulationStartTime,
        eventCount: eventCount,
        connectedClients: clients.size,
        stats: simulationData.stats,
        nodeStates: Array.from(nodeStates.values()),
        timestamp: Date.now()
      });
      break;

    case 'get_stats':
      sendToClient(clientId, {
        type: "stats",
        stats: simulationData.stats,
        timestamp: Date.now()
      });
      break;
      
    case 'get_node_states':
      sendToClient(clientId, {
        type: "node_states",
        nodes: Array.from(nodeStates.values()),
        timestamp: Date.now()
      });
      break;
      
    case 'get_node_history':
      const nodeId = data.nodeId;
      const nodeHistory = simulationData.nodeHistory.filter(record => record.nodeId === nodeId);
      sendToClient(clientId, {
        type: "node_history",
        nodeId: nodeId,
        history: nodeHistory,
        timestamp: Date.now()
      });
      break;

    case 'client_type':
      if (client) {
        client.metadata.type = data.clientType || 'visualization';
        client.metadata.subscribedEvents = data.subscribedEvents || ['all'];
        console.log(`Client ${clientId} set type to: ${client.metadata.type}`);
      }
      break;
      
    case 'subscribe':
      if (client && data.events) {
        client.metadata.subscribedEvents = data.events;
        console.log(`Client ${clientId} subscribed to: ${data.events.join(', ')}`);
      }
      break;

    case 'ping':
      sendToClient(clientId, {
        type: "pong",
        timestamp: Date.now(),
        serverTime: Date.now()
      });
      break;
      
    case 'kill_node':
      if (isSimulationRunning && ns3Process) {
        const nodeId = data.nodeId;
        // Send kill command to simulation (would need IPC mechanism)
        console.log(`Received request to kill node ${nodeId}`);
        sendToClient(clientId, {
          type: "system",
          event: "node_kill_requested",
          nodeId: nodeId,
          timestamp: Date.now()
        });
      }
      break;

    default:
      console.log(`Unknown command from ${clientId}:`, data.command || data.type);
  }
}

// Start simulation
function startSimulation(initiatorClientId = null) {
  console.log("🎬 Starting enhanced NS-3 simulation with node resilience...");
  isSimulationRunning = true;
  simulationStartTime = Date.now();
  eventCount = 0;

  // Initialize data logging and node states
  initDataLogging();

  // Notify all clients
  broadcast({
    type: "system",
    event: "simulation_starting",
    message: "Starting enhanced NS-3 MEMOSTP simulation with node resilience...",
    initiator: initiatorClientId,
    startTime: simulationStartTime,
    timestamp: Date.now()
  });

  // Start NS-3 simulation with timeout
  ns3Process = exec(config.simulationCommand, {
    timeout: config.simulationTimeout,
    maxBuffer: 1024 * 1024 * 20 // 20MB buffer for larger logs
  });

  // Handle process completion
  ns3Process.on('exit', (code, signal) => {
    isSimulationRunning = false;
    simulationData.endTime = Date.now();
    simulationData.totalEvents = eventCount;

    console.log(`✅ Simulation process exited with code ${code}`);
    
    // Clear status interval
    if (statusInterval) {
      clearInterval(statusInterval);
      statusInterval = null;
    }

    // Save final data to log
    fs.writeFileSync(config.dataLogFile, JSON.stringify(simulationData, null, 2));

    // Notify clients
    broadcast({
      type: "system",
      event: "simulation_completed",
      message: `Simulation completed. Total events: ${eventCount}`,
      exitCode: code,
      signal: signal,
      duration: simulationData.endTime - simulationData.startTime,
      totalEvents: eventCount,
      stats: simulationData.stats,
      timestamp: Date.now()
    });

    // Broadcast final stats and node states
    broadcast({
      type: "stats_summary",
      stats: simulationData.stats,
      nodeStates: Array.from(nodeStates.values()),
      timestamp: Date.now()
    });

    // Generate performance report
    generatePerformanceReport();

    // If not keeping server running and no clients, exit
    if (!config.keepServerRunning && clients.size === 0) {
      setTimeout(() => {
        console.log("👋 Server shutting down");
        process.exit(0);
      }, 10000);
    }
  });

  // Handle errors
  ns3Process.on('error', (error) => {
    console.error(`❌ Simulation error: ${error.message}`);
    isSimulationRunning = false;
    
    if (statusInterval) {
      clearInterval(statusInterval);
      statusInterval = null;
    }

    broadcast({
      type: "system",
      event: "simulation_error",
      message: `Simulation error: ${error.message}`,
      timestamp: Date.now()
    });
  });

  // Process NS-3 output in real-time
  ns3Process.stdout.on('data', (data) => {
    const lines = data.toString().split('\n');
    lines.forEach(line => {
      processLine(line.trim());
    });
  });

  ns3Process.stderr.on('data', (data) => {
    const errorLine = data.toString().trim();
    if (errorLine && !errorLine.includes('Waf:')) {
      console.error(`NS-3 Error: ${errorLine}`);

      if (errorLine.includes('ERROR') || errorLine.includes('Assert')) {
        broadcast({
          type: "error",
          event: "ns3_error",
          message: errorLine,
          timestamp: Date.now()
        });
      }
    }
  });

  // Send periodic simulation status updates
  statusInterval = setInterval(() => {
    if (!isSimulationRunning) {
      clearInterval(statusInterval);
      return;
    }

    const statusUpdate = {
      type: "status_update",
      simulationRunning: isSimulationRunning,
      elapsedTime: Date.now() - simulationStartTime,
      eventCount: eventCount,
      stats: simulationData.stats,
      nodeStates: Array.from(nodeStates.values()),
      timestamp: Date.now()
    };

    broadcast(statusUpdate);
    
    // Send node states specifically to visualization clients
    broadcastToType('visualization', {
      type: "node_states_update",
      nodes: Array.from(nodeStates.values()),
      timestamp: Date.now()
    });
  }, 2000); // Every 2 seconds for smoother updates
}

// Stop simulation
function stopSimulation(initiatorClientId) {
  if (!isSimulationRunning || !ns3Process) {
    sendToClient(initiatorClientId, {
      type: "system",
      event: "simulation_not_running",
      message: "No simulation is currently running",
      timestamp: Date.now()
    });
    return;
  }

  console.log("🛑 Stopping simulation...");
  
  // Kill the NS-3 process
  ns3Process.kill('SIGTERM');
  
  sendToClient(initiatorClientId, {
    type: "system",
    event: "simulation_stopped",
    message: "Simulation stopped by user request",
    timestamp: Date.now()
  });
}

// Process simulation output line
function processLine(line) {
  if (!line) return;

  try {
    // Check if it's JSON
    if (line.startsWith('{') && line.endsWith('}')) {
      const event = JSON.parse(line);
      eventCount++;

      // Add server metadata
      event._id = eventCount;
      event._serverTime = Date.now();
      event._sequence = eventCount;

      // Update stats
      updateStats(event);

      // Save to log
      saveEventToLog(event);

      // Broadcast to all clients
      broadcast(event);

      // Send specialized events to specific client types
      if (event.event.includes('node')) {
        broadcastToType('monitoring', {
          type: "node_event",
          original: event,
          timestamp: Date.now()
        });
        
        // Update visualization clients with node states
        broadcastToType('visualization', {
          type: "node_update",
          event: event,
          nodeStates: Array.from(nodeStates.values()),
          timestamp: Date.now()
        });
      }

      if (event.event.includes('encrypt') || event.event.includes('decrypt')) {
        broadcastToType('monitoring', {
          type: "crypto_event",
          original: event,
          timestamp: Date.now()
        });
      }

      // Log progress
      if (eventCount % 50 === 0) {
        console.log(`📨 Processed ${eventCount} events (latest: ${event.event})`);

        // Send progress update
        broadcast({
          type: "progress",
          eventCount: eventCount,
          timestamp: Date.now()
        });
      }
    } else {
      // Process non-JSON output
      if (line.includes("SIMULATION") || line.includes("RESULT") ||
          line.includes("ERROR") || line.includes("WARNING") ||
          line.includes("Node") || line.includes("Energy")) {
        
        console.log(`📝 ${line}`);

        // Send important messages to clients
        if (line.includes("SIMULATION COMPLETE") ||
            line.includes("ENHANCED MEMOSTP") ||
            line.includes("Node died") ||
            line.includes("Energy:")) {
          
          broadcast({
            type: "system",
            event: "simulation_output",
            message: line,
            timestamp: Date.now()
          });
        }

        // Parse performance metrics from console output
        const metrics = extractMetricsFromLine(line);
        if (metrics) {
          broadcast({
            type: "performance_metrics",
            metrics: metrics,
            timestamp: Date.now()
          });
        }
      }
    }
  } catch (error) {
    // Not JSON or parse error - log if it looks important
    if (line.length > 10 && !line.includes('Waf:') && !line.includes('build/')) {
      console.log(`📝 Raw output: ${line.substring(0, 100)}...`);
    }
  }
}

// Extract metrics from console output
function extractMetricsFromLine(line) {
  const metrics = {};

  // Extract PDR
  const pdrMatch = line.match(/PDR:\s*([\d.]+)%/);
  if (pdrMatch) metrics.pdr = parseFloat(pdrMatch[1]);

  // Extract Throughput
  const throughputMatch = line.match(/Throughput:\s*([\d.]+)\s*Mbps/);
  if (throughputMatch) metrics.throughput = parseFloat(throughputMatch[1]);

  // Extract Energy
  const energyMatch = line.match(/Energy.*?([\d.]+)\s*J/);
  if (energyMatch) metrics.energy = parseFloat(energyMatch[1]);

  // Extract Delay
  const delayMatch = line.match(/Delay:\s*([\d.]+)\s*s/);
  if (delayMatch) metrics.delay = parseFloat(delayMatch[1]);

  // Extract alive nodes
  const aliveMatch = line.match(/Alive Nodes:\s*(\d+)\/(\d+)/);
  if (aliveMatch) {
    metrics.aliveNodes = parseInt(aliveMatch[1]);
    metrics.totalNodes = parseInt(aliveMatch[2]);
  }

  // Extract dead nodes
  const deadMatch = line.match(/Dead Nodes:\s*(\d+)/);
  if (deadMatch) metrics.deadNodes = parseInt(deadMatch[1]);

  return Object.keys(metrics).length > 0 ? metrics : null;
}

// Generate performance report
function generatePerformanceReport() {
  const report = {
    timestamp: Date.now(),
    simulationDuration: simulationData.endTime - simulationData.startTime,
    summary: {
      networkAvailability: ((simulationData.stats.nodes.alive / simulationData.stats.nodes.total) * 100).toFixed(2) + '%',
      packetDeliveryRate: simulationData.stats.pdr + '%',
      networkLifetime: simulationData.stats.networkLifetime + 's',
      energyEfficiency: simulationData.stats.energy.efficiency + ' packets/J',
      resilienceScore: ((simulationData.stats.nodes.alive / simulationData.stats.nodes.total) * simulationData.stats.pdr / 100).toFixed(2)
    },
    details: simulationData.stats
  };

  // Save report to file
  const reportFile = 'performance_report_' + Date.now() + '.json';
  fs.writeFileSync(reportFile, JSON.stringify(report, null, 2));
  
  console.log(`📄 Performance report saved to: ${reportFile}`);
  
  // Broadcast report to clients
  broadcast({
    type: "performance_report",
    report: report,
    timestamp: Date.now()
  });
}

// Handle server shutdown gracefully
function shutdownServer() {
  console.log("\n🛑 Server shutdown requested...");

  // Stop simulation if running
  if (isSimulationRunning && ns3Process) {
    ns3Process.kill('SIGTERM');
  }

  // Clear intervals
  if (statusInterval) {
    clearInterval(statusInterval);
  }

  // Notify all clients
  broadcast({
    type: "system",
    event: "server_shutdown",
    message: "Server is shutting down",
    timestamp: Date.now()
  });

  // Close all client connections
  clients.forEach((client, clientId) => {
    if (client.ws.readyState === WebSocket.OPEN) {
      client.ws.close(1000, "Server shutdown");
    }
  });

  // Wait a bit then exit
  setTimeout(() => {
    console.log("👋 Server shutdown complete");
    process.exit(0);
  }, 1000);
}

// HTTP Dashboard Server
const dashboardServer = http.createServer((req, res) => {
  if (req.url === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(`
      <!DOCTYPE html>
      <html>
      <head>
        <title>Enhanced MEMOSTP Simulation Dashboard</title>
        <style>
          body { font-family: Arial, sans-serif; margin: 20px; }
          .status { padding: 10px; margin: 10px 0; border-radius: 5px; }
          .running { background: #d4edda; }
          .stopped { background: #f8d7da; }
          .stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
          .stat-card { background: #f8f9fa; padding: 15px; border-radius: 5px; }
          .node-grid { display: grid; grid-template-columns: repeat(10, 30px); gap: 5px; margin: 20px 0; }
          .node { width: 30px; height: 30px; border-radius: 50%; display: flex; align-items: center; justify-content: center; color: white; font-size: 12px; }
        </style>
      </head>
      <body>
        <h1>Enhanced MEMOSTP Simulation Dashboard</h1>
        <div id="status" class="status"></div>
        <div id="stats" class="stats"></div>
        <div id="nodeGrid" class="node-grid"></div>
        <div>
          <button onclick="startSim()">Start Simulation</button>
          <button onclick="stopSim()">Stop Simulation</button>
          <button onclick="getStatus()">Refresh Status</button>
        </div>
        <script>
          const ws = new WebSocket('ws://localhost:${selectedPort}');
          
          ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            updateDashboard(data);
          };
          
          function updateDashboard(data) {
            // Update status
            if (data.type === 'status' || data.type === 'status_update') {
              document.getElementById('status').innerHTML = \`
                <h2>Simulation Status: \${data.simulationRunning ? 'RUNNING' : 'STOPPED'}</h2>
                <p>Events: \${data.eventCount} | Connected Clients: \${data.connectedClients}</p>
              \`;
              document.getElementById('status').className = \`status \${data.simulationRunning ? 'running' : 'stopped'}\`;
            }
            
            // Update stats
            if (data.stats) {
              const stats = data.stats;
              document.getElementById('stats').innerHTML = \`
                <div class="stat-card">
                  <h3>Nodes</h3>
                  <p>Alive: \${stats.nodes?.alive || 0}/\${stats.nodes?.total || 0}</p>
                  <p>Dead: \${stats.nodes?.dead || 0}</p>
                </div>
                <div class="stat-card">
                  <h3>Packets</h3>
                  <p>TX: \${stats.packets?.tx || 0}</p>
                  <p>RX: \${stats.packets?.rx || 0}</p>
                  <p>PDR: \${stats.pdr || 0}%</p>
                </div>
                <div class="stat-card">
                  <h3>Energy</h3>
                  <p>Total: \${stats.energy?.total?.toFixed(2) || 0} J</p>
                  <p>Efficiency: \${stats.energy?.efficiency || 0}</p>
                </div>
              \`;
            }
            
            // Update node grid
            if (data.nodeStates) {
              const grid = document.getElementById('nodeGrid');
              grid.innerHTML = '';
              data.nodeStates.forEach(node => {
                const nodeEl = document.createElement('div');
                nodeEl.className = 'node';
                nodeEl.style.backgroundColor = node.color || '#666';
                nodeEl.title = \`Node \${node.id}\\nEnergy: \${node.energy?.toFixed(1) || 0}%\`;
                nodeEl.textContent = node.id;
                grid.appendChild(nodeEl);
              });
            }
          }
          
          function startSim() {
            ws.send(JSON.stringify({ command: 'start_simulation' }));
          }
          
          function stopSim() {
            ws.send(JSON.stringify({ command: 'stop_simulation' }));
          }
          
          function getStatus() {
            ws.send(JSON.stringify({ command: 'get_status' }));
          }
          
          // Get initial status
          ws.onopen = () => {
            getStatus();
            ws.send(JSON.stringify({ command: 'get_node_states' }));
          };
        </script>
      </body>
      </html>
    `);
  } else if (req.url === '/health') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      status: 'ok',
      port: selectedPort,
      connectedClients: clients.size,
      simulationRunning: isSimulationRunning,
      eventCount: eventCount,
      uptime: process.uptime(),
      timestamp: Date.now()
    }));
  } else if (req.url === '/data') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(simulationData));
  } else {
    res.writeHead(404);
    res.end();
  }
});

// Start dashboard server
dashboardServer.listen(config.dashboardPort, () => {
  console.log(`📊 Dashboard available at http://localhost:${config.dashboardPort}/`);
});

// Health check server
const healthServer = http.createServer((req, res) => {
  if (req.url === '/health') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      status: 'ok',
      port: selectedPort,
      connectedClients: clients.size,
      simulationRunning: isSimulationRunning,
      eventCount: eventCount,
      uptime: process.uptime(),
      timestamp: Date.now()
    }));
  } else {github KarthikBytes
    res.writeHead(404);
    res.end();
  }
});

healthServer.listen(config.healthCheckPort, () => {
  console.log(`🏥 Health check server on http://localhost:${config.healthCheckPort}/health`);
});

// Handle process signals
process.on('SIGINT', shutdownServer);
process.on('SIGTERM', shutdownServer);

console.log(`🌐 Server ready on port ${selectedPort}`);
console.log(`📊 Data will be logged to: ${config.dataLogFile}`);
console.log("=".repeat(70));
console.log("📋 Available Features:");
console.log("  • Real-time node energy monitoring");
console.log("  • Node death/resilience simulation");
console.log("  • Adaptive parameter optimization");
console.log("  • Web dashboard visualization");
console.log("  • Performance reporting");
console.log("=".repeat(70));
console.log("⏳ Waiting for clients to connect...");
console.log(`📱 Open http://localhost:${config.dashboardPort} for dashboard`);
console.log("");
console.log("Available WebSocket commands:");
console.log("  • start_simulation - Start NS-3 simulation");
console.log("  • stop_simulation - Stop running simulation");
console.log("  • get_status - Get current simulation status");
console.log("  • get_stats - Get current statistics");
console.log("  • get_node_states - Get current node states");
console.log("  • subscribe - Subscribe to specific events");
console.log("=".repeat(70));