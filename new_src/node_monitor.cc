#include "node_monitor.h"
#include "event_emitter.h"
#include <fstream>
#include <algorithm>
#include <cmath>

NodeMonitor::NodeMonitor() : networkStartTime(0.0), totalNodes(0), areaSize(400.0) {}

void NodeMonitor::InitializeNodes(uint32_t nodeCount, double initialEnergy) {
    totalNodes = nodeCount;
    nodeStatuses.resize(nodeCount);
    
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodeStatuses[i] = {
            i,                     // nodeId
            initialEnergy,         // initialEnergy
            initialEnergy,         // remainingEnergy
            true,                  // isAlive
            -1.0,                  // deathTime
            "",                    // deathCause
            0,                     // packetsSent
            0,                     // packetsReceived
            0.0,                   // lastActivityTime
            0.0,                   // jitterSum
            0,                     // jitterCount
            0.0,                   // positionX
            0.0                    // positionY
        };
    }
    
    EventEmitter::Instance().EmitEvent("monitor_init", nodeCount);
}

void NodeMonitor::UpdateEnergy(uint32_t nodeId, double energyConsumed) {
    if (nodeId >= nodeStatuses.size()) return;
    
    nodeStatuses[nodeId].remainingEnergy -= energyConsumed;
    nodeStatuses[nodeId].remainingEnergy = std::max(0.0, nodeStatuses[nodeId].remainingEnergy);
    
    EventEmitter::Instance().EmitNodeEvent(nodeId, "energy_update", 
                                          nodeStatuses[nodeId].remainingEnergy);
}

void NodeMonitor::UpdatePacketCount(uint32_t nodeId, bool isSent) {
    if (nodeId >= nodeStatuses.size() || !nodeStatuses[nodeId].isAlive) return;
    
    if (isSent) {
        nodeStatuses[nodeId].packetsSent++;
    } else {
        nodeStatuses[nodeId].packetsReceived++;
    }
    
    nodeStatuses[nodeId].lastActivityTime = 
        EventEmitter::Instance().GetFirstNodeDeathTime(); // Using as timestamp proxy
}

void NodeMonitor::CheckNodeDeath(uint32_t nodeId, double currentTime, const std::string& cause) {
    if (nodeId >= nodeStatuses.size() || !nodeStatuses[nodeId].isAlive) return;
    
    if (nodeStatuses[nodeId].remainingEnergy <= 0.05) { // Threshold for death
        nodeStatuses[nodeId].isAlive = false;
        nodeStatuses[nodeId].deathTime = currentTime;
        nodeStatuses[nodeId].deathCause = cause;
        
        EventEmitter::Instance().LogNodeDeath(nodeId, currentTime, cause);
    }
}

void NodeMonitor::RecordJitter(uint32_t nodeId, double jitter) {
    if (nodeId >= nodeStatuses.size() || !nodeStatuses[nodeId].isAlive) return;
    
    nodeStatuses[nodeId].jitterSum += fabs(jitter);
    nodeStatuses[nodeId].jitterCount++;
}

void NodeMonitor::UpdatePosition(uint32_t nodeId, double x, double y) {
    if (nodeId >= nodeStatuses.size()) return;
    
    nodeStatuses[nodeId].positionX = x;
    nodeStatuses[nodeId].positionY = y;
}

NodeMonitor::NodeStatus NodeMonitor::GetNodeStatus(uint32_t nodeId) const {
    if (nodeId < nodeStatuses.size()) {
        return nodeStatuses[nodeId];
    }
    return NodeStatus();
}

std::vector<NodeMonitor::NodeStatus> NodeMonitor::GetAllNodeStatus() const {
    return nodeStatuses;
}

bool NodeMonitor::IsNodeAlive(uint32_t nodeId) const {
    return (nodeId < nodeStatuses.size()) ? nodeStatuses[nodeId].isAlive : false;
}

double NodeMonitor::GetNodeRemainingEnergy(uint32_t nodeId) const {
    return (nodeId < nodeStatuses.size()) ? nodeStatuses[nodeId].remainingEnergy : 0.0;
}

double NodeMonitor::GetNetworkLifetime() const {
    double lastDeath = GetLastNodeDeathTime();
    double firstDeath = GetFirstNodeDeathTime();
    
    if (firstDeath < 0) return 0.0; // No deaths yet
    return lastDeath - firstDeath;
}

double NodeMonitor::GetAverageNodeLifetime() const {
    double totalLifetime = 0.0;
    int deadNodes = 0;
    
    for (const auto& status : nodeStatuses) {
        if (!status.isAlive && status.deathTime > 0) {
            totalLifetime += status.deathTime;
            deadNodes++;
        }
    }
    
    return (deadNodes > 0) ? totalLifetime / deadNodes : 0.0;
}

uint32_t NodeMonitor::GetAliveNodeCount() const {
    uint32_t alive = 0;
    for (const auto& status : nodeStatuses) {
        if (status.isAlive) alive++;
    }
    return alive;
}

double NodeMonitor::calculateCoverage() const {
    // Simplified coverage calculation based on alive nodes
    uint32_t aliveCount = GetAliveNodeCount();
    if (aliveCount == 0) return 0.0;
    
    // Assuming nodes are evenly distributed
    double maxCoverage = areaSize * areaSize; // Total area
    double effectiveCoverage = (double)aliveCount / totalNodes * maxCoverage;
    
    return (effectiveCoverage / maxCoverage) * 100.0;
}

double NodeMonitor::GetNetworkCoverage() const {
    return calculateCoverage();
}

double NodeMonitor::GetFirstNodeDeathTime() const {
    double firstDeath = -1.0;
    for (const auto& status : nodeStatuses) {
        if (!status.isAlive && status.deathTime > 0) {
            if (firstDeath < 0 || status.deathTime < firstDeath) {
                firstDeath = status.deathTime;
            }
        }
    }
    return firstDeath;
}

double NodeMonitor::GetLastNodeDeathTime() const {
    double lastDeath = -1.0;
    for (const auto& status : nodeStatuses) {
        if (!status.isAlive && status.deathTime > 0) {
            if (status.deathTime > lastDeath) {
                lastDeath = status.deathTime;
            }
        }
    }
    return lastDeath;
}

void NodeMonitor::PrintNodeStatusTable() const {
    std::cout << "\n\033[1;36m📊 NODE STATUS TABLE\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(90, '=') << "\033[0m" << std::endl;
    std::cout << std::left << std::setw(6) << "Node" 
              << std::setw(8) << "Status" 
              << std::setw(10) << "Energy" 
              << std::setw(8) << "Sent" 
              << std::setw(10) << "Received"
              << std::setw(12) << "Death Time"
              << std::setw(15) << "Cause" << std::endl;
    std::cout << "\033[1;37m" << std::string(90, '-') << "\033[0m" << std::endl;
    
    for (const auto& status : nodeStatuses) {
        std::cout << std::left << std::setw(6) << status.nodeId;
        
        if (status.isAlive) {
            std::cout << "\033[32m" << std::setw(8) << "ALIVE" << "\033[0m";
        } else {
            std::cout << "\033[31m" << std::setw(8) << "DEAD" << "\033[0m";
        }
        
        std::cout << std::setw(10) << std::fixed << std::setprecision(2) << status.remainingEnergy
                  << std::setw(8) << status.packetsSent
                  << std::setw(10) << status.packetsReceived;
        
        if (status.deathTime > 0) {
            std::cout << std::setw(12) << std::fixed << std::setprecision(1) << status.deathTime
                      << std::setw(15) << status.deathCause.substr(0, 12);
        } else {
            std::cout << std::setw(12) << "N/A" 
                      << std::setw(15) << "N/A";
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "\033[1;37m" << std::string(90, '=') << "\033[0m" << std::endl;
    std::cout << "Alive Nodes: " << GetAliveNodeCount() << "/" << totalNodes 
              << " (" << std::fixed << std::setprecision(1) 
              << (double)GetAliveNodeCount()/totalNodes*100 << "%)" << std::endl;
}

void NodeMonitor::PrintNetworkLifetimeMetrics() const {
    double firstDeath = GetFirstNodeDeathTime();
    double lastDeath = GetLastNodeDeathTime();
    
    std::cout << "\n\033[1;33m📈 NETWORK LIFETIME METRICS:\033[0m" << std::endl;
    std::cout << "\033[1;37m" << std::string(50, '=') << "\033[0m" << std::endl;
    
    if (firstDeath < 0) {
        std::cout << "No node deaths recorded yet." << std::endl;
    } else {
        std::cout << "First Node Death:   " << std::fixed << std::setprecision(2) 
                  << firstDeath << "s" << std::endl;
        std::cout << "Last Node Death:    " << lastDeath << "s" << std::endl;
        std::cout << "Network Lifetime:   " << (lastDeath - firstDeath) << "s" << std::endl;
        std::cout << "Avg Node Lifetime:  " << std::fixed << std::setprecision(2) 
                  << GetAverageNodeLifetime() << "s" << std::endl;
        std::cout << "Network Coverage:   " << std::fixed << std::setprecision(1) 
                  << GetNetworkCoverage() << "%" << std::endl;
        std::cout << "Alive Nodes:        " << GetAliveNodeCount() << "/" << totalNodes 
                  << std::endl;
    }
    
    std::cout << "\033[1;37m" << std::string(50, '=') << "\033[0m" << std::endl;
}

void NodeMonitor::ExportNodeData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }
    
    file << "NodeID,Status,RemainingEnergy,PacketsSent,PacketsReceived,DeathTime,DeathCause,PositionX,PositionY\n";
    
    for (const auto& status : nodeStatuses) {
        file << status.nodeId << ","
             << (status.isAlive ? "ALIVE" : "DEAD") << ","
             << std::fixed << std::setprecision(3) << status.remainingEnergy << ","
             << status.packetsSent << ","
             << status.packetsReceived << ","
             << (status.deathTime > 0 ? std::to_string(status.deathTime) : "N/A") << ","
             << (status.deathCause.empty() ? "N/A" : status.deathCause) << ","
             << status.positionX << ","
             << status.positionY << "\n";
    }
    
    file.close();
    std::cout << "Node data exported to: " << filename << std::endl;
}