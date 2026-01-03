// server.js - Enhanced NS-3 WebSocket Bridge Server
const WebSocket = require("ws");
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

console.log("🚀 Starting Enhanced NS-3 WebSocket Bridge Server");
console.log("=".repeat(60));

// Configuration
const config = {
  ports: [8080, 8081, 8082, 8083, 8084],
  simulationCommand: 'cd .. && ./ns3 run protocol_cry 2>&1',
  simulationTimeout: 300000, // 5 minutes
  dataLogFile: 'simulation_data.json',
  keepServerRunning: true,
  maxClients: 10
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

// Store connected clients and simulation state
const clients = new Map(); // Map client id -> {ws, metadata}
let isSimulationRunning = false;
let simulationStartTime = null;
let eventCount = 0;
let simulationData = {
  startTime: null,
  endTime: null,
  totalEvents: 0,
  events: [],
  stats: {
    packets: { tx: 0, rx: 0, encrypted: 0, decrypted: 0 },
    nodes: 0,
    pdr: 0,
    energy: 0,
    throughput: 0
  }
};

// Generate unique client ID
function generateClientId() {
  return 'client_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
}

// Initialize data logging
function initDataLogging() {
  simulationData.startTime = Date.now();
  simulationData.events = [];

  // Clear old log file
  if (fs.existsSync(config.dataLogFile)) {
    fs.unlinkSync(config.dataLogFile);
  }

  console.log("📊 Data logging initialized");
}

// Save event to log file
function saveEventToLog(event) {
  simulationData.events.push(event);

  // Save to file periodically or when simulation ends
  if (eventCount % 100 === 0 || event.event === 'simulation_complete') {
    fs.writeFileSync(config.dataLogFile, JSON.stringify(simulationData, null, 2));
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
      simulationData.stats.nodes = event.from || 0;
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
    case 'stats_packets':
      simulationData.stats.packets.tx = event.from || 0;
      simulationData.stats.packets.rx = event.to || 0;
      break;
    case 'stats_pdr':
      simulationData.stats.pdr = event.from || 0;
      break;
    case 'stats_energy':
      simulationData.stats.energy = event.from || 0;
      break;
    case 'stats_throughput':
      simulationData.stats.throughput = event.from || 0;
      break;
  }

  // Calculate PDR if we have both tx and rx
  if (simulationData.stats.packets.tx > 0) {
    simulationData.stats.pdr = (simulationData.stats.packets.rx / simulationData.stats.packets.tx * 100).toFixed(2);
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
      type: 'visualization' // default type
    }
  });

  // Send welcome message with client info
  ws.send(JSON.stringify({
    type: "system",
    event: "client_connected",
    message: "Connected to NS-3 Simulation Server",
    clientId: clientId,
    serverPort: selectedPort,
    serverTime: Date.now(),
    simulationStatus: isSimulationRunning ? "running" : "idle",
    connectedClients: clients.size
  }));

  // If this is the first client and simulation isn't running, notify but don't start automatically
  if (clients.size === 1 && !isSimulationRunning) {
    console.log("⏳ First client connected. Waiting for start command...");
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

  console.log(`📨 Received from ${clientId}: ${data.command || 'message'}`);

  switch(data.command) {
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
      // Note: This would require more complex handling to stop NS-3 process
      sendToClient(clientId, {
        type: "system",
        event: "stop_simulation",
        message: "Simulation stop requested. Currently NS-3 must complete naturally.",
        timestamp: Date.now()
      });
      break;

    case 'get_status':
      sendToClient(clientId, {
        type: "status",
        simulationRunning: isSimulationRunning,
        simulationStartTime: simulationStartTime,
        eventCount: eventCount,
        connectedClients: clients.size,
        stats: simulationData.stats,
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

    case 'client_type':
      // Client identifies its type (visualization, control, monitoring)
      if (client) {
        client.metadata.type = data.clientType || 'visualization';
        console.log(`Client ${clientId} set type to: ${client.metadata.type}`);
      }
      break;

    case 'ping':
      sendToClient(clientId, {
        type: "pong",
        timestamp: Date.now(),
        serverTime: Date.now()
      });
      break;

    default:
      console.log(`Unknown command from ${clientId}:`, data.command);
  }
}

// Start simulation
function startSimulation(initiatorClientId = null) {
  console.log("🎬 Starting NS-3 simulation...");
  isSimulationRunning = true;
  simulationStartTime = Date.now();
  eventCount = 0;

  // Initialize data logging
  initDataLogging();

  // Notify all clients
  broadcast({
    type: "system",
    event: "simulation_starting",
    message: "Starting NS-3 MEMOSTP simulation...",
    initiator: initiatorClientId,
    startTime: simulationStartTime,
    timestamp: Date.now()
  });

  // Start NS-3 simulation with timeout
  const ns3Process = exec(config.simulationCommand, {
    timeout: config.simulationTimeout,
    maxBuffer: 1024 * 1024 * 10 // 10MB buffer
  });

  // Handle process completion
  ns3Process.on('exit', (code, signal) => {
    isSimulationRunning = false;
    simulationData.endTime = Date.now();
    simulationData.totalEvents = eventCount;

    console.log(`✅ Simulation process exited with code ${code}`);

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

    // Broadcast final stats to all clients
    broadcast({
      type: "stats_summary",
      stats: simulationData.stats,
      timestamp: Date.now()
    });

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
    if (errorLine && !errorLine.includes('Waf:')) { // Filter out Waf messages
      console.error(`NS-3 Error: ${errorLine}`);

      // Send important errors to clients
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
  const statusInterval = setInterval(() => {
    if (!isSimulationRunning) {
      clearInterval(statusInterval);
      return;
    }

    broadcast({
      type: "status_update",
      simulationRunning: isSimulationRunning,
      elapsedTime: Date.now() - simulationStartTime,
      eventCount: eventCount,
      stats: simulationData.stats,
      timestamp: Date.now()
    });
  }, 5000); // Every 5 seconds

  // Clean up interval when simulation ends
  ns3Process.on('exit', () => {
    clearInterval(statusInterval);
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

      // Send to specific client types if needed
      if (event.event === 'encrypt' || event.event === 'decrypt') {
        broadcastToType('monitoring', {
          type: "crypto_event",
          original: event,
          timestamp: Date.now()
        });
      }

      // Log progress
      if (eventCount % 100 === 0) {
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
          line.includes("ERROR") || line.includes("WARNING")) {
        console.log(`📝 ${line}`);

        // Send important messages to clients
        if (line.includes("SIMULATION COMPLETE") ||
            line.includes("ENHANCED MEMOSTP SIMULATION RESULTS")) {
          broadcast({
            type: "system",
            event: "simulation_output",
            message: line,
            timestamp: Date.now()
          });
        }

        // Parse performance metrics from console output
        if (line.includes("PDR:") || line.includes("Throughput:") || line.includes("Energy:")) {
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

  return Object.keys(metrics).length > 0 ? metrics : null;
}

// Handle server shutdown gracefully
function shutdownServer() {
  console.log("\n🛑 Server shutdown requested...");

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

// Handle process signals
process.on('SIGINT', shutdownServer);
process.on('SIGTERM', shutdownServer);

// Health check endpoint (optional - for monitoring)
const http = require('http');
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
  } else {
    res.writeHead(404);
    res.end();
  }
});

// Start health server on next available port
healthServer.listen(selectedPort + 1000, () => {
  console.log(`🏥 Health check server on http://localhost:${selectedPort + 1000}/health`);
});

console.log(`🌐 Server ready on port ${selectedPort}`);
console.log(`📊 Data will be logged to: ${config.dataLogFile}`);
console.log("=".repeat(60));
console.log("⏳ Waiting for clients to connect...");
console.log("📱 Open dashboard.html in browser to begin");
console.log("");
console.log("Available commands via WebSocket:");
console.log("  • start_simulation - Start NS-3 simulation");
console.log("  • get_status - Get current simulation status");
console.log("  • get_stats - Get current statistics");
console.log("  • ping - Test connection");
console.log("=".repeat(60));
