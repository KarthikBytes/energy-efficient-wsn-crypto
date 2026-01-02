const WebSocket = require("ws");
const { exec } = require('child_process');

console.log("🚀 Starting NS-3 WebSocket Bridge Server");

// Try multiple ports
const PORTS = [8080, 8081, 8082, 8083, 8084];
let wss = null;
let selectedPort = null;

// Try to create server on available port
for (const port of PORTS) {
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

// Store connected clients
const clients = new Set();
let isSimulationRunning = false;
let eventCount = 0;

wss.on("connection", (ws) => {
  console.log("📱 New client connected");
  clients.add(ws);

  // Send welcome message
  ws.send(JSON.stringify({
    type: "system",
    message: "Connected to NS-3 Simulation Server",
    port: selectedPort,
    timestamp: Date.now()
  }));

  // If this is the first client and simulation isn't running, start it
  if (clients.size === 1 && !isSimulationRunning) {
    startSimulation();
  }

  ws.on("close", () => {
    console.log("📱 Client disconnected");
    clients.delete(ws);

    // If no clients left, consider stopping
    if (clients.size === 0) {
      console.log("⚠️  All clients disconnected");
    }
  });

  ws.on("error", (error) => {
    console.error("WebSocket client error:", error.message);
  });
});

function startSimulation() {
  console.log("🎬 Starting NS-3 simulation...");
  isSimulationRunning = true;
  eventCount = 0;

  // Notify all clients
  broadcast({
    type: "system",
    message: "Starting NS-3 MEMOSTP simulation...",
    timestamp: Date.now()
  });

  // Start NS-3 simulation
  const ns3Process = exec('cd .. && ./ns3 run protocol_cry 2>&1',
    (error, stdout, stderr) => {
      isSimulationRunning = false;

      if (error) {
        console.error(`❌ Simulation error: ${error.message}`);
        broadcast({
          type: "system",
          message: `Simulation error: ${error.message}`,
          timestamp: Date.now()
        });
        return;
      }

      console.log(`✅ Simulation completed. Total events: ${eventCount}`);
      broadcast({
        type: "system",
        message: `Simulation completed. Total events: ${eventCount}`,
        totalEvents: eventCount,
        timestamp: Date.now()
      });

      // Keep server running for 10 seconds
      setTimeout(() => {
        console.log("👋 Server shutting down");
        process.exit(0);
      }, 10000);
    }
  );

  // Process NS-3 output in real-time
  ns3Process.stdout.on('data', (data) => {
    const lines = data.toString().split('\n');
    lines.forEach(line => {
      processLine(line.trim());
    });
  });

  ns3Process.stderr.on('data', (data) => {
    console.error(`NS-3 Error: ${data.toString()}`);
  });
}

function processLine(line) {
  if (!line) return;

  try {
    // Check if it's JSON
    if (line.startsWith('{') && line.endsWith('}')) {
      const event = JSON.parse(line);
      eventCount++;

      // Add metadata
      event._id = eventCount;
      event._serverTime = Date.now();

      // Broadcast to all clients
      broadcast(event);

      // Log progress
      if (eventCount % 50 === 0) {
        console.log(`📨 Processed ${eventCount} events (latest: ${event.event})`);
      }
    } else {
      // Log non-JSON output
      if (line.includes("SIMULATION") || line.includes("ERROR") || line.includes("WARNING")) {
        console.log(`📝 ${line}`);

        // Send important messages to clients
        if (line.includes("SIMULATION COMPLETE") || line.includes("RESULT")) {
          broadcast({
            type: "system",
            message: line,
            timestamp: Date.now()
          });
        }
      }
    }
  } catch (error) {
    // Not JSON - ignore
  }
}

function broadcast(data) {
  const message = JSON.stringify(data);
  clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message);
    }
  });
}

console.log(`🌐 Server ready on port ${selectedPort}`);
console.log("⏳ Waiting for clients to connect...");
console.log("📱 Open dashboard.html in browser to begin");
