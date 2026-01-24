#ifndef NODE_MONITOR_H
#define NODE_MONITOR_H

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <iomanip>

class NodeMonitor {
public:
    struct NodeStatus {
        uint32_t nodeId;
        double initialEnergy;
        double remainingEnergy;
        bool isAlive;
        double deathTime;
        std::string deathCause;
        uint32_t packetsSent;
        uint32_t packetsReceived;
        double lastActivityTime;
        double jitterSum;
        uint32_t jitterCount;
        double positionX;
        double positionY;
    };
    
    NodeMonitor();
    
    void InitializeNodes(uint32_t nodeCount, double initialEnergy);
    void UpdateEnergy(uint32_t nodeId, double energyConsumed);
    void UpdatePacketCount(uint32_t nodeId, bool isSent);
    void CheckNodeDeath(uint32_t nodeId, double currentTime, const std::string& cause = "Energy Depletion");
    void RecordJitter(uint32_t nodeId, double jitter);
    void UpdatePosition(uint32_t nodeId, double x, double y);
    
    NodeStatus GetNodeStatus(uint32_t nodeId) const;
    std::vector<NodeStatus> GetAllNodeStatus() const;
    
    bool IsNodeAlive(uint32_t nodeId) const;
    double GetNodeRemainingEnergy(uint32_t nodeId) const;
    
    double GetNetworkLifetime() const;
    double GetAverageNodeLifetime() const;
    uint32_t GetAliveNodeCount() const;
    double GetNetworkCoverage() const;
    double GetFirstNodeDeathTime() const;
    double GetLastNodeDeathTime() const;
    
    void PrintNodeStatusTable() const;
    void PrintNetworkLifetimeMetrics() const;
    void ExportNodeData(const std::string& filename) const;
    
private:
    std::vector<NodeStatus> nodeStatuses;
    double networkStartTime;
    uint32_t totalNodes;
    double areaSize;
    
    double calculateCoverage() const;
};

#endif // NODE_MONITOR_H